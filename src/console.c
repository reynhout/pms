/* vi:set ts=4 sts=4 sw=4 et:
 *
 * Practical Music Search
 * Copyright (c) 2006-2014 Kim Tore Jensen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include "pms.h"

extern WINDOW * window_main;

static logline_t ** lines = NULL;
static unsigned int line_cursor = 0;
static unsigned int line_limit = 0;

pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;

window_t * console_window = NULL;

void console_resize() {
    console_window->height = LINES - 2;
    console_window->position = 0;
    console_window->num_lines = 0;
}

void console_init(unsigned int max_lines) {
    if (lines != NULL) {
        lines = realloc(lines, max_lines * sizeof(logline_t *));
    } else {
        lines = malloc(max_lines * sizeof(logline_t *));
    }
    if (lines == NULL) {
        fatal(PMS_EXIT_MEMORY, "out of memory\n");
    }
    if (console_window == NULL) {
        console_window = malloc(sizeof(window_t));
        if (console_window == NULL) {
            fatal(PMS_EXIT_MEMORY, "out of memory\n");
        }
        console_resize();
    }
    memset(lines+line_limit, 0, (max_lines-line_limit)*sizeof(logline_t *));
    line_limit = max_lines;
}

void console(const char * format, ...) {
    logline_t *    line;
    time_t        t;
    va_list        ap;

    line = new_logline();

    va_start(ap, format);
    vsnprintf(line->str, 512, format, ap);
    va_end(ap);

    if (lines[line_cursor] != NULL) {
        free_logline(lines[line_cursor]);
    }

    ++console_window->num_lines;

    t = time(NULL);
    localtime_r(&t, &line->timestamp);
    strftime(line->ts, 9, "%H:%M:%S", &line->timestamp);

    lines[line_cursor] = line;

    if (++line_cursor >= line_limit) {
        console_init(line_limit*2);
    }

    /* Scroll window if at bottom. */
    if (console_window->num_lines < console_window->height || console_window->position + console_window->height + 1 >= console_window->num_lines) {
        console_scroll(1);
    }
}

void console_draw_lines(long start, long end) {

    long s;
    long ptr;
    logline_t * line;

    ptr = console_window->position + start;
    for (s = start; s <= end; s++) {
        if (ptr >= console_window->num_lines) {
            break;
        }
        line = lines[ptr];
        mvwprintw(window_main, s, 0, "%s: %s", line->ts, line->str);
        ++ptr;
    }

    wrefresh(window_main);
}

int console_scroll(long delta) {

    int changed = window_scroll(console_window, delta);

    if (changed > 0) {
        console_draw_lines(console_window->height - changed, console_window->height);
    } else if (changed < 0) {
        console_draw_lines(0, -changed - 1);
    } else {
        console_draw_lines(console_window->num_lines - 1, console_window->num_lines - 1);
    }

    return changed;
}

int console_scroll_to(long position) {
    if (position < 0) {
        position = console_window->num_lines+position;
    }
    position -= console_window->position;
    console_scroll(position);
}

logline_t * new_logline() {
    logline_t * line;

    if ((line = malloc(sizeof(logline_t))) == NULL) {
        fatal(PMS_EXIT_MEMORY, "Out of memory");
    }

    line->str = malloc(512);
    line->ts = malloc(9);

    if (line->str == NULL || line->ts == NULL) {
        fatal(PMS_EXIT_MEMORY, "Out of memory");
    }

    return line;
}

void free_logline(logline_t * line) {
    free(line->str);
    free(line->ts);
    free(line);
}
