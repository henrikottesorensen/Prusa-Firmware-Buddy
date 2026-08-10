#include <catch2/catch_test_macros.hpp>

#include <cstring>

extern "C" {
#include "netdev.h"
#include "sntp/sntp.h"
#include "sntp/sntp_client.h"
#include "sntp/sntp_opts.h"
}

/*
 * The sntp_client keeps its running/stopped state in a static variable, so
 * every test case must leave the client stopped (netif down + step) to not
 * leak state into the next one. The bring_up/bring_down helpers below take
 * care of that.
 */

// Controls what the wui_get_ntp_server() stub hands to sntp_client_init().
static const char *ntp_server_config = nullptr;
// Controls what netdev_get_status() reports per interface. The wifi one stays
// down in the HAS_ESP() builds unless a test raises it, which is also what a
// critical-infrastructure printer looks like: the firmware has the wifi code,
// the hardware is gone, so the interface never comes up.
static netdev_status_t eth_status = NETDEV_NETIF_DOWN;
static netdev_status_t esp_status = NETDEV_NETIF_DOWN;

extern "C" const char *wui_get_ntp_server(void) {
    return ntp_server_config;
}

extern "C" netdev_status_t netdev_get_status(uint32_t netdev_id) {
#if HAS_ESP()
    if (netdev_id == NETDEV_ESP_ID) {
        return esp_status;
    }
#else
    (void)netdev_id;
#endif
    return eth_status;
}

// Counted by the stub in missing_functions.c. A failed lwIP assert is not fatal
// on the firmware either -- it only log_criticals -- so without this a contract
// violation is invisible to every test here.
extern "C" unsigned lwip_assert_count;

namespace {

/// Fails the case if any lwIP assert fired during it.
struct NoLwipAsserts {
    NoLwipAsserts() { lwip_assert_count = 0; }
    ~NoLwipAsserts() { CHECK(lwip_assert_count == 0); }
};

void bring_up() {
    eth_status = NETDEV_NETIF_UP;
    sntp_client_step();
}

void bring_down() {
    eth_status = NETDEV_NETIF_DOWN;
    sntp_client_step();
}

const char *server_name() {
    const char *name = sntp_getservername(0);
    REQUIRE(name != nullptr);
    return name;
}

} // namespace

TEST_CASE("sntp: default server is used when no override is configured") {
    NoLwipAsserts no_asserts;
    ntp_server_config = nullptr;
    bring_up();

    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
}

TEST_CASE("sntp: configured server overrides the compiled-in default") {
    NoLwipAsserts no_asserts;
    // sntp_init() unconditionally resets server 0 to SNTP_SERVER_ADDRESS,
    // so this only passes when the override is applied after it.
    ntp_server_config = "ntp.example.test";
    bring_up();

    CHECK(strcmp(server_name(), "ntp.example.test") == 0);

    bring_down();
    ntp_server_config = nullptr;
}

TEST_CASE("sntp: config change is picked up on the next netif down/up cycle") {
    NoLwipAsserts no_asserts;
    ntp_server_config = nullptr;
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    // A config change alone doesn't re-register the server; the client only
    // re-initializes after it observes the interface go down and up again.
    ntp_server_config = "10.0.0.5";
    sntp_client_step();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
    bring_up();
    CHECK(strcmp(server_name(), "10.0.0.5") == 0);

    bring_down();
    ntp_server_config = nullptr;
}

TEST_CASE("sntp: reset picks up a config change without a down/up cycle") {
    NoLwipAsserts no_asserts;
    // reconfigure() brings the interfaces down and up within a single pass of
    // the network loop, so sntp_client_step() never observes the down state and
    // a settings reload would otherwise not be applied until the next reboot.
    ntp_server_config = nullptr;
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    ntp_server_config = "10.0.0.5";
    sntp_client_reset();
    sntp_client_step(); // interface never went down
    CHECK(strcmp(server_name(), "10.0.0.5") == 0);

    // And the reverse: clearing it reverts to the default, which is what the
    // ini documents. Without the reset the registered pointer would still be
    // aiming at the config store buffer that has just been emptied.
    ntp_server_config = nullptr;
    sntp_client_reset();
    sntp_client_step();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
}

#if HAS_ESP()

TEST_CASE("sntp: wifi staying up hides an ethernet down/up entirely") {
    NoLwipAsserts no_asserts;
    // sntp_client_step() ORs the two interfaces, so on a printer with wifi
    // associated an ethernet bounce is never visible as "down" at all -- not
    // even to a poll that happens to land in the middle of it. The reset is
    // then the only thing that re-applies a changed server.
    esp_status = NETDEV_NETIF_UP;
    ntp_server_config = nullptr;
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    ntp_server_config = "10.0.0.5";
    bring_down(); // ethernet only; wifi still up, so the client stays running
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    sntp_client_reset();
    sntp_client_step();
    CHECK(strcmp(server_name(), "10.0.0.5") == 0);

    esp_status = NETDEV_NETIF_DOWN;
    bring_down();
    ntp_server_config = nullptr;
}

TEST_CASE("sntp: wifi hardware removed behaves like ethernet only") {
    NoLwipAsserts no_asserts;
    // The critical-infrastructure editions run this build with the wifi
    // circuitry physically removed, so NETDEV_ESP_ID never comes up.
    esp_status = NETDEV_NETIF_DOWN;
    ntp_server_config = "10.0.0.5";
    bring_up();
    CHECK(strcmp(server_name(), "10.0.0.5") == 0);

    ntp_server_config = nullptr;
    bring_down();
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
}

#endif

TEST_CASE("sntp: clearing the override reverts to the default on reconnect") {
    NoLwipAsserts no_asserts;
    ntp_server_config = "10.0.0.5";
    bring_up();
    CHECK(strcmp(server_name(), "10.0.0.5") == 0);

    ntp_server_config = nullptr;
    bring_down();
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
}
