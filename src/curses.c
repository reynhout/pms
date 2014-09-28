/* vi:set ts=4 sts=4 sw=4 noet:
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

#include "pms.h"
#include <stdlib.h>

WINDOW * window_topbar;
WINDOW * window_main;
WINDOW * window_statusbar;

static pthread_mutex_t curses_mutex = PTHREAD_MUTEX_INITIALIZER;

void curses_destroy_windows() {
	if (window_topbar) {
		delwin(window_topbar);
	}
	if (window_main) {
		delwin(window_main);
	}
	if (window_statusbar) {
		delwin(window_statusbar);
	}
}

void curses_init_windows() {
	window_topbar = newwin(1, COLS, 0, 0);
	window_main = newwin(LINES - 2, COLS, 1, 0);
	window_statusbar = newwin(1, COLS, LINES - 1, 0);
	scrollok(window_main, true);
	scrollok(window_topbar, false);
	scrollok(window_statusbar, false);
	keypad(window_statusbar, true);
}

void curses_init() {
	if ((initscr()) == NULL) {
		fatal(PMS_EXIT_NCURSES, "Unable to start ncurses, exiting.\n");
	}

	curses_init_windows();

	noecho();
	raw();
	curs_set(0);
	halfdelay(10);

#ifdef HAVE_CURSES_COLOR
	if (has_colors()) {
		start_color();
		use_default_colors();
	}
#endif

	wclear(window_topbar);
	wclear(window_main);
	wclear(window_statusbar);
	wrefresh(window_topbar);
	wrefresh(window_main);
	wrefresh(window_statusbar);
}

void curses_shutdown() {
	endwin();
}

int curses_get_input() {
	return wgetch(window_statusbar);
}

void pms_curses_lock() {
	pthread_mutex_lock(&curses_mutex);
}

void pms_curses_unlock() {
	pthread_mutex_unlock(&curses_mutex);
}
