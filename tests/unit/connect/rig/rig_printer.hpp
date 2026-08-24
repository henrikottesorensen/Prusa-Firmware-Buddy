#pragma once

// The Printer implementation the connect rig runs the real Connect client against - the seam that
// MarlinPrinter occupies on hardware (see src/connect/run.cpp). Structurally a copy of
// tests/unit/connect/mock_printer.h, with four deliberate differences:
//
//   1. load_config() returns a real, enrolled config (host/port/token, TLS off). That is the
//      USB-provisioning shape: a printer that already holds a token skips /p/register entirely
//      (marlin_printer.cpp's connect_ini_handler does the same on hardware).
//   2. params() reports live state from this object, so telemetry actually changes over time.
//   3. job_control()/set_ready()/set_idle() reproduce MarlinPrinter's real state gates
//      (marlin_printer.cpp:442, :574-586), so a Pause/Stop round-trip answers FINISHED or REJECTED
//      for the same reasons hardware would.
//   4. submit_gcode() interprets the tool-change and filament gcodes, so a multi-tool build answers
//      T/M701/M702 the way the machine it was built as would. See "Filament gcode" below.
//
// ## Multi-tool
//
// The CEILING on how many tools may exist is a property of the BUILD, not of this class:
// Params::slots is sized by VirtualToolIndex::count, which comes from EXTRUDERS - and in this tree
// that is tests/stubs/inc/MarlinConfigPre.h's `EXTRUDERS 9`, shadowing the real Configuration_*.h.
// Eight tools, which is the widest `slot_mask` can represent and therefore the most demanding case,
// not any one printer's head count. -DPRINTER does not move it.
//
// So the ceiling is not a claim about the machine: an XL build will happily accept eight tools even
// though a real XL has five. --tools decides how many actually exist, and that is where a realistic
// printer is described.
//
// What -DPRINTER *does* change is which machine this claims to be, and that is not cosmetic:
// get_printer_version() decides `printer_type` on the wire, and HAS_INDX() decides whether a
// filament gcode leaves a tool picked. See "Filament gcode" below.
//
// ## Filament gcode
//
// The point of interpreting it here is to be WRONG in the same places a real machine is. In
// particular a tool-resolution failure is not reported to Connect at all: firmware answers the gcode
// when it is QUEUED, and the failure happens later inside the gcode, where report_error() writes to
// the serial console (gcode.cpp:126-129). So submit_gcode() answers Submitted and then does nothing,
// which is the confident-success-for-a-no-op this rig exists to let a server be tested against.
//
// Reproduced from firmware, all at the ref this checkout is on:
//
//   - `T` is a GCODE tool number, 0-based, while the wire's slot keys are 1-based (render.cpp:235).
//     Slot 1 on the wire is `T0` here.
//   - `T` equal to the tool count means NoTool (gcode.cpp:104-105/114-115); above it is a parsing
//     error. Firmware spells the rule per printer - "T5 will park all tools" on a five-tool XL
//     (Configuration_XL.h:139) - so here it is whatever VirtualToolIndex::count is, `T8` by default.
//   - No `T` at all resolves to VirtualToolIndex::currently_selected(), which may legally be NoTool
//     (gcode.cpp:125).
//   - A tool that is not enabled is refused the same way an absent one is (gcode.cpp:133-138).
//   - M701 and M702 PICK the target tool (M701_2.cpp:103, :169). Whether it STAYS picked is the one
//     place these builds genuinely diverge: INDX docks back to NoTool when the gcode finishes
//     (`#if HAS_INDX()`, :127-131 and :185-188) and an XL does not. So on an XL a second unload
//     targets the same tool again, while on an INDX it finds nothing picked and does nothing at all -
//     which is the resting state there, not an edge case.
//   - `M701 R` restarts a paused print (:133), on every build - the dock above is the only half of
//     that flag INDX gates. Reach it with --paused.
//
//   - `M9933 C<cookie>` is Connect's own cork marker rather than a printer feature, and it MUST be
//     answered or the client wedges: the planner holds its background command until the cookie comes
//     back done, and every command arriving meanwhile is refused with no reason at all
//     (planner.cpp:1093-1098, the one rejection site that sets none). Marked when this rig's queue
//     drains, which is where Marlin would mark it - so an unload occupies the client for its whole
//     duration, as it does on hardware.
//
// Not reproduced, and deliberately: this is not GCodeParser2. It reads exactly the words the rig
// needs and ignores every other parameter, so it cannot stand in for firmware's parser. Anything
// beyond T/M701/M702/M9933 is accepted and discarded, as an unknown gcode would be.

