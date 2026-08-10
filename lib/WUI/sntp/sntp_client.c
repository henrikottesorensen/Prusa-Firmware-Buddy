#include "sntp.h"
#include "sntp_client.h"
#include "netdev.h"
#include "wui_api.h"
#include "tcpip.h"

#include <option/has_esp.h>

// volatile: written by sntp_client_reset(), read by sntp_client_step().
static volatile uint32_t sntp_running = 0; // describes if sntp is currently running or not
void sntp_client_init(void) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // sntp_init() short-circuits on an already allocated pcb, so on a re-init
    // it would re-apply only the server name and never schedule a request.
    // Stopping first makes a re-init a real restart, which is what a settings
    // change needs. It is a no-op the first time, when there is no pcb yet.
    sntp_stop();

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
