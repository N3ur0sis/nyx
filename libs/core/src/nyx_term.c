#include "nyx_term.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int enabled;
    int tty_checked;
    int interactive;
    int has_status;
    unsigned int suspend_depth;
    char status_line[512];
    pthread_mutex_t lock;
} nyx_term_state_t;

static nyx_term_state_t g_term = {.enabled = 1,
                                  .tty_checked = 0,
                                  .interactive = 0,
                                  .has_status = 0,
                                  .suspend_depth = 0,
                                  .status_line = {0},
                                  .lock = PTHREAD_MUTEX_INITIALIZER};

static void term_detect_tty_locked(void)
{
    if (g_term.tty_checked)
        return;

    g_term.interactive = isatty(STDERR_FILENO) ? 1 : 0;
    g_term.tty_checked = 1;
}

static int term_can_render_locked(void)
{
    term_detect_tty_locked();
    return g_term.enabled && g_term.interactive;
}

static void term_draw_locked(void)
{
    if (!term_can_render_locked() || g_term.suspend_depth > 0 || !g_term.has_status)
        return;

    flockfile(stderr);
    fprintf(stderr, "\r\033[2K%s", g_term.status_line);
    fflush(stderr);
    funlockfile(stderr);
}

static void term_clear_locked(void)
{
    if (!term_can_render_locked())
        return;

    flockfile(stderr);
    fprintf(stderr, "\r\033[2K");
    fflush(stderr);
    funlockfile(stderr);
}

void nyx_term_set_enabled(int enabled)
{
    pthread_mutex_lock(&g_term.lock);
    g_term.enabled = enabled ? 1 : 0;
    if (!g_term.enabled)
        term_clear_locked();
    pthread_mutex_unlock(&g_term.lock);
}

int nyx_term_is_enabled(void)
{
    pthread_mutex_lock(&g_term.lock);
    int enabled = g_term.enabled;
    pthread_mutex_unlock(&g_term.lock);
    return enabled;
}

int nyx_term_is_interactive(void)
{
    pthread_mutex_lock(&g_term.lock);
    term_detect_tty_locked();
    int interactive = g_term.interactive;
    pthread_mutex_unlock(&g_term.lock);
    return interactive;
}

void nyx_term_suspend(void)
{
    pthread_mutex_lock(&g_term.lock);
    g_term.suspend_depth++;
    term_clear_locked();
    pthread_mutex_unlock(&g_term.lock);
}

void nyx_term_resume(void)
{
    pthread_mutex_lock(&g_term.lock);
    if (g_term.suspend_depth > 0)
        g_term.suspend_depth--;
    term_draw_locked();
    pthread_mutex_unlock(&g_term.lock);
}

void nyx_term_clear_status(void)
{
    pthread_mutex_lock(&g_term.lock);
    term_clear_locked();
    g_term.has_status = 0;
    g_term.status_line[0] = '\0';
    pthread_mutex_unlock(&g_term.lock);
}

void nyx_term_statusf(const char *fmt, ...)
{
    if (!fmt)
        return;

    pthread_mutex_lock(&g_term.lock);

    va_list args;
    va_start(args, fmt);
    vsnprintf(g_term.status_line, sizeof(g_term.status_line), fmt, args);
    va_end(args);

    g_term.has_status = 1;
    term_draw_locked();
    pthread_mutex_unlock(&g_term.lock);
}

void nyx_term_progress(const char *label, size_t current, size_t total)
{
    if (label && label[0]) {
        nyx_term_statusf("[*] %s: %zu/%zu", label, current, total);
    } else {
        nyx_term_statusf("[*] Progress: %zu/%zu", current, total);
    }
}

void nyx_term_spinner(const char *label, unsigned long tick)
{
    static const char frames[] = {'|', '/', '-', '\\'};
    char frame = frames[tick % (sizeof(frames) / sizeof(frames[0]))];

    if (label && label[0]) {
        nyx_term_statusf("[%c] %s", frame, label);
    } else {
        nyx_term_statusf("[%c] Working...", frame);
    }
}
