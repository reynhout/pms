/* vi:set ts=4 sts=4 sw=4 noet:
 *
 * Practical Music Search
 * Copyright (c) 2006-2013 Kim Tore Jensen
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
#include "console.h"
#include "curses.h"

static char ** lines = NULL;
static unsigned int line_cursor = 0;
static unsigned int line_limit = 0;
static unsigned int first_line = 0;
static unsigned int num_lines = 0;
static int full = 0;

pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;

extern WINDOW * window_main;

window_t * console_window = NULL;

void console_init(unsigned int max_lines) {
	if (lines != NULL) {
		lines = realloc(lines, max_lines * sizeof(char *));
	} else {
		lines = malloc(max_lines * sizeof(char *));
	}
	if (lines == NULL) {
		fatal(PMS_EXIT_MEMORY, "out of memory\n");
	}
	if (console_window == NULL) {
		console_window = malloc(sizeof(window_t));
		if (console_window == NULL) {
			fatal(PMS_EXIT_MEMORY, "out of memory\n");
		}
		// TODO: move into resize func
		console_window->height = LINES - 2;
	}
	line_limit = max_lines;
	memset(lines, 0, max_lines * sizeof(char *));
}

const char * console_get_line(unsigned int n) {
	n = (n + first_line) % line_limit;
	return lines[n];
}

void console(const char * format, ...) {
	va_list		ap;
	char *		buffer;
	char *		existing;

	pthread_mutex_lock(&console_mutex);

	buffer = malloc(512);
	va_start(ap, format);
	vsnprintf(buffer, 512, format, ap);
	va_end(ap);

	if (lines[line_cursor] != NULL) {
		free(lines[line_cursor]);
	}

	if (full) {
		first_line += 1;
		if (first_line >= line_limit) {
			first_line = 0;
		}
	} else {
		++num_lines;
	}

	lines[line_cursor] = buffer;

	if (++line_cursor >= line_limit) {
		line_cursor = 0;
		full = 1;
	}

	int changed = console_scroll(1);
	if (changed > 0) {
		console_draw_lines(console_window->height - changed, console_window->height);
	} else if (changed == 0) {
		console_draw_lines(num_lines - 1, num_lines - 1);
	}

	pthread_mutex_unlock(&console_mutex);
}

void console_draw_lines(long start, long end) {

	long s;
	long ptr = console_window->position;

	ptr += start;
	for (s = start; s <= end; s++) {
		if (ptr >= num_lines) {
			break;
		}
		mvwprintw(window_main, s, 0, "%d %s", time(NULL), console_get_line(ptr));
		++ptr;
	}

	wrefresh(window_main);
}

int console_scroll(long delta) {
	long npos;

	if (num_lines <= console_window->height) {
		return 0;
	}

	if (delta > 0) {
		npos = console_window->position + delta;
		if (npos + console_window->height > num_lines) {
			delta = num_lines - console_window->height - console_window->position;
			// TODO: beep?
		}
	} else if (delta < 0) {
		npos = console_window->position - delta;
		if (npos < 0) {
			delta = -console_window->position;
			// TODO: beep?
		}
	}

	console_window->position += delta;
	wscrl(window_main, delta);
	wrefresh(window_main);

	return delta;
}
