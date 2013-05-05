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

/**
 * Console log line data structure.
 */
typedef struct {
	char str[512];
	char ts[10];
	int verbosity;
	struct tm * timestamp;
} logline_t;

/**
 * Initialize the console.
 */
void console_init(unsigned int max_lines);

/**
 * Get string at array position n
 */
logline_t * console_get_line(unsigned int n);

/**
 * Print formatted string to console.
 */
void console(const char * format, ...);

/**
 * Draw a number of lines on the console window.
 */
void console_draw_lines(long start, long end);

/**
 * Scroll console up or down.
 *
 * @see window_scroll@window.h
 */
int console_scroll(long delta);

/**
 * Free memory for a log line.
 */
void free_logline(logline_t * line);
