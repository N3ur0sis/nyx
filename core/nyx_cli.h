/**
 * @file nyx_cli.h
 * @brief Command-line interface utilities for Nyx framework
 * @author Neur0sis (2025)
 *
 * This module provides standardized CLI handling for Nyx tools, including
 * argument parsing, help formatting, banner display, and input validation.
 */

#ifndef NYX_CLI_H
#define NYX_CLI_H

#include <stddef.h>

/**
 * Maximum length of CLI argument values
 */
#define NYX_CLI_MAX_ARG_LEN 256

/**
 * Option flag types
 */
#define NYX_CLI_FLAG_NONE      0x00  /**< No flags */
#define NYX_CLI_FLAG_REQUIRED  0x01  /**< Option is required */
#define NYX_CLI_FLAG_OPTIONAL  0x02  /**< Option is optional */
#define NYX_CLI_FLAG_HIDDEN    0x04  /**< Don't show in help text */

/**
 * Option argument types
 */
typedef enum {
    NYX_CLI_ARG_NONE,         /**< No argument expected (boolean flag) */
    NYX_CLI_ARG_REQUIRED,     /**< Argument is required */
    NYX_CLI_ARG_OPTIONAL      /**< Argument is optional */
} nyx_cli_arg_type_t;

/**
 * CLI option definition structure
 */
typedef struct {
    char short_opt;              /**< Short option character (e.g. 'h' for -h) */
    const char *long_opt;        /**< Long option name (e.g. "help" for --help) */
    const char *arg_name;        /**< Argument name for help text (e.g. "FILE") */
    const char *help_text;       /**< Help text description */
    nyx_cli_arg_type_t arg_type; /**< Argument requirement type */
    int flags;                   /**< Option flags */
} nyx_cli_opt_def_t;

/**
 * CLI parsing result for a single option
 */
typedef struct {
    int present;                /**< Whether option was provided on command line */
    char short_opt;            /**< Short option character this corresponds to */
    char value[NYX_CLI_MAX_ARG_LEN]; /**< Option value (if any) */
} nyx_cli_opt_result_t;

/**
 * CLI parsing results structure
 */
typedef struct {
    nyx_cli_opt_result_t *options; /**< Array of parsed options */
    size_t option_count;          /**< Number of options in the array */
    char **extra_args;            /**< Extra non-option arguments */
    size_t extra_arg_count;       /**< Number of extra arguments */
    int error;                    /**< Error code (0 = no error) */
    char error_msg[256];          /**< Error message if error != 0 */
} nyx_cli_result_t;

/**
 * Color options for banner and text
 */
typedef enum {
    NYX_CLI_COLOR_NONE,    /**< No color */
    NYX_CLI_COLOR_RED,     /**< Red text */
    NYX_CLI_COLOR_GREEN,   /**< Green text */
    NYX_CLI_COLOR_YELLOW,  /**< Yellow text */
    NYX_CLI_COLOR_BLUE,    /**< Blue text */
    NYX_CLI_COLOR_MAGENTA, /**< Magenta text */
    NYX_CLI_COLOR_CYAN,    /**< Cyan text */
    NYX_CLI_COLOR_WHITE    /**< White text */
} nyx_cli_color_t;

/**
 * Banner style options
 */
typedef enum {
    NYX_CLI_BANNER_NONE,     /**< No banner */
    NYX_CLI_BANNER_SIMPLE,   /**< Simple text banner */
    NYX_CLI_BANNER_ASCII_ART /**< ASCII art banner */
} nyx_cli_banner_style_t;

/**
 * Banner configuration
 */
typedef struct {
	const char *module;          /**< Nyx Module name (e.g. "phobos") */
    const char *tool_name;        /**< Tool name */
    const char *version;          /**< Version string */
    const char *author;           /**< Author name */
    nyx_cli_color_t primary_color;   /**< Primary color for banner */
    nyx_cli_color_t secondary_color; /**< Secondary color for text */
    nyx_cli_banner_style_t style;    /**< Banner style */
} nyx_cli_banner_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parses command-line arguments according to the provided option definitions
 *
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @param opt_defs Array of option definitions
 * @param opt_count Number of options in the array
 * @return Parsed results (caller must free with nyx_cli_free_result)
 */
nyx_cli_result_t* nyx_cli_parse(int argc, char *argv[], 
                                const nyx_cli_opt_def_t *opt_defs, 
                                size_t opt_count);

/**
 * Frees resources allocated for CLI parsing results
 *
 * @param result Result structure to free
 */
void nyx_cli_free_result(nyx_cli_result_t *result);

/**
 * Checks if an option is present in the parsed results
 *
 * @param result Parsed CLI results
 * @param short_opt Short option character to check
 * @return 1 if option is present, 0 otherwise
 */
int nyx_cli_has_option(const nyx_cli_result_t *result, char short_opt);

/**
 * Gets option value from parsed results
 *
 * @param result Parsed CLI results
 * @param short_opt Short option character to get value for
 * @return Option value string, or NULL if not present
 */
const char* nyx_cli_get_option(const nyx_cli_result_t *result, char short_opt);

/**
 * Prints formatted usage text based on option definitions
 *
 * @param program_name Name of the program
 * @param opt_defs Array of option definitions
 * @param opt_count Number of options in the array 
 * @param description Program description
 */
void nyx_cli_print_usage(const char *program_name, 
                         const nyx_cli_opt_def_t *opt_defs, 
                         size_t opt_count,
                         const char *description);

/**
 * Validates required options and reports any missing ones
 *
 * @param result Parsed CLI results
 * @param opt_defs Array of option definitions
 * @param opt_count Number of options in the array
 * @return 1 if all required options are present, 0 otherwise
 */
int nyx_cli_validate_required(const nyx_cli_result_t *result,
                              const nyx_cli_opt_def_t *opt_defs, 
                              size_t opt_count);

/**
 * Outputs a formatted banner for the tool
 * 
 * @param config Banner configuration
 */
void nyx_cli_print_banner(const nyx_cli_banner_config_t *config);

/**
 * Validates that a string is a valid IPv4 address
 *
 * @param ip IP address string to validate
 * @return 1 if valid, 0 if invalid
 */
int nyx_cli_validate_ipv4(const char *ip);

/**
 * Validates that a string is a valid MAC address
 *
 * @param mac MAC address to validate (XX:XX:XX:XX:XX:XX format)
 * @return 1 if valid, 0 if invalid
 */
int nyx_cli_validate_mac(const char *mac);

/**
 * Validates that a string is a valid CIDR notation
 *
 * @param cidr CIDR string to validate (e.g., "192.168.1.0/24")
 * @return 1 if valid, 0 if invalid
 */
int nyx_cli_validate_cidr(const char *cidr);

/**
 * Convenience macro to find an option in the options array
 */
#define NYX_CLI_FIND_OPT(opts, count, ch) \
    ({ \
        int _i, _idx = -1; \
        for (_i = 0; _i < (int)(count); _i++) { \
            if ((opts)[_i].short_opt == (ch)) { \
                _idx = _i; \
                break; \
            } \
        } \
        _idx; \
    })

#ifdef __cplusplus
}
#endif

#endif /* NYX_CLI_H */