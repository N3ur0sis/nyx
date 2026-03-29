/**
 * @file nyx_cli.c
 * @brief Command-line interface utilities implementation
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <arpa/inet.h>

#include "nyx_cli.h"
#include "nyx_logger.h"

// ANSI color codes
static const char *ansi_colors[] = {
    [NYX_CLI_COLOR_NONE] = "\033[0m",     [NYX_CLI_COLOR_RED] = "\033[1;31m",
    [NYX_CLI_COLOR_GREEN] = "\033[1;32m", [NYX_CLI_COLOR_YELLOW] = "\033[1;33m",
    [NYX_CLI_COLOR_BLUE] = "\033[1;34m",  [NYX_CLI_COLOR_MAGENTA] = "\033[1;35m",
    [NYX_CLI_COLOR_CYAN] = "\033[1;36m",  [NYX_CLI_COLOR_WHITE] = "\033[1;37m"};

/**
 * Helper function to create a long_options array for getopt_long
 */
static struct option *create_long_options(const nyx_cli_opt_def_t *opt_defs, size_t opt_count,
                                          char **optstring)
{
    struct option *long_options = calloc(opt_count + 1, sizeof(struct option));
    if (!long_options) {
        return NULL;
    }

    // Calculate optstring length: each option needs 1-3 chars (letter + 0-2 colons)
    size_t optstring_len = (opt_count * 3) + 1;
    *optstring = calloc(optstring_len, sizeof(char));
    if (!*optstring) {
        free(long_options);
        return NULL;
    }

    size_t idx = 0;
    char *os_ptr = *optstring;

    for (size_t i = 0; i < opt_count; i++) {
        if (opt_defs[i].short_opt) {
            *os_ptr++ = opt_defs[i].short_opt;

            // Add : or :: for options with arguments
            switch (opt_defs[i].arg_type) {
            case NYX_CLI_ARG_REQUIRED:
                *os_ptr++ = ':';
                break;
            case NYX_CLI_ARG_OPTIONAL:
                *os_ptr++ = ':';
                *os_ptr++ = ':';
                break;
            case NYX_CLI_ARG_NONE:
                break;
            }
        }

        // Add to long options if long name exists
        if (opt_defs[i].long_opt) {
            long_options[idx].name = opt_defs[i].long_opt;

            switch (opt_defs[i].arg_type) {
            case NYX_CLI_ARG_NONE:
                long_options[idx].has_arg = no_argument;
                break;
            case NYX_CLI_ARG_REQUIRED:
                long_options[idx].has_arg = required_argument;
                break;
            case NYX_CLI_ARG_OPTIONAL:
                long_options[idx].has_arg = optional_argument;
                break;
            }

            long_options[idx].flag = NULL;
            long_options[idx].val = opt_defs[i].short_opt;
            idx++;
        }
    }

    // Ensure null termination
    *os_ptr = '\0';

    return long_options;
}

nyx_cli_result_t *nyx_cli_parse(int argc, char *argv[], const nyx_cli_opt_def_t *opt_defs,
                                size_t opt_count)
{
    if (!argv || !opt_defs || opt_count == 0) {
        return NULL;
    }

    // Allocate the result structure
    nyx_cli_result_t *result = calloc(1, sizeof(nyx_cli_result_t));
    if (!result) {
        return NULL;
    }

    result->options = calloc(opt_count, sizeof(nyx_cli_opt_result_t));
    if (!result->options) {
        free(result);
        return NULL;
    }
    result->option_count = opt_count;

    // Initialize all options with their short_opt value
    for (size_t i = 0; i < opt_count; i++) {
        result->options[i].short_opt = opt_defs[i].short_opt;
    }

    // Create getopt structures
    char *optstring = NULL;
    struct option *long_options = create_long_options(opt_defs, opt_count, &optstring);
    if (!long_options || !optstring) {
        if (long_options)
            free(long_options);
        if (optstring)
            free(optstring);
        free(result->options);
        free(result);
        return NULL;
    }

    // Reset getopt state
    optind = 0;

    // Parse arguments
    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
        if (opt == '?' || opt == ':') {
            // Error in argument parsing
            snprintf(result->error_msg, sizeof(result->error_msg),
                     "Invalid option or missing argument");
            result->error = 1;
            break;
        }

        // Find the corresponding option in our definitions
        int idx = NYX_CLI_FIND_OPT(opt_defs, opt_count, opt);
        if (idx < 0) {
            continue;
        }

        // Mark option as present (short_opt already initialized)
        result->options[idx].present = 1;

        // Store value if applicable
        if (opt_defs[idx].arg_type != NYX_CLI_ARG_NONE && optarg) {
            strncpy(result->options[idx].value, optarg, NYX_CLI_MAX_ARG_LEN - 1);
            result->options[idx].value[NYX_CLI_MAX_ARG_LEN - 1] = '\0';
        }
    }

    if (optind < argc) {
        result->extra_arg_count = (size_t)(argc - optind);
        result->extra_args = calloc(result->extra_arg_count, sizeof(char *));
        if (result->extra_args) {
            for (size_t i = 0; i < result->extra_arg_count; i++) {
                result->extra_args[i] = strdup(argv[optind + (int)i]);
            }
        } else {
            result->extra_arg_count = 0;
        }
    }

    // Clean up
    free(optstring);
    free(long_options);

    return result;
}