#include <connect/printer.hpp>
#include <general_response.hpp>
#include <marlin_server_shared.h>
#include <timing.h>
#include <connect/printer_type.hpp>
#include <option/has_indx.h>
#include <feature/cork/tracker.hpp>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string_view>

extern "C" size_t strlcpy(char *, const char *, size_t);

namespace connect_client {

class RigPrinter final : public Printer {
public:
    /// How hot the nozzle goes while filament moves. A real printer takes this from the filament's
    /// own parameters; the rig has no filament table (see xl_hardware_mock.cpp) and one number is
    /// enough for telemetry to show a heater doing something.
    static constexpr int16_t filament_change_nozzle_temp = 215;

    RigPrinter(const char *host, uint16_t port, const char *token, const char *fingerprint,
               const char *serial, const char *firmware, bool tls = false, bool custom_cert = false) {
        info.appendix = false;
        info.firmware_version = firmware;
        strlcpy(info.fingerprint, fingerprint, PrinterInfo::FINGERPRINT_BUFF_LEN);
        strlcpy(info.serial_number.begin(), serial, PrinterInfo::SER_NUM_BUFR_LEN);

        strlcpy(config.host, host, Config::CONNECT_URL_BUF_LEN);
        strlcpy(config.token, token, Config::CONNECT_TOKEN_BUF_LEN);
        config.port = port;
        config.tls = tls;
        config.custom_cert = custom_cert;
        config.enabled = true;
        config.loaded = true;

        state.state = printer_state::DeviceState::Idle;
        // The printer this binary was built as, rather than a constant. render.cpp:341 sends it as
        // `printer_type`, so a server sees an XL only when the build is one.
        state.version = get_printer_version();
        // One tool, holding nothing. A multi-tool rig overwrites this through set_tool().
        state.slot_mask = 1;
        state.slots[0] = SlotInfo {
            .nozzle_diameter = 0.4f,
        };
        state.slots[0].temp_nozzle = 25;
        state.temp_bed = 24;
        state.target_bed = 0;
        state.target_nozzle = 0;
        // Nothing picked, which is what a toolchanger reports at power-on and what render.cpp:126-129
        // sends as `active: 0`. On a single-tool build the slot block is not emitted at all
        // (render.cpp:230, gated on enabled_tool_cnt() > 1), so this is invisible there.
        state.active_slot = NoTool {};
    }

    /// Declares one tool and what it holds.
    ///
    /// <paramref name="wire_slot"/> is the number the WIRE uses - 1-based, as render.cpp emits it and
    /// as a server displays it - not the 0-based number gcode `T` takes. <paramref name="material"/>
    /// empty or null means the tool exists and holds nothing.
    ///
    /// Only the tools named here are enabled, so occupancy can be left non-contiguous ("1,2,5"),
    /// which is what a real toolchanger is allowed to report (printer.hpp:128-129).
    ///
    /// @returns an error message, or nullptr on success.
    const char *set_tool(unsigned wire_slot, const char *material) {
        if (wire_slot < 1 || wire_slot > VirtualToolIndex::count) {
            return "tool number out of range - the build's ceiling is VirtualToolIndex::count";
        }

        if (material != nullptr && strlen(material) >= FilamentTypeParameters::Name::capacity()) {
            return "material name too long";
        }

        const std::lock_guard guard(mutex);
        const uint8_t raw = static_cast<uint8_t>(wire_slot - 1);

        state.slot_mask |= static_cast<uint8_t>(1 << raw);
        state.slots[raw] = SlotInfo {
            .material = material == nullptr ? FilamentTypeParameters::Name {} : FilamentTypeParameters::Name { material },
            .nozzle_diameter = 0.4f,
        };
        state.slots[raw].temp_nozzle = 25;

        return nullptr;
    }

