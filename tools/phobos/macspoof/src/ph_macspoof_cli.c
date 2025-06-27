/**
 * @file ph_macspoof_cli.c
 * @brief Command-line interface for MAC spoofing tool
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ph_macspoof_api.h"
#include "nyx_logger.h"
#include "nyx_cli.h"
#include "nyx_error.h"

#define VERSION "1.0"
#define TOOL_NAME "macspoof"
#define NYX_MODULE "phobos"

// Verbose flag for detailed error reporting
static int verbose_mode = 0;

/**
 * Command-line options for MAC spoofing tool
 */
static const nyx_cli_opt_def_t cli_options[] = {
	{'i', "interface", "IFACE", "Interface to operate on", NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_OPTIONAL},
	{'m', "mac", "MAC", "Set custom MAC address", NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_OPTIONAL},
	{'r', "random", NULL, "Set random MAC address", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
	{'s', "show", NULL, "Show current MAC address", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
	{'l', "list", NULL, "List available interfaces", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
	{'R', "restore", NULL, "Restore original MAC address", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
	{'v', "version", NULL, "Display version information", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
	{'d', "debug", NULL, "Enable debug output", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
	{'h', "help", NULL, "Show help information", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL}};

/**
 * Tool description for help text
 */
static const char *tool_description =
	"MAC address spoofing tool for changing, randomizing, or restoring MAC addresses\n"
	"on network interfaces. Part of the Nyx Offensive Security Framework.";

/**
 * Entry point for MAC spoofing command-line tool
 */
int main(int argc, char *argv[])
{
	// Configure banner
	nyx_cli_banner_config_t banner = {
		.module = NYX_MODULE,
		.tool_name = TOOL_NAME,
		.version = VERSION,
		.author = "Neur0sis (2025)",
		.primary_color = NYX_CLI_COLOR_CYAN,
		.secondary_color = NYX_CLI_COLOR_YELLOW,
		.style = NYX_CLI_BANNER_ASCII_ART};

	// Display banner
	nyx_cli_print_banner(&banner);

	// Parse command-line arguments
	const size_t opt_count = sizeof(cli_options) / sizeof(cli_options[0]);
	nyx_cli_result_t *result = nyx_cli_parse(argc, argv, cli_options, opt_count);

	if (!result)
	{
		nyx_log(NYX_LOG_ERROR, "Failed to parse command-line arguments");
		return 1;
	}

	// Check for parsing errors
	if (result->error)
	{
		nyx_log(NYX_LOG_ERROR, "Error parsing arguments: %s", result->error_msg);
		nyx_cli_print_usage(argv[0], cli_options, opt_count, tool_description);
		nyx_cli_free_result(result);
		return 1;
	}

	// Check for verbose/debug mode
	if (nyx_cli_has_option(result, 'd'))
	{
		verbose_mode = 1;
	}

	// Show help if requested or if no operation is specified
	if (nyx_cli_has_option(result, 'h') ||
		(!nyx_cli_has_option(result, 's') &&
		 !nyx_cli_has_option(result, 'r') &&
		 !nyx_cli_has_option(result, 'm') &&
		 !nyx_cli_has_option(result, 'R') &&
		 !nyx_cli_has_option(result, 'l') &&
		 !nyx_cli_has_option(result, 'v') &&
		 !nyx_cli_has_option(result, 'd')))
	{
		nyx_cli_print_usage(argv[0], cli_options, opt_count, tool_description);
		nyx_cli_free_result(result);
		return 0;
	}

	// Show version info
	if (nyx_cli_has_option(result, 'v'))
	{
		printf("Nyx %s v%s\n", TOOL_NAME, VERSION);
		nyx_cli_free_result(result);
		return 0;
	}

	// List interfaces
	if (nyx_cli_has_option(result, 'l'))
	{
		nyx_log(NYX_LOG_INFO, "Listing available interfaces...");
		int ret = ph_macspoof_list_interfaces_stdout();
		if (ret != PH_SUCCESS)
		{
			// Use the new error logging function with verbose flag
			nyx_error_log(NYX_LOG_ERROR, verbose_mode);
		}
		nyx_cli_free_result(result);
		return ret;
	}

	// All other operations need an interface
	const char *iface = nyx_cli_get_option(result, 'i');
	if (!iface)
	{
		// Use extended error setting
		NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
						 "No interface specified",
						 "Use -i or --interface to specify a network interface");
		nyx_error_log(NYX_LOG_ERROR, verbose_mode);
		nyx_cli_free_result(result);
		return 1;
	}

	int ret = PH_SUCCESS;

	// Show interface MAC
	if (nyx_cli_has_option(result, 's'))
	{
		nyx_log(NYX_LOG_INFO, "Retrieving current MAC address for interface: %s", iface);
		ret = ph_macspoof_show_mac(iface);
	}
	// Restore original MAC
	else if (nyx_cli_has_option(result, 'R'))
	{
		nyx_log(NYX_LOG_INFO, "Attempting to restore original MAC address for: %s", iface);
		ret = ph_macspoof_restore_mac(iface);
	}
	// Random MAC
	else if (nyx_cli_has_option(result, 'r'))
	{
		nyx_log(NYX_LOG_INFO, "Generating and applying a random MAC address to: %s", iface);
		ret = ph_macspoof_random_mac(iface);
	}
	// Custom MAC
	else if (nyx_cli_has_option(result, 'm'))
	{
		const char *mac = nyx_cli_get_option(result, 'm');
		if (!mac)
		{
			NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
							 "No MAC address provided with -m option",
							 "Specify a MAC address in XX:XX:XX:XX:XX:XX format");
			nyx_error_log(NYX_LOG_ERROR, verbose_mode);
			nyx_cli_free_result(result);
			return 1;
		}

		// Validate MAC format
		if (!nyx_cli_validate_mac(mac))
		{
			NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC, NYX_ERROR_SEV_ERROR,
							 "Invalid MAC address format",
							 "MAC address must be in XX:XX:XX:XX:XX:XX format with valid hexadecimal digits");
			nyx_error_log(NYX_LOG_ERROR, verbose_mode);
			nyx_cli_free_result(result);
			return 1;
		}

		nyx_log(NYX_LOG_INFO, "Spoofing interface '%s' with custom MAC: %s", iface, mac);
		ret = ph_macspoof_custom_mac(iface, mac);
	}

	// Handle errors with the enhanced error system
	if (ret != PH_SUCCESS)
	{
		// Error already set by the module functions
		nyx_error_log(NYX_LOG_ERROR, verbose_mode);
	}

	nyx_cli_free_result(result);
	return (ret == PH_SUCCESS) ? 0 : 1;
}
