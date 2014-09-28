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

#include "build.h"

#ifndef HAVE_PTHREAD
    #error "POSIX thread library required."
#endif

#include <pthread.h>
#include <stdarg.h>
#include <mpd/client.h>

#include "curses.h"
#include "window.h"
#include "console.h"
#include "topbar.h"
#include "input.h"

#define PMS_EXIT_SUCCESS 0
#define PMS_EXIT_MEMORY 1
#define PMS_EXIT_NCURSES 2
#define PMS_EXIT_THREAD 3
#define PMS_EXIT_KILLED 4

#define PMS_HAS_INPUT_STDIN 1
#define PMS_HAS_INPUT_MPD 2

struct options_t {
    char * server;
    unsigned int port;
    unsigned int timeout;
    unsigned int console_size;

};

struct pms_state_t {
    /* Set to false when shutting down. */
    int running;
    struct mpd_status * status;
};

/**
 * Shut down program with fatal error.
 */
void fatal(int exitcode, const char * format, ...);

/**
 * Exit program.
 */
void shutdown();

/**
 * Lock MPD status object
 */
void pms_status_lock();

/**
 * Lock MPD status object
 */
void pms_status_unlock();
