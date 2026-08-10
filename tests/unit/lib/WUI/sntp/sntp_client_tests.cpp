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
// Controls what netdev_get_status() reports for the ETH interface.
static netdev_status_t eth_status = NETDEV_NETIF_DOWN;

extern "C" const char *wui_get_ntp_server(void) {
    return ntp_server_config;
}

extern "C" netdev_status_t netdev_get_status(uint32_t) {
    return eth_status;
}

namespace {

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
    ntp_server_config = nullptr;
    bring_up();

    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
}

TEST_CASE("sntp: configured server overrides the compiled-in default") {
    // sntp_init() unconditionally resets server 0 to SNTP_SERVER_ADDRESS,
    // so this only passes when the override is applied after it.
    ntp_server_config = "ntp.example.test";
    bring_up();

    CHECK(strcmp(server_name(), "ntp.example.test") == 0);

    bring_down();
    ntp_server_config = nullptr;
}

TEST_CASE("sntp: config change is picked up on the next netif down/up cycle") {
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

TEST_CASE("sntp: clearing the override reverts to the default on reconnect") {
    ntp_server_config = "10.0.0.5";
    bring_up();
    CHECK(strcmp(server_name(), "10.0.0.5") == 0);

    ntp_server_config = nullptr;
    bring_down();
    bring_up();
    CHECK(strcmp(server_name(), SNTP_SERVER_ADDRESS) == 0);

    bring_down();
}
