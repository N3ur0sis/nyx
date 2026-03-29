/**
 * @file ph_macspoof_impl.c
 * @brief Implementation of Phobos MAC Spoofing API
 * @author Neur0sis (2025)
 *
 * This module provides secure MAC address manipulation capabilities for
 * network interfaces on Linux systems. Designed for penetration testing
 * and security research within the NYX framework.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "nyx_logger.h"
#include "nyx_error.h"
#include "ph_macspoof_api.h"
#include "nyx_iface.h"

// Configuration constants
#define MAX_RETRY_ATTEMPTS 5	   // Maximum retries for operations
#define DEFAULT_RETRY_DELAY_MS 500 // Default delay between retries
#define INTERFACE_SETTLE_MS 100	   // Time to wait for interface state changes
#define BACKUP_FILE_PREFIX "macspoof_"
#define BACKUP_FILE_SUFFIX ".bak"

// Security and path constants
#define SECURE_DIR_MODE 0700  // Owner-only access for directories
#define SECURE_FILE_MODE 0600 // Owner-only access for files
#define FALLBACK_TMP_DIR "/tmp"

// Well-known OUI prefixes for realistic MAC generation
static const char *COMMON_OUIS[] = {
	"00:1A:2B", // Generic
	"B8:27:EB", // Raspberry Pi Foundation
	"00:16:6F", // Intel Corporate
	"3C:5A:B4", // Google, Inc.
	"F4:5C:89", // Apple, Inc.
	"00:50:56", // VMware, Inc.
	"DC:A6:32", // Raspberry Pi Trading Ltd
	"E0:CB:4E", // Apple, Inc.
	"FC:FB:FB", // Apple, Inc.
	"AC:DE:48", // Dell Inc.
	"00:15:5D", // Microsoft Corporation
	"08:00:27"	// PCS Systemtechnik GmbH (VirtualBox)
};

// Forward declarations for internal functions
static int create_backup_directory(char *path, size_t path_len);
static int atomic_file_write(const char *path, const char *content);
static void interface_settle_delay(void);
static int verify_mac_change(const char *iface, const char *expected_mac);

/**
 * Maps nyx_iface error codes to ph_macspoof error codes
 * with enhanced error messages and suggested fixes
 */
static int map_iface_error_to_macspoof(int err)
{
	switch (err)
	{
	case NYX_IFACE_SUCCESS:
		return PH_SUCCESS;
	case NYX_IFACE_ERR_PARAM:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid parameter for interface operation",
						 "Verify the interface name and MAC address format (XX:XX:XX:XX:XX:XX)");
		return PH_ERR_INVALID_MAC;
	case NYX_IFACE_ERR_NOTFOUND:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "Interface not found",
						 "Verify the interface exists using 'ip link' or run with '-l' to list available interfaces");
		return PH_ERR_NO_IFACE;
	case NYX_IFACE_ERR_SOCKET:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_SOCKET, NYX_ERROR_SEV_ERROR,
						 "Socket error during network interface operation",
						 "Check network stack health and kernel logs for more details");
		return PH_ERR_SOCKET;
	case NYX_IFACE_ERR_IO:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "I/O error during interface operation",
						 "Verify file system permissions and available disk space");
		return PH_ERR_FILE_IO;
	case NYX_IFACE_ERR_PERM:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_PERMISSION, NYX_ERROR_SEV_ERROR,
						 "Permission denied for interface operation",
						 "Try running the command with sudo privileges");
		return PH_ERR_PERMISSION;
	case NYX_IFACE_ERR_BUSY:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_BUSY, NYX_ERROR_SEV_WARNING,
						 "Interface is busy or locked by another process",
						 "Wait a moment and try again, or check if another program is using the interface");
		return PH_ERR_BUSY;
	default:
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_ERROR,
						 "Unknown error during interface operation",
						 "Check system logs for additional information");
		return PH_ERR_IOCTL;
	}
}

/**
 * Retrieves current MAC address by using the interface utility
 */
int ph_macspoof_get_current_mac(const char *iface, char *buffer, size_t len)
{
	if (!iface || !buffer || len < PH_MAX_MAC_LEN)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid parameters for getting MAC address",
						 "Ensure interface name is valid and buffer is large enough");
		return PH_ERR_INVALID_MAC;
	}

	int result = nyx_iface_get_mac(iface, buffer, len);
	return map_iface_error_to_macspoof(result);
}