void nyx_cli_free_result(nyx_cli_result_t *result)
{
    if (!result) {
        return;
    }

    // Free options array
    if (result->options) {
        free(result->options);
    }

    // Free extra arguments
    if (result->extra_args) {
        for (size_t i = 0; i < result->extra_arg_count; i++) {
            free(result->extra_args[i]);
        }
        free(result->extra_args);
    }

    // Free the result structure itself
    free(result);
}

int nyx_cli_has_option(const nyx_cli_result_t *result, char short_opt)
{
    if (!result || !result->options) {
        return 0;
    }

    // Search for the option with matching short_opt that is present
    for (size_t i = 0; i < result->option_count; i++) {
        if (result->options[i].short_opt == short_opt && result->options[i].present) {
            return 1;
        }
    }

    return 0;
}

const char *nyx_cli_get_option(const nyx_cli_result_t *result, char short_opt)
{
    if (!result || !result->options) {
        return NULL;
    }

    // Search for the option with matching short_opt that is present
    for (size_t i = 0; i < result->option_count; i++) {
        if (result->options[i].short_opt == short_opt && result->options[i].present) {
            // Return NULL for empty strings to distinguish absence
            return result->options[i].value[0] ? result->options[i].value : NULL;
        }
    }

    return NULL;
}

void nyx_cli_print_usage(const char *program_name, const nyx_cli_opt_def_t *opt_defs,
                         size_t opt_count, const char *description)
{
    printf("\nUsage: %s [OPTIONS]\n", program_name);

    if (description) {
        printf("\n%s\n", description);
    }

    printf("\nOptions:\n");

    for (size_t i = 0; i < opt_count; i++) {
        // Skip hidden options
        if (opt_defs[i].flags & NYX_CLI_FLAG_HIDDEN) {
            continue;
        }

        // Format short and long options
        printf("  ");
        if (opt_defs[i].short_opt) {
            printf("-%c", opt_defs[i].short_opt);
        } else {
            printf("  ");
        }

        if (opt_defs[i].short_opt && opt_defs[i].long_opt) {
            printf(", ");
        } else if (opt_defs[i].short_opt) {
            printf("  ");
        }

        if (opt_defs[i].long_opt) {
            printf("--%s", opt_defs[i].long_opt);
        }

        // Format argument
        if (opt_defs[i].arg_type != NYX_CLI_ARG_NONE) {
            if (opt_defs[i].arg_name) {
                if (opt_defs[i].arg_type == NYX_CLI_ARG_REQUIRED) {
                    printf(" <%s>", opt_defs[i].arg_name);
                } else {
                    printf(" [%s]", opt_defs[i].arg_name);
                }
            } else {
                if (opt_defs[i].arg_type == NYX_CLI_ARG_REQUIRED) {
                    printf(" <arg>");
                } else {
                    printf(" [arg]");
                }
            }
        }

        // Print help text with proper alignment
        if (opt_defs[i].help_text) {
            printf("\t%s", opt_defs[i].help_text);
        }

        // Mark required options
        if (opt_defs[i].flags & NYX_CLI_FLAG_REQUIRED) {
            printf(" [REQUIRED]");
        }

        printf("\n");
    }
    printf("\n");
}

int nyx_cli_validate_required(const nyx_cli_result_t *result, const nyx_cli_opt_def_t *opt_defs,
                              size_t opt_count)
{
    if (!result || !opt_defs) {
        return 0;
    }

    int missing_required = 0;

    for (size_t i = 0; i < opt_count; i++) {
        if (opt_defs[i].flags & NYX_CLI_FLAG_REQUIRED) {
            int idx = NYX_CLI_FIND_OPT(opt_defs, opt_count, opt_defs[i].short_opt);
            if (idx >= 0 && !result->options[idx].present) {
                nyx_log(NYX_LOG_ERROR, "Required option -%c%s%s is missing", opt_defs[i].short_opt,
                        opt_defs[i].long_opt ? "/--" : "",
                        opt_defs[i].long_opt ? opt_defs[i].long_opt : "");
                missing_required = 1;
            }
        }
    }

    return !missing_required;
}

