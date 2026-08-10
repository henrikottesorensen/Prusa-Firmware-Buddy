#include "sntp.h"
#include "sntp_client.h"
#include "netdev.h"
#include "wui_api.h"
#include "tcpip.h"

#include <option/has_esp.h>

// volatile: written by sntp_client_reset(), read by sntp_client_step().
static volatile uint32_t sntp_running = 0; // describes if sntp is currently running or not
void sntp_client_init(void) {
    // Stop first, and before sntp_setoperatingmode(), which asserts that the
    // client is not running. This is a no-op on the first call, when lwIP has
    // not allocated the UDP control block yet, and on a re-init it is what
    // makes the restart real: sntp_init() short-circuits when that block
    // already exists, so without the stop the server name would be re-applied
    // but no request would ever be scheduled.
    sntp_stop();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_init();

    // sntp_init() resets server 0 to the compiled-in default
    // (SNTP_SERVER_ADDRESS), so the override must be applied after it.
    const char *ntp_server = wui_get_ntp_server();
    if (ntp_server != NULL) {
        sntp_setservername(0, ntp_server);
    }
}

void sntp_client_reset(void) {
    // Only clears the latch; the lwIP calls stay in sntp_client_step(), which
    // already holds the tcpip core lock for them.
    sntp_running = 0;
}

void sntp_client_step(void) {
    bool netif_up = netdev_get_status(NETDEV_ETH_ID) == NETDEV_NETIF_UP;
#if HAS_ESP()
    netif_up |= netdev_get_status(NETDEV_ESP_ID) == NETDEV_NETIF_UP;
#endif

    if (!sntp_running && netif_up) {
        LOCK_TCPIP_CORE();
        sntp_client_init();
        UNLOCK_TCPIP_CORE();
        sntp_running = 1;
    } else if (sntp_running && !netif_up) {
        LOCK_TCPIP_CORE();
        sntp_stop();
        UNLOCK_TCPIP_CORE();
        sntp_running = 0;
    }
}
