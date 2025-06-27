/**
 * @file ph_pingsweep.h
 * @brief Public API for Phobos Ping Sweep tool
 * @author Neur0sis (2025)
 * 
 * The Ping Sweep tool is designed to perform a network discovery on a subnet by sending 
 * ICMP Echo Request (ping) packets to a range of IP addresses. It identifies which hosts are 
 * alive and reachable, providing a simple way to map out devices on a network.
 * 
 * This tool is part of the Phobos Offensive Security Framework and is intended for
 * authorized penetration testing and network analysis. It is not intended for malicious use.
 *
 * This tool is part of the Phobos module from the Nyx Offensive Security Framework.
 */

 #ifndef PH_PINGSWEEP_H
 #define PH_PINGSWEEP_H

/**
 * @name Constants
 * Configuration constants for the ping sweep tool
 * @{
 */
#define PH_PINGSWEEP_MAX_IP_LEN 16     /**< Maximum IPv4 address length (XXX.XXX.XXX.XXX\0) */
#define PH_PINGSWEEP_MAX_HOSTS 65536   /**< Maximum number of hosts to scan in a single sweep */
#define PH_PINGSWEEP_DEFAULT_TIMEOUT 1000  /**< Default timeout for each ping in milliseconds */
#define PH_PINGSWEEP_DEFAULT_THREADS 4  /**< Default number of threads for scanning */
/** @} */

/**
 * @name Status Codes
 * Return values for API functions
 * @{
 */
#define PH_PINGSWEEP_SUCCESS 0          /**< Operation completed successfully */
#define PH_PINGSWEEP_ERR_INVALID_IP -1  /**< Invalid IP address format */
#define PH_PINGSWEEP_ERR_SOCKET -2      /**< Socket creation failed */
#define PH_PINGSWEEP_ERR_SEND -3        /**< Failed to send ICMP packet */
#define PH_PINGSWEEP_ERR_RECEIVE -4     /**< Failed to receive ICMP response */
#define PH_PINGSWEEP_ERR_TIMEOUT -5     /**< Ping request timed out */
#define PH_PINGSWEEP_ERR_NO_HOSTS -6    /**< No hosts found in the specified range */
#define PH_PINGSWEEP_ERR_PERMISSION -7  /**< Insufficient permissions to send ICMP packets */
#define PH_PINGSWEEP_ERR_MEMORY -8      /**< Memory allocation failed */
#define PH_PINGSWEEP_ERR_THREAD -9      /**< Thread creation or management failed */
#define PH_PINGSWEEP_ERR_INVALID_PARAM -10 /**< Invalid parameter provided */
#define PH_PINGSWEEP_ERR_CANCELED -11   /**< Operation was canceled */
/** @} */


 #endif // PH_PINGSWEEP_H