/**
 * Validates MAC address format using the interface utility
 */
int ph_macspoof_is_valid_mac(const char *mac)
{
	if (!mac)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "NULL MAC address provided",
						 "MAC address must be in format XX:XX:XX:XX:XX:XX with valid hex digits");
		return 0;
	}

	return nyx_iface_is_valid_mac(mac);
}

/**
 * Checks if interface is up using the interface utility
 */
int ph_macspoof_is_interface_up(const char *iface)
{
	if (!iface)
	{
		NYX_ERROR_SET(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE,
					  "NULL interface name");
		return 0;
	}

	return nyx_iface_is_up(iface);
}

/**
 * Creates secure backup directory with proper permissions
 */
static int create_backup_directory(char *path, size_t path_len)
{
	const char *candidates[] = {
		"/var/lib/nyx",		// System-wide location (root)
		NULL,				// Will be set to $HOME/.nyx
		"/tmp/nyx_macspoof" // Fallback
	};

	char home_path[PATH_MAX] = {0};
	const char *home = getenv("HOME");
	if (home && *home)
	{
		snprintf(home_path, sizeof(home_path), "%s/.nyx", home);
		candidates[1] = home_path;
	}

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
	{
		if (!candidates[i])
			continue;

		// Check if we can use this location
		if (geteuid() != 0 && i == 0)
			continue; // Skip /var/lib/nyx if not root

		struct stat st;
		if (stat(candidates[i], &st) == 0)
		{
			// Directory exists, check if it's usable
			if (S_ISDIR(st.st_mode) && access(candidates[i], W_OK) == 0)
			{
				if (snprintf(path, path_len, "%s", candidates[i]) < (int)path_len)
				{
					return 0;
				}
			}
		}
		else
		{
			// Try to create it
			if (mkdir(candidates[i], SECURE_DIR_MODE) == 0 || errno == EEXIST)
			{
				if (snprintf(path, path_len, "%s", candidates[i]) < (int)path_len)
				{
					return 0;
				}
			}
		}
	}

	NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
					 "Failed to find or create a backup directory",
					 "Check file permissions in /var/lib/nyx, ~/.nyx, or /tmp directories");
	return -1;
}

/**
 * Performs atomic file write using temporary file and rename
 */
static int atomic_file_write(const char *path, const char *content)
{
	char tmp_path[PATH_MAX];
	int ret = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, getpid());
	if (ret >= (int)sizeof(tmp_path))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Temporary path too long",
						 "Use a shorter interface name or install in a directory with a shorter path");
		return PH_ERR_FILE_IO;
	}

	mode_t old_mask = umask(0077); // Ensure secure permissions

	FILE *fp = fopen(tmp_path, "w");
	if (!fp)
	{
		umask(old_mask);
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Failed to create temporary file",
						 "Check directory permissions and available disk space");
		return PH_ERR_FILE_IO;
	}

	if (fprintf(fp, "%s", content) < 0)
	{
		int err = errno;
		fclose(fp);
		unlink(tmp_path);
		umask(old_mask);
		char msg[256];
		snprintf(msg, sizeof(msg), "Failed to write content: %s", strerror(err));
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 msg, "Check disk space and permissions");
		return PH_ERR_FILE_IO;
	}

	// Ensure data is written to disk
	if (fflush(fp) != 0 || fsync(fileno(fp)) != 0)
	{
		int err = errno;
		fclose(fp);
		unlink(tmp_path);
		umask(old_mask);
		char msg[256];
		snprintf(msg, sizeof(msg), "Failed to flush data: %s", strerror(err));
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 msg, "Check disk space and permissions");
		return PH_ERR_FILE_IO;
	}

	fclose(fp);

	// Atomic rename
	if (rename(tmp_path, path) != 0)
	{
		unlink(tmp_path);
		umask(old_mask);
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Failed to rename temporary file to target location",
						 "Check directory permissions and available disk space");
		return PH_ERR_FILE_IO;
	}

	umask(old_mask);
	return 0;
}

/**
 * Saves original MAC address for later restoration
 */
