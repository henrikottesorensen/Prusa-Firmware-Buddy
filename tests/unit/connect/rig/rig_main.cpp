// connect_rig: the real Buddy Connect client, driven against a Connect server on this host.
//
// Deliberately the same four-line shape as the firmware's own entry point (src/connect/run.cpp):
// a SharedBuffer, a Printer, a Connect over both, then run(). The only substitution is RigPrinter
// where MarlinPrinter sits on hardware - everything above it (planner, render, command parsing,
// websocket framing, the transfer engine) is the shipped firmware code, unmodified.
//
// Reads a small JSON identity file (Fingerprint, Token, SerialNumber, Firmware) so that one
// enrollment can be shared with other test clients.
//
// Gcode lines can also be fed in on STDIN, which is how the printer is driven when the server has no
// way to send a particular one. They take exactly the path a server's gcode takes - the same
// submit_gcode() - so the two cannot behave differently.
//
// Connect::run() is noreturn, so this process is ended from outside (Ctrl-C).

#include "rig_printer.hpp"

#include <connect/connect.hpp>
#include <connect/printer.hpp>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {

/// Pulls one string value out of the identity JSON. Deliberately not a JSON parser: the file has a
/// known, flat shape, and dragging a parser into the rig's link would be gratuitous.
std::string json_field(const std::string &json, const char *key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t at = json.find(needle);

    if (at == std::string::npos) {
        return {};
    }

    size_t colon = json.find(':', at + needle.size());

    if (colon == std::string::npos) {
        return {};
    }

    size_t open = json.find('"', colon);

    if (open == std::string::npos) {
        return {};
    }

    size_t close = json.find('"', open + 1);

    if (close == std::string::npos) {
        return {};
    }

    return json.substr(open + 1, close - open - 1);
}

std::string read_file(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == nullptr) {
        fprintf(stderr, "rig: cannot open %s\n", path);
        exit(1);
    }

    std::string content;
    char buffer[4096];
    size_t read = 0;

    while ((read = fread(buffer, 1, sizeof buffer, file)) > 0) {
        content.append(buffer, read);
    }

    fclose(file);

    return content;
}

[[noreturn]] void usage() {
    fprintf(stderr,
        "usage: connect_rig --identity <identity.json> --host <host> --port <port> [--printing]\n"
        "\n"
        "  --identity  JSON holding an enrolled printer's credentials: Fingerprint, Token,\n"
        "              SerialNumber, Firmware\n"
        "  --printing  start out mid-job, so telemetry carries the job block and Pause round-trips\n"
        "  --swap-in <seconds>  schedule one filament change (M600) that far into the job, so the\n"
        "              client emits a counting-down filament_change_in\n"
        "  --tools <spec>  which tools exist and what they hold, as `1:PLA,2:PETG,3:-,5:ASA`.\n"
        "              Numbers are the ones the WIRE uses - 1-based - not gcode T numbers, which\n"
        "              are 0-based and one lower. `-` is a tool holding nothing. Tools left out do\n"
        "              not exist, so occupancy can be non-contiguous, as a real toolchanger's may\n"
        "              be. The ceiling is the build's, not the printer's - see the startup line.\n"
        "  --active <n>|none  which tool is picked. Default none, which is what a toolchanger\n"
        "              reports at power-on and what makes a gcode carrying no T do nothing at all.\n"
        "  --filament-seconds <n>  how long a load or unload takes (default 5). A real one runs for\n"
        "              minutes; this is long enough for the in-progress state to be observable.\n"
        "\n"
        "Gcode on STDIN is submitted as a server's would be, one line at a time. T<n> picks a tool\n"
        "(0-based, so T0 is tool 1), M701 S\"PLA\" T<n> loads, M702 T<n> W0 unloads. With no T they\n"
        "act on whatever is picked - and do nothing at all when that is nothing.\n");
    exit(2);
}

/// Applies a --tools spec, exiting with a diagnostic if it does not describe a printer this build
/// can be. Called before anything observes the printer, because a tool set is only legal once whole.
void apply_tools(connect_client::RigPrinter &printer, std::string_view spec) {
    printer.clear_tools();

    while (!spec.empty()) {
        const size_t comma = spec.find(',');
        std::string_view entry = spec.substr(0, comma);

        spec = comma == std::string_view::npos ? std::string_view {} : spec.substr(comma + 1);

        const size_t colon = entry.find(':');

        if (colon == std::string_view::npos) {
            fprintf(stderr, "rig: --tools entry '%.*s' is not <number>:<material>\n",
                static_cast<int>(entry.size()), entry.data());
            exit(2);
        }

        const std::string number { entry.substr(0, colon) };
        const std::string material { entry.substr(colon + 1) };

        // `-` and an empty material both mean a tool that exists and holds nothing.
        const char *held = (material.empty() || material == "-") ? nullptr : material.c_str();

        if (const char *error = printer.set_tool(atoi(number.c_str()), held); error != nullptr) {
            fprintf(stderr, "rig: --tools entry '%.*s': %s\n",
                static_cast<int>(entry.size()), entry.data(), error);
            exit(2);
        }
    }

    if (!printer.has_any_tool()) {
        fprintf(stderr, "rig: --tools names no tools; a printer has at least one\n");
        exit(2);
    }
}

