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
// Connect::run() is noreturn, so this process is ended from outside (Ctrl-C).

#include "rig_printer.hpp"

#include <connect/connect.hpp>
#include <connect/printer.hpp>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <cstring>
#include <string>
#include <string_view>

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
        "              client emits a counting-down filament_change_in\n");
    exit(2);
}

} // namespace

int main(int argc, char *argv[]) {
    const char *identity_path = nullptr;
    const char *host = "127.0.0.1";
    uint16_t port = 5052;
    bool printing = false;
    std::optional<uint32_t> swap_in = std::nullopt;

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
        } else if (arg == "--swap-in" && i + 1 < argc) {
            swap_in = static_cast<uint32_t>(atoi(argv[++i]));
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
        firmware.empty() ? "6.6.0" : firmware.c_str());

    if (printing) {
        printer.start_fake_print(1, 3600, swap_in);
    }

    static SharedBuffer buffer;
    static connect_client::Connect client(printer, buffer);

    // noreturn, like the firmware's own run().
    client.run();
}
