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

#include <mpd/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "pms.h"

struct options_t * options = NULL;
struct pms_state_t * pms_state = NULL;

static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

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
	options->timeout = 200;
	options->console_size = 1000;

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

	pms_get_mpd_state(connection);

	topbar_draw();

}

static void * pms_thread_mpd(void * threadid) {

	struct mpd_connection * connection = NULL;
	bool is_idle = false;
	enum mpd_idle flags = -1;

	connection = pms_mpd_connect();

	while(pms_state->running) {
		if (connection) {
			if (pms_state->status != NULL && flags == 0) {
				mpd_send_idle(connection);
				flags = mpd_recv_idle(connection, true);
			} else {
				pms_handle_mpd_idle_update(connection, flags);
				topbar_draw();
				flags = 0;
			}
		} else {
			console("No connection, sleeping...");
			sleep(1);
		}
	}

	pthread_exit(NULL);

}

static void pms_start_threads() {
	pthread_t thread_mpd;
	if (pthread_create(&thread_mpd, NULL, pms_thread_mpd, NULL)) {
		fatal(PMS_EXIT_THREAD, "Cannot create MPD thread\n");
	}
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

int main(int argc, char** argv) {

	reset_options();
	pms_state = malloc(sizeof(struct pms_state_t));
	memset(pms_state, 0, sizeof(struct pms_state_t));
	pms_state->running = true;

	curses_init();
	console_init(options->console_size);
	signal_init();
	console("%s %s (c) 2006-2014 Kim Tore Jensen <%s>", PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_BUGREPORT);
	pms_start_threads();

	while(pms_state->running) {
		curses_get_input();
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
