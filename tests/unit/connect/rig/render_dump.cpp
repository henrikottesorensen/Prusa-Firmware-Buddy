// connect_render_dump: runs the real Connect renderer over a fixed set of printer states and
// prints what it produces, as a JSON array on stdout.
//
// The renderer is the printer->server half of the protocol: it turns a Printer::Params plus an
// Action into the exact bytes a printer puts on the wire. tests/unit/connect/render.cpp asserts
// those bytes against string literals; this target instead *emits* them, so a server implementation
// on the other side can be tested against real renderer output rather than against a hand-copy of
// it, and can re-generate at a newer firmware revision by re-running a command.
//
// The scenarios below are the sections of render.cpp, reproduced setup-for-setup and keyed by that
// file's own section names (including its "Even -" typos, which are load-bearing as join keys).
// Only the `expected` strings are left out - they are what this target produces rather than
// asserts. Keep the two in step: a section added there should be added here.
//
// Time is deliberately the frozen clock (missing_functions_time.cpp, the one connect_tests links)
// rather than the rig's real one. Every duration the renderer emits - time_printing, time_remaining,
// time_transferring - is derived from it, so a real clock would make the output differ per run and
// turn a committed fixture into noise.

#include "mock_printer.h"

#include <render.hpp>
#include <transfers/monitor.hpp>

#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

using std::optional;
using transfers::Monitor;
using namespace connect_client;
using namespace json;