    /// Drops the default single tool, so that set_tool() builds the whole set rather than adding to
    /// it. Separate from set_tool() because a mask of zero is not a legal state to be left in -
    /// preferred_slot() has no answer for it - so nothing may observe the printer in between.
    void clear_tools() {
        const std::lock_guard guard(mutex);

        state.slot_mask = 0;
    }

    /// Picks a tool, or nothing. <paramref name="wire_slot"/> is 1-based as above; nullopt is NoTool.
    /// @returns an error message, or nullptr on success.
    const char *set_active_slot(std::optional<unsigned> wire_slot) {
        const std::lock_guard guard(mutex);

        if (!wire_slot.has_value()) {
            state.active_slot = NoTool {};

            return nullptr;
        }

        if (*wire_slot < 1 || *wire_slot > VirtualToolIndex::count) {
            return "tool number out of range for this build";
        }

        const uint8_t raw = static_cast<uint8_t>(*wire_slot - 1);

        if ((state.slot_mask & (1 << raw)) == 0) {
            return "no such tool on this printer";
        }

        state.active_slot = VirtualToolIndex::from_raw(raw);

        return nullptr;
    }

    /// How long a load or an unload pretends to take. A real one runs for minutes; the default is
    /// short enough to watch and long enough that the in-progress state is observable.
    void set_filament_change_duration(uint32_t seconds) {
        const std::lock_guard guard(mutex);

        filament_change_duration_s = seconds;
    }

    /// The most tools this BUILD can represent, which is neither how many are enabled nor how many
    /// the real printer has. Compiled in via EXTRUDERS, so it is the ceiling --tools may name.
    static constexpr unsigned tool_capacity() {
        return VirtualToolIndex::count;
    }

    /// The printer this build reports itself as, for saying so at startup.
    PrinterVersion version() const {
        const std::lock_guard guard(mutex);

        return state.version;
    }

    /// @returns false if no tool is enabled, which preferred_slot() has no answer for.
    bool has_any_tool() const {
        const std::lock_guard guard(mutex);

        return state.slot_mask != 0;
    }

    /// Starts a simulated job. <paramref name="duration_s"/> is how long it pretends to take, and
    /// <paramref name="filament_change_in_s"/> optionally schedules one pause (an M600 colour swap) that
    /// many seconds in - which is what makes the firmware emit `filament_change_in` at all.
    void start_fake_print(uint16_t job_id, uint32_t duration_s = 3600,
                          std::optional<uint32_t> filament_change_in_s = std::nullopt,
                          bool paused = false) {
        const std::lock_guard guard(mutex);

        state.job_id = job_id;
        state.has_job = true;
        // Paused is a job that exists and is not running, which is the only state `M701 R` does
        // anything in. Reaching it otherwise needs a PAUSE_PRINT from the server.
        state.state = paused ? printer_state::DeviceState::Paused : printer_state::DeviceState::Printing;
        state.target_nozzle = filament_change_nozzle_temp;
        // The tool the flat temperature fields describe. On a multi-tool printer that is one head's
        // reading presented as the machine's, which is what preferred_slot() means (printer.cpp:214).
        state.slots[state.preferred_slot()].temp_nozzle = filament_change_nozzle_temp;
        state.target_bed = 60;
        state.temp_bed = 60;

        print_start_ms = ticks_ms();
        print_duration_s = duration_s;
        pause_at_s = filament_change_in_s;
    }

    virtual void renew(std::optional<SharedBuffer::Borrow>) override {}

    virtual void drop_paths() override {}