int ph_macspoof_save_original_mac(const char *iface, const char *mac)
{
	if (!nyx_iface_is_valid(iface))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "Invalid interface name",
						 "Verify the interface exists using 'ip link' or run with '-l' to list interfaces");
		return PH_ERR_NO_IFACE;
	}
	if (!nyx_iface_is_valid_mac(mac))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid MAC address format",
						 "MAC address must be in format XX:XX:XX:XX:XX:XX with valid hex digits");
		return PH_ERR_INVALID_MAC;
	}

	char backup_dir[PATH_MAX];
	if (create_backup_directory(backup_dir, sizeof(backup_dir)) != 0)
	{
		// Error already set by create_backup_directory
		return PH_ERR_FILE_IO;
	}

	char backup_path[PATH_MAX];
	int ret = snprintf(backup_path, sizeof(backup_path), "%s/%s%s%s",
					   backup_dir, BACKUP_FILE_PREFIX, iface, BACKUP_FILE_SUFFIX);
	if (ret >= (int)sizeof(backup_path))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Backup path too long for interface",
						 "Use a shorter interface name");
		return PH_ERR_FILE_IO;
	}

	// Check if backup already exists
	if (access(backup_path, F_OK) == 0)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_ALREADY_SAVED, NYX_ERROR_SEV_WARNING,
						 "Original MAC already saved for interface",
						 "This is just a warning, operation will continue");
		return PH_ERR_ALREADY_SAVED;
	}

	// Create backup content with metadata
	char backup_content[512];
	time_t now = time(NULL);
	ret = snprintf(backup_content, sizeof(backup_content),
				   "# Phobos MAC Spoof Backup - DO NOT EDIT\n"
				   "INTERFACE=%s\n"
				   "ORIGINAL_MAC=%s\n"
				   "TIMESTAMP=%ld\n"
				   "PID=%d\n"
				   "VERSION=1.0\n",
				   iface, mac, (long)now, getpid());

	if (ret >= (int)sizeof(backup_content))
	{
		return NYX_ERROR_SET(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO,
							 "Backup content too long");
	}

	if (atomic_file_write(backup_path, backup_content) != 0)
	{
		// Error already set by atomic_file_write
		return PH_ERR_FILE_IO;
	}

	nyx_log(NYX_LOG_INFO, "Original MAC %s saved for %s at %s", mac, iface, backup_path);
	return PH_SUCCESS;
}

/**
 * Loads original MAC address from backup
 */
int ph_macspoof_load_original_mac(const char *iface, char *buffer, size_t len)
{
	if (!iface || !buffer || len < PH_MAX_MAC_LEN)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid parameters for loading MAC",
						 "Ensure valid interface name and buffer");
		return PH_ERR_INVALID_MAC;
	}

	char backup_dir[PATH_MAX];
	if (create_backup_directory(backup_dir, sizeof(backup_dir)) != 0)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Failed to locate backup directory",
						 "Run this operation with the same privileges used when changing the MAC");
		return PH_ERR_FILE_IO;
	}

	char backup_path[PATH_MAX];
	int ret = snprintf(backup_path, sizeof(backup_path), "%s/%s%s%s",
					   backup_dir, BACKUP_FILE_PREFIX, iface, BACKUP_FILE_SUFFIX);
	if (ret >= (int)sizeof(backup_path))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Backup path too long",
						 "Use a shorter interface name");
		return PH_ERR_FILE_IO;
	}

	FILE *fp = fopen(backup_path, "r");
	if (!fp)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NOT_FOUND, NYX_ERROR_SEV_ERROR,
						 "No saved original MAC for interface",
						 "Either MAC was never changed or you're running as a different user than when it was changed");
		return PH_ERR_NOT_FOUND;
	}

	char line[256];
	char saved_mac[PH_MAX_MAC_LEN] = {0};
	int found_mac = 0;

	while (fgets(line, sizeof(line), fp))
	{
		// Remove trailing newline
		line[strcspn(line, "\n\r")] = '\0';

		// Skip comments and empty lines
		if (line[0] == '#' || line[0] == '\0')
			continue;

		// Parse ORIGINAL_MAC= line
		if (strncmp(line, "ORIGINAL_MAC=", 13) == 0)
		{
			const char *mac_value = line + 13;
			size_t value_len = strlen(mac_value);

			// Verify length is reasonable before copying
			if (value_len == 0 || value_len > PH_MAX_MAC_LEN - 1)
			{
				NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
								 "Invalid MAC address length in backup file",
								 "Backup file may be corrupted");
				continue;
			}

			memcpy(saved_mac, mac_value, value_len + 1); // +1 to include null terminator
			found_mac = 1;
			break;
		}
	}

	fclose(fp);

	if (!found_mac)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Malformed backup file for interface",
						 "The backup file may be corrupted, try manually setting a MAC address instead");
		return PH_ERR_FILE_IO;
	}

	if (!nyx_iface_is_valid_mac(saved_mac))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid MAC in backup file",
						 "The backup file contains an invalid MAC address, try manually setting a MAC address instead");
		return PH_ERR_INVALID_MAC;
	}

	strncpy(buffer, saved_mac, len - 1);
	buffer[len - 1] = '\0';

	nyx_log(NYX_LOG_VERBOSE, "Loaded original MAC %s for %s", buffer, iface);
	return PH_SUCCESS;
}

