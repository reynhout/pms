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

#include "build.h"

#if defined HAVE_NCURSESW_CURSES_H
	#include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
	#include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
	#include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
	#include <ncurses.h>
#elif defined HAVE_CURSES_H
	#include <curses.h>
#else
	#error "SysV or X/Open-compatible Curses header file required"
#endif

void curses_init();
void curses_shutdown();
void curses_init_windows();
void curses_destroy_windows();

int curses_get_input();

void pms_curses_lock();
void pms_curses_unlock();