    virtual Config load_config() override {
        return config;
    }

    /// <summary>
    /// The stored state with the job counters recomputed from the clock.
    /// </summary>
    /// <remarks>
    /// Derived here rather than advanced by a ticking caller, because there is nowhere to call one
    /// from: `Connect::run()` is noreturn and owns the thread (connect.cpp:877). A background thread
    /// would race this method's copy of `state` for no benefit, and hanging it off `renew()` would tie
    /// progression to loop cadence instead of real time.
    ///
    /// It also matters for fidelity, not just realism. The planner picks Full over Reduced telemetry
    /// when `telemetry_fingerprint()` changes (planner.cpp:539-546); with frozen counters nothing ever
    /// changes, so the rig would only ever send Full on the 5-minute forced refresh and would never
    /// exercise the change-driven path a real printer uses constantly.
    ///
    /// A pending filament change is settled rather than derived, because unlike the job counters it
    /// is a step and not a curve, and because submit_gcode() has to see the settled state to decide
    /// what a second command does. `state` is therefore mutable and the lock is taken even here: the
    /// rig's stdin channel writes it from a second thread, which the job counters never had to
    /// contend with.
    /// </remarks>
    virtual Params params() const override {
        const std::lock_guard guard(mutex);

        settle_filament_change();

        Params derived = state;

        if (derived.state.device_state != printer_state::DeviceState::Printing) {
            return derived;
        }

        const uint32_t elapsed_s = (ticks_ms() - print_start_ms) / 1000;

        derived.print_duration = elapsed_s;
        derived.time_to_end = elapsed_s >= print_duration_s ? 0 : print_duration_s - elapsed_s;
        derived.progress_percent = print_duration_s == 0
            ? 100
            : static_cast<uint8_t>(std::min<uint32_t>(100, elapsed_s * 100 / print_duration_s));

        // Only rendered while valid, so the sentinel is how a print with no scheduled swap omits the
        // field entirely rather than sending zero (render.cpp:164).
        derived.time_to_pause = marlin_server::TIME_TO_END_INVALID;

        if (pause_at_s.has_value() && elapsed_s < *pause_at_s) {
            derived.time_to_pause = *pause_at_s - elapsed_s;
        }

        return derived;
    }

    virtual std::optional<NetInfo> net_info(Iface) const override {
        return std::nullopt;
    }

    virtual NetCreds net_creds() const override {
        return {};
    }

    // MarlinPrinter::job_control, marlin_printer.cpp:442 - the state gates decide FINISHED vs
    // REJECTED, and the reason strings live in planner.cpp's JC macro.
    virtual bool job_control(JobControl control) override {
        const std::lock_guard guard(mutex);

        switch (control) {
        case JobControl::Pause:
            if (state.state.device_state == printer_state::DeviceState::Printing) {
                state.state = printer_state::DeviceState::Paused;

                return true;
            }

            return false;

        case JobControl::Resume:
            if (state.state.device_state == printer_state::DeviceState::Paused) {
                state.state = printer_state::DeviceState::Printing;

                return true;
            }

            return false;

        case JobControl::Stop:
            if (state.state.device_state == printer_state::DeviceState::Paused
                || state.state.device_state == printer_state::DeviceState::Printing
                || state.state.device_state == printer_state::DeviceState::Attention) {
                state.state = printer_state::DeviceState::Stopped;
                state.job_id = 0;
                state.has_job = false;

                return true;
            }

            return false;
        }

        return false;
    }

    virtual bool is_valid_file_or_transfer(const char *) const override {
        return false;
    }

    virtual StartPrintResult start_print(const char *, const std::optional<ToolMapping> &) override {
        return std::unexpected("Rig does not print");
    }

    virtual const char *delete_file(const char *) override {
        return "Rig has no files";
    }