/**
 * Deletes original MAC backup file
 */
void ph_macspoof_delete_original_mac(const char *iface)
{
	if (!iface)
		return;

	char backup_dir[PATH_MAX];
	if (create_backup_directory(backup_dir, sizeof(backup_dir)) != 0)
	{
		return;
	}

	char backup_path[PATH_MAX];
	int ret = snprintf(backup_path, sizeof(backup_path), "%s/%s%s%s",
					   backup_dir, BACKUP_FILE_PREFIX, iface, BACKUP_FILE_SUFFIX);
	if (ret >= (int)sizeof(backup_path))
	{
		return;
	}

	if (unlink(backup_path) == 0)
	{
		nyx_log(NYX_LOG_INFO, "Deleted backup for %s", iface);
	}
	else
	{
		nyx_log(NYX_LOG_ERROR, "Failed to delete backup for %s: %s",
				iface, strerror(errno));
	}
}

/**
 * Brief delay to allow interface state to settle
 */
static void interface_settle_delay(void)
{
	usleep(INTERFACE_SETTLE_MS * 1000);
}

/**
 * Verifies MAC change was successful
 */
static int verify_mac_change(const char *iface, const char *expected_mac)
{
	char current_mac[PH_MAX_MAC_LEN] = {0};

	interface_settle_delay(); // Allow time for change to take effect

	if (ph_macspoof_get_current_mac(iface, current_mac, sizeof(current_mac)) != PH_SUCCESS)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_WARNING,
						 "Failed to read MAC for verification",
						 "MAC may have been changed but couldn't be verified");
		return 0;
	}

	if (strcasecmp(current_mac, expected_mac) == 0)
	{
		return 1;
	}

	NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_ERROR,
					 "MAC verification failed: expected different MAC than received",
					 "The system may have hardware restrictions preventing MAC address changes");
	return 0;
}

/**
 * Changes MAC address using the interface utility
 */
