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

#include "pms.h"

extern WINDOW * window_main;

int window_scroll(window_t * window, long delta) {
	long npos;

	if (window->num_lines <= window->height) {
		return 0;
	}

	if (delta > 0) {
		npos = window->position + delta;
		if (npos + window->height > window->num_lines) {
			delta = window->num_lines - window->height - window->position;
			// TODO: beep?
		}
	} else if (delta < 0) {
		npos = window->position - delta - 2;
		if (npos < 0) {
			delta = -window->position;
			// TODO: beep?
		}
	}

	window->position += delta;
	wscrl(window_main, delta);
	wrefresh(window_main);

	return delta;
}