    /// <summary>
    /// Queues a gcode line, acting on the tool-change and filament ones. See "Filament gcode" at the
    /// top of this file for what is reproduced and what is not.
    /// </summary>
    /// <remarks>
    /// Always answers Submitted, including for a line it does nothing with. That is not laziness: on
    /// hardware this method's answer only says the line reached the queue, and every failure below -
    /// no tool picked, a tool that is not enabled, a `T` out of range - happens later, inside the
    /// gcode, and is reported to the serial console where Connect cannot see it. Returning Failed
    /// here would make the rig kinder than the machine it stands in for, and hide the one defect a
    /// server most needs to be tested against.
    ///
    /// The rig's own stdout is a different matter: it says what it did, because a host binary's
    /// terminal is an instrument and not part of the wire. Nothing a server observes changes.
    /// </remarks>
    virtual GcodeResult submit_gcode(const char *gcode) override {
        const std::lock_guard guard(mutex);

        settle_filament_change();

        const std::string_view line = trim(gcode);

        if (line.starts_with("M9933")) {
            // Connect's own marker, not something a server composed: after submitting a gcode it
            // takes a cork and submits M9933 C<cookie>, then waits for that cookie to come back done
            // before it will accept another command (background.cpp:22-35, :90-101). On hardware
            // Marlin marks it when the gcode EXECUTES, which is why an M702 running for minutes makes
            // the next command meet a refusal.
            //
            // Held rather than marked here, for exactly that reason: it completes when this rig's own
            // queue drains, so the window matches the operation instead of closing instantly.
            pending_cork = parameter(line.substr(5), 'C').transform(
                [](unsigned c) { return static_cast<buddy::cork::Tracker::Cookie>(c); });
        } else if (line.starts_with("T")) {
            tool_change(line.substr(1));
        } else if (line.starts_with("M701")) {
            filament_gcode(line.substr(4), Operation::load);
        } else if (line.starts_with("M702")) {
            filament_gcode(line.substr(4), Operation::unload);
        }

        // Again, so that a gcode arriving at an idle printer starts now rather than on the next
        // telemetry frame.
        settle_filament_change();

        return GcodeResult::Submitted;
    }

    // set_printer_ready only succeeds from Idle on hardware.
    virtual bool set_ready(bool ready) override {
        const std::lock_guard guard(mutex);

        if (!ready) {
            if (state.state.device_state == printer_state::DeviceState::Ready) {
                state.state = printer_state::DeviceState::Idle;
            }

            // Un-readying cannot fail (planner.cpp asserts as much).
            return true;
        }

        if (state.state.device_state == printer_state::DeviceState::Idle) {
            state.state = printer_state::DeviceState::Ready;

            return true;
        }

        return false;
    }

    // MarlinPrinter::set_idle, marlin_printer.cpp:579-586 - only from the Finished/Stopped screen.
    virtual bool set_idle() override {
        const std::lock_guard guard(mutex);

        if (state.state.device_state == printer_state::DeviceState::Finished
            || state.state.device_state == printer_state::DeviceState::Stopped) {
            state.state = printer_state::DeviceState::Idle;
            state.job_id = 0;
            state.has_job = false;

            return true;
        }

        return false;
    }

    virtual bool is_printing() const override {
        const std::lock_guard guard(mutex);

        return state.state.device_state == printer_state::DeviceState::Printing;
    }

    virtual bool is_in_error() const override {
        return false;
    }

    virtual bool is_idle() const override {
        const std::lock_guard guard(mutex);

        return state.state.device_state == printer_state::DeviceState::Idle;
    }

    virtual uint32_t cancelable_fingerprint() const override {
        return 0;
    }

#if HAS_CANCEL_OBJECT()
    virtual void set_object_cancelled(uint16_t, bool) override {}
#endif

    // The rig is pre-provisioned, so this should never be reached; if the server ever issues
    // SET_TOKEN it is worth seeing rather than silently accepting.
    virtual void init_connect(const char *token) override {
        const std::lock_guard guard(mutex);

        strlcpy(config.token, token, Config::CONNECT_TOKEN_BUF_LEN);
    }

    virtual void reset_printer() override {}