int ph_macspoof_change_mac(const char *iface, const char *mac)
{
	if (!iface || !mac)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Missing interface name or MAC address",
						 "Both interface name and MAC address must be provided");
		return PH_ERR_INVALID_MAC;
	}

	if (!nyx_iface_is_valid(iface))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "Interface validation failed",
						 "Verify the interface exists and is accessible");
		return PH_ERR_NO_IFACE;
	}

	if (!nyx_iface_is_valid_mac(mac))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid MAC address format",
						 "MAC address must be in format XX:XX:XX:XX:XX:XX with valid hex digits");
		return PH_ERR_INVALID_MAC;
	}

	// Save original MAC if not already saved
	char current_mac[PH_MAX_MAC_LEN] = {0};
	int get_result = ph_macspoof_get_current_mac(iface, current_mac, sizeof(current_mac));

	if (get_result == PH_SUCCESS)
	{
		int save_result = ph_macspoof_save_original_mac(iface, current_mac);
		if (save_result != PH_SUCCESS && save_result != PH_ERR_ALREADY_SAVED)
		{
			nyx_log(NYX_LOG_WARN, "Failed to save original MAC: %d", save_result);
		}
	}

	// Remember if interface was up
	int was_up = nyx_iface_is_up(iface);

	// Bring interface down if it's up
	if (was_up)
	{
		if (nyx_iface_set_status(iface, 0) != NYX_IFACE_SUCCESS)
		{
			NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_ERROR,
							 "Failed to bring interface down",
							 "You may need elevated privileges (sudo) or the interface might be in use");
			return PH_ERR_IOCTL;
		}
		nyx_log(NYX_LOG_INFO, "Interface %s brought down", iface);
		interface_settle_delay();
	}

	int ret = -1;
	pid_t pid = fork();
	if (pid == 0)
	{
		// Child process - execute the command
		execl("/sbin/ip", "ip", "link", "set", "dev", iface, "address", mac, NULL);
		// If we get here, exec failed
		exit(127);
	}
	else if (pid > 0)
	{
		// Parent process - wait for child
		int status;
		if (waitpid(pid, &status, 0) != -1)
		{
			ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		}
	}
	if (ret != 0)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_ERROR,
						 "Failed to set MAC address for interface",
						 "This may be due to hardware restrictions, driver limitations, or insufficient permissions");

		// Restore interface state if it was up
		if (was_up)
		{
			nyx_iface_set_status(iface, 1);
		}
		return PH_ERR_IOCTL;
	}

	nyx_log(NYX_LOG_INFO, "MAC address set successfully for %s", iface);

	// Restore interface state if it was up
	if (was_up)
	{
		interface_settle_delay();
		if (nyx_iface_set_status(iface, 1) != NYX_IFACE_SUCCESS)
		{
			nyx_log(NYX_LOG_ERROR, "Failed to restore interface %s state", iface);
			NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_ERROR,
							 "Failed to restore interface state after MAC change",
							 "Interface may be left in DOWN state; bring it up manually: ip link set <iface> up");
			return PH_ERR_IOCTL;
		}
		nyx_log(NYX_LOG_INFO, "Interface %s restored to up state", iface);
	}

	// Verify the change was successful
	if (!verify_mac_change(iface, mac))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_IOCTL, NYX_ERROR_SEV_ERROR,
						 "MAC address change couldn't be verified",
						 "MAC may have been changed but couldn't be confirmed - check with 'ip link'");
		return PH_ERR_IOCTL;
	}

	return PH_SUCCESS;
}

/**
 * Restores original MAC address from backup
 */
int ph_macspoof_restore_mac(const char *iface)
{
	if (!iface)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "No interface specified for MAC restoration",
						 "Provide a valid network interface name");
		return PH_ERR_NO_IFACE;
	}

	char original_mac[PH_MAX_MAC_LEN] = {0};
	int load_result = ph_macspoof_load_original_mac(iface, original_mac, sizeof(original_mac));

	if (load_result != PH_SUCCESS)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, load_result, NYX_ERROR_SEV_ERROR,
						 "Failed to load original MAC for interface",
						 "Try running with the same privileges used when changing the MAC, or check if backup exists");
		return load_result;
	}

	nyx_log(NYX_LOG_INFO, "Restoring original MAC %s for %s", original_mac, iface);

	int change_result = ph_macspoof_change_mac(iface, original_mac);
	if (change_result == PH_SUCCESS)
	{
		ph_macspoof_delete_original_mac(iface);
		nyx_log(NYX_LOG_INFO, "Successfully restored original MAC for %s", iface);
	}

	return change_result;
}

/**
 * Generates a random MAC address with realistic OUI
 */
int ph_macspoof_generate_random_mac(char *buffer, size_t len)
{
	if (!buffer || len < PH_MAX_MAC_LEN)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid buffer for MAC generation",
						 "Provide a buffer of at least 18 bytes");
		return PH_ERR_INVALID_MAC;
	}

	unsigned char entropy[4];
	FILE *urand = fopen("/dev/urandom", "r");
	if (!urand)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Failed to open /dev/urandom",
						 "Ensure /dev/urandom is accessible");
		return PH_ERR_FILE_IO;
	}
	if (fread(entropy, 1, sizeof(entropy), urand) != sizeof(entropy))
	{
		fclose(urand);
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_FILE_IO, NYX_ERROR_SEV_ERROR,
						 "Failed to read from /dev/urandom",
						 "System entropy source unavailable");
		return PH_ERR_FILE_IO;
	}
	fclose(urand);

	size_t oui_count = sizeof(COMMON_OUIS) / sizeof(COMMON_OUIS[0]);
	const char *oui = COMMON_OUIS[entropy[0] % oui_count];

	unsigned char byte4 = entropy[1];
	unsigned char byte5 = entropy[2];
	unsigned char byte6 = entropy[3];

	/* Unicast (clear multicast bit), locally administered (set LAA bit) */
	byte4 = (byte4 & 0xFE) | 0x02;

	int ret = snprintf(buffer, len, "%s:%02X:%02X:%02X", oui, byte4, byte5, byte6);
	if (ret >= (int)len)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Buffer too small for generated MAC",
						 "Provide a buffer of at least 18 bytes");
		return PH_ERR_INVALID_MAC;
	}

	return PH_SUCCESS;
}