void nyx_cli_print_banner(const nyx_cli_banner_config_t *config)
{
    if (!config || config->style == NYX_CLI_BANNER_NONE) {
        return;
    }

    const char *primary_color = ansi_colors[config->primary_color];
    const char *secondary_color = ansi_colors[config->secondary_color];
    const char *reset = ansi_colors[NYX_CLI_COLOR_NONE];

    // Start with a blank line
    printf("\n");

    if (config->style == NYX_CLI_BANNER_ASCII_ART) {
        // ASCII art banner - customize for each tool
        if (strcasecmp(config->module, "phobos") == 0) {
            printf("%s", primary_color);
            printf("  ██████╗ ██╗  ██╗ ██████╗ ██████╗  ██████╗ ███████╗\n");
            printf("  ██╔══██╗██║  ██║██╔═══██╗██╔══██╗██╔═══██╗██╔════╝\n");
            printf("  ██████╔╝███████║██║   ██║██████╔╝██║   ██║███████╗\n");
            printf("  ██╔═══╝ ██╔══██║██║   ██║██╔══██╗██║   ██║╚════██║\n");
            printf("  ██║     ██║  ██║╚██████╔╝██████╔╝╚██████╔╝███████║\n");
            printf("  ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚══════╝ v%s\n",
                   config->version ? config->version : "?.?");
        } else if (strcasecmp(config->module, "nyx") == 0) {
            printf("%s", primary_color);
            printf("  ███╗   ██╗██╗   ██╗██╗  ██╗\n");
            printf("  ████╗  ██║╚██╗ ██╔╝╚██╗██╔╝\n");
            printf("  ██╔██╗ ██║ ╚████╔╝  ╚███╔╝ \n");
            printf("  ██║╚██╗██║  ╚██╔╝   ██╔██╗ \n");
            printf("  ██║ ╚████║   ██║   ██╔╝ ██╗\n");
            printf("  ╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝ v%s\n",
                   config->version ? config->version : "?.?");
        } else {
            // Generic banner for any tool
            printf("%s", primary_color);
            printf("  ███╗   ██╗██╗   ██╗██╗  ██╗\n");
            printf("  ████╗  ██║╚██╗ ██╔╝╚██╗██╔╝     %s\n",
                   config->tool_name ? config->tool_name : "");
            printf("  ██╔██╗ ██║ ╚████╔╝  ╚███╔╝      v%s\n",
                   config->version ? config->version : "?.?");
            printf("  ██║╚██╗██║  ╚██╔╝   ██╔██╗ \n");
            printf("  ██║ ╚████║   ██║   ██╔╝ ██╗\n");
            printf("  ╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝\n");
        }

        // Print subtitle and author
        printf("%s          NYX OFFENSIVE SUITE MODULE - %s%s\n", secondary_color,
               config->tool_name ? config->tool_name : "Unknown Tool", reset);
        if (config->author) {
            printf("%s          Author: %s%s\n", secondary_color, config->author, reset);
        }
    } else {
        // Simple text banner
        printf("%s=== NYX %s", primary_color, config->tool_name ? config->tool_name : "Tool");
        if (config->version) {
            printf(" v%s", config->version);
        }
        printf(" ===%s\n", reset);
        if (config->author) {
            printf("%sBy: %s%s\n", secondary_color, config->author, reset);
        }
    }

    // End with a blank line
    printf("\n");
}

int nyx_cli_validate_ipv4(const char *ip)
{
    struct in_addr addr;
    return ip && inet_pton(AF_INET, ip, &addr) == 1;
}

int nyx_cli_validate_mac(const char *mac)
{
    if (!mac) {
        return 0;
    }

    // Check length
    size_t len = strlen(mac);
    if (len != 17) { // XX:XX:XX:XX:XX:XX = 17 chars
        return 0;
    }

    // Check format (XX:XX:XX:XX:XX:XX)
    for (int i = 0; i < 17; i++) {
        if ((i + 1) % 3 == 0) {
            // Positions 2, 5, 8, 11, 14 should be colons
            if (mac[i] != ':') {
                return 0;
            }
        } else {
            // All other positions should be hex digits
            if (!isxdigit((unsigned char)mac[i])) {
                return 0;
            }
        }
    }

    return 1;
}

int nyx_cli_validate_cidr(const char *cidr)
{
    if (!cidr) {
        return 0;
    }

    const char *slash = strchr(cidr, '/');
    if (!slash) {
        return 0;
    }

    char ip[16];
    size_t ip_len = (size_t)(slash - cidr);
    if (ip_len >= sizeof(ip)) {
        return 0;
    }
    memcpy(ip, cidr, ip_len);
    ip[ip_len] = '\0';

    // Validate IP
    if (!nyx_cli_validate_ipv4(ip)) {
        return 0;
    }

    // Validate prefix length
    const char *prefix_str = slash + 1;
    char *endptr;
    long prefix = strtol(prefix_str, &endptr, 10);
    if (*endptr != '\0' || prefix < 0 || prefix > 32) {
        return 0;
    }

    return 1;
}