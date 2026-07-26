// Real time for the connect rig, replacing tests/unit/connect/missing_functions_time.cpp.
//
// That file returns 0 forever from ticks_ms()/ticks_s() and makes osDelay() a no-op, which is
// correct for a unit test and fatal here: the planner decides when to send telemetry and pings by
// comparing now() against timestamps (planner.cpp TELEMETRY_INTERVAL_*, connect.cpp
// ping_inactivity), so a frozen clock means it either never sends or sends without pause, and a
// no-op osDelay() spins the idle loop (sleep.cpp) at 100% CPU.

#include <chrono>
#include <cstdint>
#include <thread>

namespace {

std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

uint64_t elapsed_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time)
        .count();
}

} // namespace

extern "C" {

// Monotonic and wrapping at 32 bits, exactly like the firmware's own tick counter - the planner
// does unsigned subtraction on these, so wrap-around is handled by it rather than by us.
uint32_t ticks_ms() {
    return static_cast<uint32_t>(elapsed_ms());
}

uint32_t ticks_s() {
    return static_cast<uint32_t>(elapsed_ms() / 1000);
}
}

uint32_t osDelay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));

    return 0;
}