/**
 * Lists all available network interfaces with their MAC addresses to stdout
 */
int ph_macspoof_list_interfaces_stdout(void)
{
	return nyx_iface_print_details();
}

/**
 * Shows current MAC address of an interface (CLI wrapper)
 */
int ph_macspoof_show_mac(const char *iface)
{
	char mac_addr[PH_MAX_MAC_LEN];
	int result;

	if (!iface || !*iface)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "Invalid interface name",
						 "Provide a valid network interface name using -i option");
		return PH_ERR_NO_IFACE;
	}

	result = ph_macspoof_get_current_mac(iface, mac_addr, sizeof(mac_addr));
	if (result != PH_SUCCESS)
	{
		// Error already set by ph_macspoof_get_current_mac
		return result;
	}

	int is_up = nyx_iface_is_up(iface);
	printf("Interface: %s [%s]\n", iface, is_up ? "UP" : "DOWN");
	printf("Current MAC: %s\n", mac_addr);

	return PH_SUCCESS;
}

/**
 * Applies a random MAC address to an interface (CLI wrapper)
 */
int ph_macspoof_random_mac(const char *iface)
{
	char mac_addr[PH_MAX_MAC_LEN];
	char old_mac[PH_MAX_MAC_LEN];
	int result;

	if (!iface || !*iface)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "Invalid interface name",
						 "Provide a valid network interface name using -i option");
		return PH_ERR_NO_IFACE;
	}

	// Save the original MAC first
	result = ph_macspoof_get_current_mac(iface, old_mac, sizeof(old_mac));
	if (result == PH_SUCCESS)
	{
		ph_macspoof_save_original_mac(iface, old_mac);
	}

	// Generate a random MAC
	result = ph_macspoof_generate_random_mac(mac_addr, sizeof(mac_addr));
	if (result != PH_SUCCESS)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, result, NYX_ERROR_SEV_ERROR,
						 "Failed to generate random MAC address",
						 "This is likely a software error - please report this issue");
		return result;
	}

	// Apply the new MAC
	result = ph_macspoof_change_mac(iface, mac_addr);
	if (result != PH_SUCCESS)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, result, NYX_ERROR_SEV_ERROR,
						 "Failed to apply random MAC to interface",
						 "Try running with sudo privileges");
		return result;
	}

	nyx_log(NYX_LOG_SUCCESS, "Successfully applied random MAC %s to interface %s",
			mac_addr, iface);
	return PH_SUCCESS;
}

/**
 * Applies a custom MAC address to an interface (CLI wrapper)
 */
int ph_macspoof_custom_mac(const char *iface, const char *mac)
{
	char old_mac[PH_MAX_MAC_LEN];
	int result;

	if (!iface || !*iface)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "Invalid interface name",
						 "Provide a valid network interface name using -i option");
		return PH_ERR_NO_IFACE;
	}

	if (!nyx_iface_is_valid_mac(mac))
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
						 "Invalid MAC address format",
						 "MAC address must be in format XX:XX:XX:XX:XX:XX with valid hex digits");
		return PH_ERR_INVALID_MAC;
	}

	// Save the original MAC first
	result = ph_macspoof_get_current_mac(iface, old_mac, sizeof(old_mac));
	if (result == PH_SUCCESS)
	{
		ph_macspoof_save_original_mac(iface, old_mac);
	}

	// Apply the new MAC
	result = ph_macspoof_change_mac(iface, mac);
	if (result != PH_SUCCESS)
	{
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, result, NYX_ERROR_SEV_ERROR,
						 "Failed to apply custom MAC to interface",
						 "Try running with sudo privileges");
		return result;
	}

	nyx_log(NYX_LOG_SUCCESS, "Successfully applied custom MAC %s to interface %s",
			mac, iface);
	return PH_SUCCESS;
}
