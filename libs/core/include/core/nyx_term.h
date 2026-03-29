#ifndef NYX_TERM_H
#define NYX_TERM_H

#include <stddef.h>

void nyx_term_set_enabled(int enabled);
int nyx_term_is_enabled(void);
int nyx_term_is_interactive(void);

void nyx_term_suspend(void);
void nyx_term_resume(void);
void nyx_term_clear_status(void);

void nyx_term_statusf(const char *fmt, ...);
void nyx_term_progress(const char *label, size_t current, size_t total);
void nyx_term_spinner(const char *label, unsigned long tick);

#endif
