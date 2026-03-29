/**
 * @file ph_arpspoof_api.h
 * @brief Public API for Phobos ARP spoofing module
 * @author Neur0sis (2025)
 *
 * The Phobos ARP spoofing module provides functionality to perform ARP cache
 * poisoning on a local network segment. It can impersonate any IP address by
 * sending forged ARP reply packets to a target, redirecting traffic through
 * the attacker's machine.
 *
 * Features:
 * - Poison a target's ARP cache to impersonate a given IP (e.g., gateway)
 * - Bidirectional poisoning (poison both target and gateway)
 * - Configurable send interval
 * - Graceful shutdown with ARP table restoration
 * - Signal-safe stop mechanism
 *
 * On error, this module sets detailed error information in the nyx_error system,
 * which can be retrieved and reported using nyx_error_get() and nyx_error_log().
 *
 * This module is part of the Nyx Offensive Security Framework.
 */

#ifndef PH_ARPSPOOF_API_H
#define PH_ARPSPOOF_API_H

#include <stddef.h>
#include <stdint.h>

/**
 * @name Constants
 * @{
 */
#define PH_ARPSPOOF_MAX_IP_LEN    16   /**< Max IPv4 string length (incl. NUL) */
#define PH_ARPSPOOF_MAX_MAC_LEN   18   /**< Max MAC string length (incl. NUL) */
#define PH_ARPSPOOF_DEFAULT_INTERVAL 1 /**< Default send interval in seconds */
/** @} */

/**
 * @name Status Codes
 * Return values for API functions
 * @{
 */
#define PH_ARPSPOOF_SUCCESS            0   /**< Operation completed successfully */
#define PH_ARPSPOOF_ERR_INVALID_PARAM -1   /**< Invalid parameter provided */
#define PH_ARPSPOOF_ERR_NO_IFACE     -2   /**< Interface not found or invalid */
#define PH_ARPSPOOF_ERR_SOCKET       -3   /**< Socket creation or operation failed */
#define PH_ARPSPOOF_ERR_SEND         -4   /**< Failed to send ARP packet */
#define PH_ARPSPOOF_ERR_RESOLVE      -5   /**< Failed to resolve MAC for IP */
#define PH_ARPSPOOF_ERR_PERMISSION   -6   /**< Insufficient permissions (need root) */
#define PH_ARPSPOOF_ERR_INVALID_IP   -7   /**< Invalid IP address format */
#define PH_ARPSPOOF_ERR_BUSY         -8   /**< Spoofing session already active */
#define PH_ARPSPOOF_ERR_NOT_RUNNING  -9   /**< No active spoofing session */
/** @} */

/**
 * ARP spoofing session configuration
 */
typedef struct {
    char iface[16];                         /**< Network interface name */
    char target_ip[PH_ARPSPOOF_MAX_IP_LEN]; /**< Target IP to poison */
    char spoof_ip[PH_ARPSPOOF_MAX_IP_LEN];  /**< IP address to impersonate */
    int interval;                            /**< Send interval in seconds */
    int bidirectional;                       /**< Poison both target and gateway */
} ph_arpspoof_config_t;

/**
 * Runtime state for an active spoofing session (opaque to callers)
 */
typedef struct ph_arpspoof_session ph_arpspoof_session_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialises a spoofing session from the given configuration.
 *
 * Opens a raw packet socket, resolves the local interface MAC, and
 * resolves the target (and optionally gateway) MAC addresses via ARP.
 *
 * @param config  Session configuration
 * @param session Pointer to receive the allocated session handle
 * @return PH_ARPSPOOF_SUCCESS on success, or an error code
 */
int ph_arpspoof_init(const ph_arpspoof_config_t *config,
                     ph_arpspoof_session_t **session);

/**
 * Releases all resources held by a spoofing session.
 *
 * Closes the raw socket and frees the session structure. Does NOT
 * restore ARP tables -- call ph_arpspoof_restore() first if needed.
 *
 * @param session Session handle (NULL-safe)
 */
void ph_arpspoof_cleanup(ph_arpspoof_session_t *session);

/**
 * Resolves the MAC address for a given IP on the local network.
 *
 * Uses the kernel ARP cache (SIOCGARP ioctl).
 *
 * @param session Active session handle
 * @param ip      IPv4 address to resolve
 * @param mac_buf Buffer to store result (>= PH_ARPSPOOF_MAX_MAC_LEN)
 * @param len     Buffer length
 * @return PH_ARPSPOOF_SUCCESS on success, or an error code
 */
int ph_arpspoof_resolve_mac(const ph_arpspoof_session_t *session,
                            const char *ip, char *mac_buf, size_t len);

/**
 * Sends a single forged ARP reply packet.
 *
 * Tells @p victim_ip that @p impersonated_ip has our MAC address.
 *
 * @param session         Active session handle
 * @param victim_ip       IP whose ARP cache we poison
 * @param victim_mac      MAC of the victim (destination)
 * @param impersonated_ip IP we claim to own
 * @return PH_ARPSPOOF_SUCCESS on success, or an error code
 */
int ph_arpspoof_send_reply(const ph_arpspoof_session_t *session,
                           const char *victim_ip,
                           const uint8_t *victim_mac,
                           const char *impersonated_ip);

/**
 * Starts the continuous ARP poisoning loop.
 *
 * Blocks until ph_arpspoof_stop() is called (typically from a signal
 * handler). In bidirectional mode both target and gateway are poisoned.
 *
 * @param session Active session handle
 * @return PH_ARPSPOOF_SUCCESS when stopped gracefully, or an error code
 */
int ph_arpspoof_start(ph_arpspoof_session_t *session);

/**
 * Signals the poisoning loop to stop.
 *
 * Safe to call from a signal handler (uses sig_atomic_t internally).
 *
 * @param session Active session handle (NULL-safe)
 */
void ph_arpspoof_stop(ph_arpspoof_session_t *session);

/**
 * Sends corrective ARP replies to restore original MAC mappings.
 *
 * Should be called before cleanup to leave the network in a clean state.
 *
 * @param session Active session handle
 * @return PH_ARPSPOOF_SUCCESS on success, or an error code
 */
int ph_arpspoof_restore(ph_arpspoof_session_t *session);

/**
 * Lists available network interfaces with details to stdout.
 *
 * Convenience wrapper around ph_iface_print_details().
 *
 * @return PH_ARPSPOOF_SUCCESS on success, or an error code
 */
int ph_arpspoof_list_interfaces_stdout(void);

#ifdef __cplusplus
}
#endif

#endif /* PH_ARPSPOOF_API_H */