namespace {

constexpr const char *print_file = "/usb/box.gco";

/// render.cpp keeps this one in an anonymous namespace, so unlike params_idle()/params_dialog()
/// (which are inline in mock_printer.h) it cannot be shared and is reproduced here.
Printer::Params params_printing() {
    static SharedBuffer buffer;
    static optional<BorrowPaths> paths(*buffer.borrow());
    strcpy(paths->path(), print_file);
    strcpy(paths->name(), "box.gcode");
    Printer::Params params(paths);

    params.job_id = 42;
    params.has_job = true;
    params.progress_percent = 12;
    params.temp_bed = 65;
    params.slots[0].temp_nozzle = 200;
    params.target_bed = 70;
    params.target_nozzle = 195;
    params.state = printer_state::DeviceState::Printing;

    return params;
}

/// The locals a render.cpp SECTION sets before falling into the shared render block. The transfer
/// slot is held here because it must outlive the render call: releasing it would take the transfer
/// back out of the Monitor the renderer reads.
struct Setup {
    optional<Printer::Params> params;
    Action action;
    optional<Monitor::Slot> transfer_slot;
    optional<CommandId> background_command_id;
};

/// render.cpp's shared trailing block, minus the assertion.
std::string render(Setup &setup) {
    MockPrinter printer(setup.params.value());
    RenderState state(printer, setup.action, setup.background_command_id);
    Renderer renderer(std::move(state));
    uint8_t buffer[1024];
    const auto [result, amount] = renderer.render(buffer, sizeof buffer);

    if (result != JsonResult::Complete) {
        // Incomplete means the 1024-byte buffer did not hold the document - the fixture would be a
        // truncated prefix, which is worse than no fixture at all.
        fprintf(stderr, "render_dump: renderer did not complete\n");
        exit(1);
    }

    return std::string(reinterpret_cast<const char *>(buffer), amount);
}

Monitor::Slot allocate_transfer(const char *path) {
    optional<Monitor::Slot> slot = Monitor::instance.allocate(Monitor::Type::Connect, path, 1024);

    if (!slot.has_value()) {
        fprintf(stderr, "render_dump: could not allocate a transfer slot\n");
        exit(1);
    }

    return std::move(*slot);
}

struct Scenario {
    const char *name;
    void (*setup)(Setup &);
};

const Scenario scenarios[] = {
    { "Telemetry - reduced", [](Setup &s) {
         s.params.emplace(params_printing());
         s.action = SendTelemetry { SendTelemetry::Mode::Reduced };
     } },

    { "Telemetry - printing", [](Setup &s) {
         s.params.emplace(params_printing());
         s.action = SendTelemetry { SendTelemetry::Mode::Full };
     } },

    { "Telemetry - idle", [](Setup &s) {
         s.params.emplace(params_idle());
         s.action = SendTelemetry { SendTelemetry::Mode::Full };
     } },

    { "Telemetry - transferring", [](Setup &s) {
         s.params.emplace(params_idle());
         s.action = SendTelemetry { SendTelemetry::Mode::Full };
         s.transfer_slot = allocate_transfer("/usb/whatever.gcode");
     } },

    { "Telemetry with background command", [](Setup &s) {
         s.params.emplace(params_idle());
         s.action = SendTelemetry { SendTelemetry::Mode::Full };
         s.background_command_id = 13;
     } },

    { "Event - rejected", [](Setup &s) {
         s.action = Event {
             EventType::Rejected,
             11,
         };
         s.params.emplace(params_idle());
     } },

    { "Event - job info", [](Setup &s) {
         s.action = Event {
             EventType::JobInfo,
             11,
             42,
         };
         s.params.emplace(params_printing());
     } },

    { "Even - job info not printing", [](Setup &s) {
         s.action = Event {
             EventType::JobInfo,
             11,
             42,
         };
         s.params.emplace(params_idle());
     } },

    { "Even - job info - invalid job ID", [](Setup &s) {
         s.action = Event {
             EventType::JobInfo,
             11,
             13,
         };
         s.params.emplace(params_printing());
     } },

    { "Even - job info - old job ID FINISHED", [](Setup &s) {
         s.action = Event {
             EventType::JobInfo,
             11,
             41,
         };
         s.params.emplace(params_printing());
     } },

    { "Even - job info - old job ID ABORTED", [](Setup &s) {
         s.action = Event {
             EventType::JobInfo,
             11,
             40,
         };
         s.params.emplace(params_idle());
     } },

    { "Event - info", [](Setup &s) {
         s.action = Event {
             EventType::Info,
             11,
         };
         s.params.emplace(params_idle());
     } },

    { "Event - info - multi", [](Setup &s) {
         s.action = Event {
             EventType::Info,
             11,
         };
         auto idle = params_idle();
         // Enable slot 1 and 3
         idle.slot_mask = 5;
         idle.slots[2] = Printer::SlotInfo {
             .material = { "PETG" },
             .hardened = true,
             .nozzle_diameter = 0.6f,
         };
         s.params.emplace(idle);
     } },

    { "Event - transfer info, no transfer", [](Setup &s) {
         s.action = Event {
             EventType::TransferInfo,
             11,
         };
         s.params.emplace(params_idle());
     } },

    { "Event - transfer info", [](Setup &s) {
         s.action = Event {
             EventType::TransferInfo,
             11,
         };
         s.params.emplace(params_idle());
         s.transfer_slot = allocate_transfer("/usb/whatever.gcode");
     } },

    { "Event - rejected with transfer", [](Setup &s) {
         s.action = Event {
             EventType::Rejected,
             11,
         };
         s.params.emplace(params_idle());
         s.transfer_slot = allocate_transfer("/usb/whatever.gcode");
     } },

    // If nullptr is passed instead of a path, the resulting response should omit the path param.
    { "Event - transfer info no upload path", [](Setup &s) {
         s.action = Event {
             EventType::TransferInfo,
             11,
         };
         s.params.emplace(params_idle());
         s.transfer_slot = allocate_transfer(nullptr);
     } },

    { "Event - state changed with dialog", [](Setup &s) {
         s.action = Event {
             EventType::StateChanged,
             11,
         };
         s.params.emplace(params_dialog());
     } },
};

} // namespace

int main() {
    printf("[\n");

    bool first = true;

    for (const Scenario &scenario : scenarios) {
        // Fresh per scenario, so a transfer slot is released before the next one allocates - the
        // Monitor holds a single slot, and its transfer id advances each time.
        Setup setup;
        scenario.setup(setup);

        const std::string output = render(setup);

        if (!first) {
            printf(",\n");
        }

        first = false;

        // The scenario names are literals from render.cpp with nothing needing JSON escaping in
        // them; the rendered output is already a JSON document, so it is embedded verbatim rather
        // than as an escaped string. That keeps the fixture byte-exact and still readable as JSON.
        printf("  { \"scenario\": \"%s\", \"output\": %s }", scenario.name, output.c_str());
    }

    printf("\n]\n");

    return 0;
}
