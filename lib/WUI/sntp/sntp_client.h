#ifndef SNTP_HANDLE_H
#define SNTP_HANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

void sntp_client_init(void);
void sntp_client_step(void);

/*!
 * \brief Forces the next sntp_client_step() to re-initialize the SNTP client
 *
 * To be called when the network settings change. Without it a settings reload
 * is not picked up, because sntp_client_step() only re-initializes on a netif
 * down->up transition and the reconfiguration happens between two of its polls.
 *
 * Touches no lwIP state, so it needs no tcpip core lock -- it only clears a
 * volatile word-sized flag that sntp_client_step() reads.
 */
void sntp_client_reset(void);

#ifdef __cplusplus
}
#endif

#endif // SNTP_HANDLE_H
