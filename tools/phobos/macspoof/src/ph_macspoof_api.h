/**
 * @file ph_macspoof_api.h
 * @brief Public API for Phobos MAC address spoofing module
 * @author Neur0sis (2025)
 *
 * The Phobos MAC address spoofing module provides functionality to modify,
 * randomize, and restore MAC addresses on network interfaces. It's designed
 * for security professionals conducting authorized penetration tests.
 *
 * Features:
 * - List all network interfaces with their current MAC addresses
 * - Generate realistic random MAC addresses
 * - Apply custom MAC addresses to interfaces
 * - Save and restore original MAC addresses
 *
 * On error, this module sets detailed error information in the nyx_error system,
 * which can be retrieved and reported using nyx_error_get() and nyx_error_log().
 *
 * This module is part of the Nyx Offensive Security Framework.
 */

#ifndef PH_MACSPOOF_API_H
#define PH_MACSPOOF_API_H

#include <stddef.h>

/**
 * Maximum length of a MAC address string (XX:XX:XX:XX:XX:XX\0)
 */
#define PH_MAX_MAC_LEN 18

/**
 * @name Status Codes
 * Return values for API functions
 * @{
 */
#define PH_SUCCESS           0  /**< Operation completed successfully */
#define PH_ERR_INVALID_MAC   -1 /**< MAC address format is invalid */
#define PH_ERR_NO_IFACE      -2 /**< Interface doesn't exist or invalid */
#define PH_ERR_SOCKET        -3 /**< Socket creation failed */
#define PH_ERR_IOCTL         -4 /**< IOCTL operation failed */
#define PH_ERR_FILE_IO       -5 /**< File I/O operation failed */
#define PH_ERR_PERMISSION    -6 /**< Insufficient permissions */
#define PH_ERR_NOT_FOUND     -7 /**< Requested resource not found */
#define PH_ERR_ALREADY_SAVED -8 /**< MAC address already saved */
#define PH_ERR_BUSY          -9 /**< Device or resource busy */
/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lists all available network interfaces with their MAC addresses
 *
 * Outputs a list of network interfaces detected on the system along with
 * their operational status (UP/DOWN) and current MAC address.
 * This is a convenience function for CLI usage that prints directly to stdout.
 *
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_list_interfaces_stdout(void);

/**
 * Retrieves the current MAC address of a network interface
 *
 * @param iface Interface name (e.g., "eth0")
 * @param buffer Buffer to store the MAC address string
 * @param len Length of the buffer (should be >= PH_MAX_MAC_LEN)
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_get_current_mac(const char *iface, char *buffer, size_t len);

/**
 * Changes the MAC address of a network interface
 *
 * This function will:
 * 1. Bring the interface down
 * 2. Change the MAC address
 * 3. Bring the interface back up
 *
 * @param iface Interface name (e.g., "eth0")
 * @param mac New MAC address in XX:XX:XX:XX:XX:XX format
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_change_mac(const char *iface, const char *mac);

/**
 * Generates a realistic random MAC address
 *
 * Creates a random MAC address using common OUI prefixes to appear
 * as legitimate hardware from known vendors.
 *
 * @param buffer Buffer to store the generated MAC address (≥ PH_MAX_MAC_LEN)
 * @param len Length of the buffer
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_generate_random_mac(char *buffer, size_t len);

/**
 * Saves the original MAC address for later restoration
 *
 * @param iface Interface name (e.g., "eth0")
 * @param mac MAC address to save
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_save_original_mac(const char *iface, const char *mac);

/**
 * Loads the previously saved original MAC address
 *
 * @param iface Interface name (e.g., "eth0")
 * @param buffer Buffer to store the loaded MAC address
 * @param len Length of the buffer (should be >= PH_MAX_MAC_LEN)
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_load_original_mac(const char *iface, char *buffer, size_t len);

/**
 * Deletes the saved original MAC address
 *
 * @param iface Interface name (e.g., "eth0")
 */
void ph_macspoof_delete_original_mac(const char *iface);

/**
 * Restores the original MAC address of an interface
 *
 * Loads the previously saved original MAC address and
 * applies it to the specified interface.
 *
 * @param iface Interface name (e.g., "eth0")
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_restore_mac(const char *iface);

/**
 * @name CLI Wrapper Functions
 * High-level functions intended for CLI usage
 * @{
 */

/**
 * Shows the current MAC address of an interface
 *
 * @param iface Interface name (e.g., "eth0")
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_show_mac(const char *iface);

/**
 * Applies a random MAC address to an interface
 *
 * @param iface Interface name (e.g., "eth0")
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_random_mac(const char *iface);

/**
 * Applies a custom MAC address to an interface
 *
 * @param iface Interface name (e.g., "eth0")
 * @param mac Custom MAC address to apply
 * @return PH_SUCCESS on success, or error code on failure
 */
int ph_macspoof_custom_mac(const char *iface, const char *mac);
/** @} */

#ifdef __cplusplus
}
#endif

#endif // PH_MACSPOOF_API_H
