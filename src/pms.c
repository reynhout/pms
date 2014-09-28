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

#include <mpd/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include "pms.h"

struct options_t * options = NULL;
struct pms_state_t * pms_state = NULL;

static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

void debug(const char * format, ...) {

    va_list ap;

    if (options->debug) {
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
    }
}

void reset_options() {

    if (options == NULL) {
        options = malloc(sizeof(struct options_t));
    }

    if (options == NULL) {
        perror("out of memory\n");
        exit(PMS_EXIT_MEMORY);
    }

    options->server = "localhost";
    options->port = 0;
    options->timeout = 2000;
    options->console_size = 1024;
    options->debug = true;

}

void fatal(int exitcode, const char * format, ...) {
    va_list ap;
    curses_shutdown();
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    exit(exitcode);
}

void shutdown() {
    console("Shutting down.");
    pms_state->running = 0;
}

static struct mpd_connection * pms_mpd_connect() {

    enum mpd_error status;
    const char * error_msg;
    struct mpd_connection * connection;

    console("Connecting to %s...", options->server);

    connection = mpd_connection_new(options->server, options->port, options->timeout);
    if (connection == NULL) {
        fatal(PMS_EXIT_MEMORY, "mpd connect: out of memory\n");
    }

    status = mpd_connection_get_error(connection);
    if (status == MPD_ERROR_SUCCESS) {
        console("Connected to %s", options->server);
    } else {
        error_msg = mpd_connection_get_error_message(connection);
        console("Error connecting to %s: error %d: %s", options->server, status, error_msg);
        mpd_connection_free(connection);
        connection = NULL;
    }
    
    return connection;
}

static void pms_get_mpd_state(struct mpd_connection * connection) {

    pms_status_lock();
    if (pms_state->status) {
        mpd_status_free(pms_state->status);
    }
    pms_state->status = mpd_run_status(connection);
    pms_status_unlock();

}

static void pms_handle_mpd_idle_update(struct mpd_connection * connection, enum mpd_idle flags) {

    console("pms_handle_mpd_idle_update %d", flags);

    if (flags & MPD_IDLE_DATABASE) {
        console("Database has been updated.");
    }
    if (flags & MPD_IDLE_STORED_PLAYLIST) {
        console("Stored playlists have been updated.");
    }
    if (flags & MPD_IDLE_QUEUE) {
        console("The queue has been updated.");
    }
    if (flags & MPD_IDLE_PLAYER) {
        console("Player state has changed.");
    }
    if (flags & MPD_IDLE_MIXER) {
        console("Mixer parameters have changed.");
    }
    if (flags & MPD_IDLE_OUTPUT) {
        console("Outputs have changed.");
    }
    if (flags & MPD_IDLE_OPTIONS) {
        console("Options have changed.");
    }
    if (flags & MPD_IDLE_UPDATE) {
        console("Database update has started or finished.");
    }

    if (flags & (MPD_IDLE_QUEUE | MPD_IDLE_PLAYER | MPD_IDLE_MIXER | MPD_IDLE_OPTIONS)) {
        pms_get_mpd_state(connection);
    }

    topbar_draw();

}

void signal_resize(int signal) {
    console("Resized to %d x %d", LINES, COLS);
    pms_curses_lock();
    curses_destroy_windows();
    curses_init_windows();
    pms_curses_unlock();
}

void signal_kill(int signal) {
    fatal(PMS_EXIT_KILLED, "Killed by signal %d\n", signal);
}

static void signal_init() {
    if (signal(SIGWINCH, signal_resize) == SIG_ERR) {
        console("Error in signal_init(): window resizing will not work!");
    }
    signal(SIGTERM, signal_kill);
}

int pms_get_pending_input_flags(struct mpd_connection * connection) {
    struct timeval tv;
    int mpd_fd = 0;
    int retval;
    int flags = 0;
    fd_set fds;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    if (connection) {
        mpd_fd = mpd_connection_get_fd(connection);
    }

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(mpd_fd, &fds);

    retval = select(mpd_fd+1, &fds, NULL, NULL, &tv);

    if (retval == -1) {
        console("Error %d in select(): %s", errno, strerror(errno));

    } else if (retval > 0) {
        if (mpd_fd != 0 && FD_ISSET(mpd_fd, &fds)) {
            flags |= PMS_HAS_INPUT_MPD;
        }
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            flags |= PMS_HAS_INPUT_STDIN;
        }
    }

    return flags;
}

int main(int argc, char** argv) {

    command_t * command = NULL;
    struct mpd_connection * connection = NULL;
    bool is_idle = false;
    enum mpd_idle flags = -1;
    int input_flags = 0;

    reset_options();
    pms_state = malloc(sizeof(struct pms_state_t));
    memset(pms_state, 0, sizeof(struct pms_state_t));
    pms_state->running = true;

    curses_init();
    console_init(options->console_size);
    signal_init();
    input_reset();
    console("%s %s (c) 2006-2014 Kim Tore Jensen <%s>", PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_BUGREPORT);

    while(pms_state->running) {

        if (!connection) {
            if (connection = pms_mpd_connect()) {
                pms_handle_mpd_idle_update(connection, -1);
            }
        }

        if (connection && !is_idle) {
            mpd_send_idle(connection);
            is_idle = true;
        }

        input_flags = pms_get_pending_input_flags(connection);

        if (input_flags & PMS_HAS_INPUT_MPD) {
            flags = mpd_recv_idle(connection, true);
            is_idle = false;
            pms_handle_mpd_idle_update(connection, flags);
        }

        if (input_flags & PMS_HAS_INPUT_STDIN) {
            if ((command = input_get()) != NULL) {
                while(command->multiplier > 0 && input_handle(command)) {
                    --command->multiplier;
                }
                input_reset();
            }
        }

    }

    curses_shutdown();

    return PMS_EXIT_SUCCESS;
}

void pms_status_lock() {
    pthread_mutex_lock(&status_mutex);
}

void pms_status_unlock() {
    pthread_mutex_unlock(&status_mutex);
}