    virtual const char *dialog_action(printer_state::DialogId, Response) override {
        return "Rig has no dialogs";
    }

    virtual std::optional<FinishedJobResult> get_prior_job_result(uint16_t) const override {
        return std::nullopt;
    }

    virtual void set_slot_info(VirtualToolIndex, const SlotInfo &) override {}

private:
    enum class Operation {
        load,
        unload,
    };

    static std::string_view trim(const char *gcode) {
        std::string_view line = gcode == nullptr ? std::string_view {} : std::string_view { gcode };

        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1);
        }

        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r' || line.back() == '\n')) {
            line.remove_suffix(1);
        }

        return line;
    }

    /// Reads one unsigned parameter, as in `T2` or `T 2`. Absent means nullopt; present but not a
    /// number is reported as such, because `T` with a bad argument is a different case from no `T`.
    static std::optional<unsigned> read_uint(std::string_view text) {
        while (!text.empty() && text.front() == ' ') {
            text.remove_prefix(1);
        }

        unsigned value = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

        if (result.ec != std::errc {}) {
            return std::nullopt;
        }

        return value;
    }

    /// Where `X` appears as a parameter rather than inside a quoted value, so the L of `S"FLEX"`
    /// cannot be read as one.
    static std::optional<size_t> parameter_at(std::string_view params, char letter) {
        for (size_t at = 0; at < params.size(); at++) {
            if (params[at] == letter && (at == 0 || params[at - 1] == ' ')) {
                return at;
            }
        }

        return std::nullopt;
    }

    /// Finds `X<number>` in a parameter list, as firmware's parser would.
    static std::optional<unsigned> parameter(std::string_view params, char letter) {
        const std::optional<size_t> at = parameter_at(params, letter);

        return at.has_value() ? read_uint(params.substr(*at + 1)) : std::nullopt;
    }

    /// Whether a flag is present at all, value or not - firmware's `option<bool>`, where a bare `R`
    /// is true. Distinct from parameter(), which answers nullopt for both "absent" and "no number".
    static bool flag(std::string_view params, char letter) {
        return parameter_at(params, letter).has_value();
    }

    /// Reads `S"PLA"` - M701's RepRap-compatible filament name (M701_2_parse.cpp:36).
    static std::optional<FilamentTypeParameters::Name> material_parameter(std::string_view params) {
        const size_t open = params.find("S\"");

        if (open == std::string_view::npos) {
            return std::nullopt;
        }

        const size_t close = params.find('"', open + 2);

        if (close == std::string_view::npos) {
            return std::nullopt;
        }

        const std::string_view name = params.substr(open + 2, close - open - 2);

        if (name.size() >= FilamentTypeParameters::Name::capacity()) {
            return std::nullopt;
        }

        return FilamentTypeParameters::Name { name };
    }

    /// GcodeSuite::get_target_virtual_from_optional, gcode.cpp:122-154, minus the tool mapping this
    /// rig does not model. Announces its refusals on stdout where firmware announces them on serial -
    /// the same place, in the sense that Connect sees neither.
    std::optional<VirtualToolIndex> resolve_tool(std::optional<unsigned> requested) const {
        std::optional<unsigned> raw = requested;

        if (!raw.has_value()) {
            // No `T`: whatever is picked, which may legally be nothing.
            if (const auto *picked = std::get_if<VirtualToolIndex>(&state.active_slot)) {
                raw = picked->to_raw();
            } else {
                printf("rig: no tool given and none picked - the gcode does nothing, and says so only on serial\n");

                return std::nullopt;
            }
        } else if (*raw == VirtualToolIndex::count) {
            // The count itself is the legal way to spell NoTool - "T5 will park all tools" is how
            // Configuration_XL.h puts it for five, and this build's count decides the number here.
            printf("rig: T%u means no tool - the gcode does nothing\n", *raw);

            return std::nullopt;
        } else if (*raw > VirtualToolIndex::count) {
            printf("rig: T%u is not a tool index\n", *raw);

            return std::nullopt;
        }

        if ((state.slot_mask & (1 << *raw)) == 0) {
            printf("rig: tool %u is not enabled on this printer - the gcode does nothing\n", *raw + 1);

            return std::nullopt;
        }

        return VirtualToolIndex::from_raw(static_cast<uint8_t>(*raw));
    }

    /// A bare `T<n>` tool change. Nothing a server may send reaches this - Connect's gcode allowlists
    /// are a server-side matter and firmware would run it - so in practice it arrives from the rig's
    /// stdin channel, which is the only way to put the printer in a given picked state.
    ///
    /// Applied at once rather than queued behind a running load, unlike M701/M702. On hardware it
    /// would take its turn; here it is the setup lever, and its realistic use is between operations.
    void tool_change(std::string_view params) {
        const std::optional<unsigned> requested = read_uint(params);

        if (!requested.has_value()) {
            return;
        }

        if (*requested > VirtualToolIndex::count) {
            // Not a tool index at all, so nothing is picked or unpicked (gcode.cpp:100-101).
            printf("rig: T%u is not a tool index - still on %s\n", *requested,
                std::holds_alternative<NoTool>(state.active_slot) ? "no tool" : "the same tool");

            return;
        }

        if (*requested == VirtualToolIndex::count) {
            // T equal to the count parks everything - "T5 will park all tools" is Configuration_XL.h's
            // wording for a five-tool machine. The one legal way to spell NoTool, and on anything but
            // INDX the only way back to it once a load or unload has picked a tool.
            state.active_slot = NoTool {};
            printf("rig: T%u - nothing picked\n", *requested);

            return;
        }

        if ((state.slot_mask & (1 << *requested)) == 0) {
            printf("rig: tool %u is not enabled on this printer\n", *requested + 1);

            return;
        }

        state.active_slot = VirtualToolIndex::from_raw(static_cast<uint8_t>(*requested));
        printf("rig: picked tool %u\n", *requested + 1);
    }

    /// Queues one. Nothing is resolved or acted on here: firmware answers a gcode when it reaches the
    /// queue and runs it later, so which tool `M702 W0` means depends on what is picked when its turn
    /// comes - which an earlier queued gcode may still change.
    void filament_gcode(std::string_view params, Operation operation) {
        pending.push_back(PendingGcode {
            .operation = operation,
            .requested_tool = parameter(params, 'T'),
            // Absent or malformed S"" on a load leaves PLA, so a bare M701 still puts something in.
            // An unload always ends with the tool holding nothing.
            .material = operation == Operation::load
                ? material_parameter(params).value_or(FilamentTypeParameters::Name { "PLA" })
                : FilamentTypeParameters::Name {},
            // M701's R, "resume print if paused" - only INDX reads it, and only to decide whether to
            // dock afterwards.
            .resume_print = flag(params, 'R'),
        });
    }

    /// Runs the filament queue up to the current time: starts the front one if it has not started,
    /// finishes it when its time comes, and moves on. Called under the lock by everything that reads
    /// or writes slot state, so nothing can observe a change that should already have finished.
    void settle_filament_change() const {
        while (!pending.empty()) {
            PendingGcode &front = pending.front();

            if (!front.started) {
                front.started = true;
                front.tool = resolve_tool(front.requested_tool);

                if (!front.tool.has_value()) {
                    // The no-op: resolve_tool has said why on stdout, firmware would have said it on
                    // serial, and Connect was told Submitted long before either.
                    pending.pop_front();

                    continue;
                }

                front.completes_at_ms = ticks_ms() + filament_change_duration_s * 1000;

                // Both gcodes pick the target tool before they do anything (M701_2.cpp:103, :169),
                // and on everything but INDX it stays picked afterwards - so a second unload carrying
                // no T targets the same tool again rather than doing nothing.
                state.active_slot = *front.tool;

                // W0 is "preheat no return no cool down", so the nozzle is left hot afterwards -
                // which is why nothing here ever puts the target back to zero.
                state.target_nozzle = filament_change_nozzle_temp;
                state.slots[*front.tool].temp_nozzle = filament_change_nozzle_temp;

                printf("rig: %s tool %u%s%s, %u s\n",
                    front.operation == Operation::load ? "loading" : "unloading",
                    front.tool->to_raw() + 1,
                    front.material.empty() ? "" : " with ",
                    front.material.empty() ? "" : front.material.data(),
                    filament_change_duration_s);
            }

            if (ticks_ms() < front.completes_at_ms) {
                return;
            }

            state.slots[*front.tool].material = front.material;

            printf("rig: tool %u now holds %s\n", front.tool->to_raw() + 1,
                front.material.empty() ? "nothing" : front.material.data());

            // M701_2.cpp:112 - `R` only means anything while a print is actually paused, and then it
            // means two separate things. This is the one flag whose two halves are gated
            // differently, so they are written apart here rather than together.
            const bool resuming = front.operation == Operation::load && front.resume_print
                && state.state.device_state == printer_state::DeviceState::Paused;

#if HAS_INDX()
            // INDX docks the head when it is done, and only INDX does - the XL leaves it picked
            // (M701_2.cpp:127-131 for the load, :185-188 for the unload). This is what makes a
            // second filament gcode carrying no T a no-op on these machines: the printer has put
            // itself back to "nothing picked" without being asked.
            //
            // Except when it is feeding a print about to restart, which would make docking absurd.
            if (!resuming) {
                state.active_slot = NoTool {};
                printf("rig: docked - nothing picked\n");
            }
#endif

            // The other half, and it is NOT inside the guard above: print_resume() is unconditional
            // (M701_2.cpp:133), so a load with `R` restarts a paused print on every printer, not just
            // on the ones that dock.
            if (resuming) {
                state.state = printer_state::DeviceState::Printing;
                printf("rig: resumed the print\n");
            }

            pending.pop_front();
        }

        // The queue is empty, so anything corked behind it has now run. Nothing to do while a cork is
        // outstanding and work is still pending: that is the printer being busy, and the planner
        // refusing further commands for the duration is the behaviour being reproduced.
        if (pending_cork.has_value() && pending.empty()) {
            buddy::cork::tracker.mark_done(*pending_cork);
            pending_cork.reset();
        }
    }

    struct PendingGcode {
        Operation operation;
        /// The `T` as written, 0-based, or nullopt for a gcode that carried none. Resolved when the
        /// gcode runs rather than when it is queued, because that is when firmware resolves it.
        std::optional<unsigned> requested_tool;
        /// What the tool holds once it finishes. Empty is an unload.
        FilamentTypeParameters::Name material;
        /// M701's `R` - resume the print if one is paused. Two effects, gated differently: it
        /// restarts the print on any build, and on INDX it also keeps the tool from docking.
        bool resume_print = false;

        bool started = false;
        std::optional<VirtualToolIndex> tool = std::nullopt;
        uint32_t completes_at_ms = 0;
    };

    Config config;

    // Guards everything below. The Connect thread reads and writes it, and the rig's stdin channel
    // writes it too - which is what the class did not have to consider while its only mutations came
    // from Connect's own loop. `params()` is const and settles pending work, hence the mutable pair.
    mutable std::mutex mutex;
    mutable Params state { std::nullopt };
    mutable std::deque<PendingGcode> pending;
    /// A cork waiting for this rig's queue to drain; see submit_gcode's M9933 branch.
    mutable std::optional<buddy::cork::Tracker::Cookie> pending_cork = std::nullopt;

    uint32_t filament_change_duration_s = 5;

    // Clock-derived job progression; see params(). ticks_ms() is elapsed milliseconds since process
    // start (rig_time.cpp) - the same clock the planner itself reads.
    uint32_t print_start_ms = 0;
    uint32_t print_duration_s = 3600;
    std::optional<uint32_t> pause_at_s = std::nullopt;
};

} // namespace connect_client