/// Feeds stdin to the printer a line at a time. Detached and never joined: Connect::run() owns the
/// main thread and does not return, so there is nothing to join back to.
void read_gcode_from_stdin(connect_client::RigPrinter &printer) {
    std::thread([&printer] {
        std::string line;

        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            printer.submit_gcode(line.c_str());
            fflush(stdout);
        }
    }).detach();
}

} // namespace

int main(int argc, char *argv[]) {
    const char *identity_path = nullptr;
    const char *host = "127.0.0.1";
    uint16_t port = 5052;
    bool printing = false;
    bool tls = false;
    bool custom_cert = false;
    std::optional<uint32_t> swap_in = std::nullopt;
    const char *tools = nullptr;
    const char *active = nullptr;
    uint32_t filament_seconds = 5;

    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];

        if (arg == "--identity" && i + 1 < argc) {
            identity_path = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(atoi(argv[++i]));
        } else if (arg == "--printing") {
            printing = true;
        } else if (arg == "--tls") {
            tls = true;
        } else if (arg == "--custom-cert") {
            // Implies --tls: verify against the DER on disk rather than the compiled-in Prusa CA.
            tls = true;
            custom_cert = true;
        } else if (arg == "--swap-in" && i + 1 < argc) {
            swap_in = static_cast<uint32_t>(atoi(argv[++i]));
        } else if (arg == "--tools" && i + 1 < argc) {
            tools = argv[++i];
        } else if (arg == "--active" && i + 1 < argc) {
            active = argv[++i];
        } else if (arg == "--filament-seconds" && i + 1 < argc) {
            filament_seconds = static_cast<uint32_t>(atoi(argv[++i]));
        } else {
            usage();
        }
    }

    if (identity_path == nullptr) {
        usage();
    }

    std::string identity = read_file(identity_path);
    std::string fingerprint = json_field(identity, "Fingerprint");
    std::string token = json_field(identity, "Token");
    std::string serial = json_field(identity, "SerialNumber");
    std::string firmware = json_field(identity, "Firmware");

    if (fingerprint.empty() || token.empty()) {
        fprintf(stderr, "rig: %s carries no Fingerprint/Token - enroll the printer first\n",
            identity_path);

        return 1;
    }

    // The firmware truncates the fingerprint to 16 characters for headers on its own
    // (FINGERPRINT_HDR_SIZE, connect.cpp) - the full 50 goes in, exactly as on hardware.
    printf("rig: connecting to %s:%u as %.16s... (token %zu chars, %s)\n",
        host, port, fingerprint.c_str(), token.size(), printing ? "printing" : "idle");
    fflush(stdout);

    static connect_client::RigPrinter printer(host, port, token.c_str(), fingerprint.c_str(),
        serial.empty() ? "RIG-0000000000000000" : serial.c_str(),
        firmware.empty() ? "6.6.0" : firmware.c_str(), tls, custom_cert);

    // What this binary IS, which no flag can change - the printer type and the tool ceiling are both
    // compiled in. Said out loud because the ceiling is the representation's rather than the
    // printer's: an XL build accepts eight tools although a real XL has five, so the line a reader
    // should believe about head count is --tools, not this one.
    const PrinterVersion version = printer.version();

    printf("rig: built as printer_type %hhu.%hhu.%hhu; can represent up to %u tools, and --tools says how many exist\n",
        version.type, version.version, version.subversion,
        connect_client::RigPrinter::tool_capacity());

    printer.set_filament_change_duration(filament_seconds);

    // Before --active, which refuses a tool the set does not contain, and before the print, which
    // reads the preferred slot.
    if (tools != nullptr) {
        apply_tools(printer, tools);
    }

    if (active != nullptr) {
        const std::string_view choice = active;
        const char *error = choice == "none"
            ? printer.set_active_slot(std::nullopt)
            : printer.set_active_slot(static_cast<unsigned>(atoi(active)));

        if (error != nullptr) {
            fprintf(stderr, "rig: --active %s: %s\n", active, error);

            return 2;
        }
    }

    if (printing) {
        printer.start_fake_print(1, 3600, swap_in);
    }

    read_gcode_from_stdin(printer);

    static SharedBuffer buffer;
    static connect_client::Connect client(printer, buffer);

    // noreturn, like the firmware's own run().
    client.run();
}
