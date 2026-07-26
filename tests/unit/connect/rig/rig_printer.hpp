#pragma once

// The Printer implementation the connect rig runs the real Connect client against - the seam that
// MarlinPrinter occupies on hardware (see src/connect/run.cpp). Structurally a copy of
// tests/unit/connect/mock_printer.h, with three deliberate differences:
//
//   1. load_config() returns a real, enrolled config (host/port/token, TLS off). That is the
//      USB-provisioning shape: a printer that already holds a token skips /p/register entirely
//      (marlin_printer.cpp's connect_ini_handler does the same on hardware).
//   2. params() reports live state from this object, so telemetry actually changes over time.
//   3. job_control()/set_ready()/set_idle() reproduce MarlinPrinter's real state gates
//      (marlin_printer.cpp:442, :574-586), so a Pause/Stop round-trip answers FINISHED or REJECTED
//      for the same reasons hardware would.

#include <connect/printer.hpp>
#include <general_response.hpp>
#include <marlin_server_shared.h>
#include <timing.h>

#include <algorithm>
#include <cstring>
#include <optional>

extern "C" size_t strlcpy(char *, const char *, size_t);

namespace connect_client {

class RigPrinter final : public Printer {
public:
    RigPrinter(const char *host, uint16_t port, const char *token, const char *fingerprint,
               const char *serial, const char *firmware) {
        info.appendix = false;
        info.firmware_version = firmware;
        strlcpy(info.fingerprint, fingerprint, PrinterInfo::FINGERPRINT_BUFF_LEN);
        strlcpy(info.serial_number.begin(), serial, PrinterInfo::SER_NUM_BUFR_LEN);

        strlcpy(config.host, host, Config::CONNECT_URL_BUF_LEN);
        strlcpy(config.token, token, Config::CONNECT_TOKEN_BUF_LEN);
        config.port = port;
        config.tls = false;
        config.enabled = true;
        config.loaded = true;

        state.state = printer_state::DeviceState::Idle;
        state.version = { 1, 3, 5 };
        state.slot_mask = 1;
        state.slots[0] = SlotInfo {
            .nozzle_diameter = 0.4f,
        };
        state.temp_bed = 24;
        state.target_bed = 0;
        state.target_nozzle = 0;
        state.slots[0].temp_nozzle = 25;
    }

    /// Starts a simulated job. <paramref name="duration_s"/> is how long it pretends to take, and
    /// <paramref name="filament_change_in_s"/> optionally schedules one pause (an M600 colour swap) that
    /// many seconds in - which is what makes the firmware emit `filament_change_in` at all.
    void start_fake_print(uint16_t job_id, uint32_t duration_s = 3600,
                          std::optional<uint32_t> filament_change_in_s = std::nullopt) {
        state.job_id = job_id;
        state.has_job = true;
        state.state = printer_state::DeviceState::Printing;
        state.target_nozzle = 215;
        state.slots[0].temp_nozzle = 215;
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
    /// `Params` is returned by value and this method is const, so computing on top of the stored
    /// snapshot mutates nothing and needs no synchronisation.
    /// </remarks>
    virtual Params params() const override {
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

    virtual GcodeResult submit_gcode(const char *) override {
        return GcodeResult::Submitted;
    }

    // set_printer_ready only succeeds from Idle on hardware.
    virtual bool set_ready(bool ready) override {
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
        return state.state.device_state == printer_state::DeviceState::Printing;
    }

    virtual bool is_in_error() const override {
        return false;
    }

    virtual bool is_idle() const override {
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
    Config config;
    Params state { std::nullopt };

    // Clock-derived job progression; see params(). ticks_ms() is elapsed milliseconds since process
    // start (rig_time.cpp) - the same clock the planner itself reads.
    uint32_t print_start_ms = 0;
    uint32_t print_duration_s = 3600;
    std::optional<uint32_t> pause_at_s = std::nullopt;
};

} // namespace connect_client
