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
struct pms_state_t * state = NULL;

void reset_options() {

	if (options == NULL) {
		options = malloc(sizeof(struct options_t));
	}

	if (options == NULL) {
		perror("out of memory\n");
		exit(PMS_EXIT_MEMORY);
	}
	options->server = NULL;
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
	state->running = 0;
}

enum mpd_error pms_mpd_connect(struct mpd_connection * connection) {

	enum mpd_error status;
	const char * error_msg;

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
	
	return status;
}

void pms_get_mpd_initial_state(struct mpd_connection * connection) {

	struct mpd_status * status;
	status = mpd_run_status(connection);

}

void * pms_thread_mpd(void * threadid) {

	struct mpd_connection * connection = NULL;
	enum mpd_idle flags;

	pms_mpd_connect(connection);
	pms_get_mpd_initial_state(connection);

	while(state->running) {
		if (connection) {
			mpd_send_idle(connection);
			flags = mpd_recv_idle(connection, false);
			console("mpd_recv_idle(): flags=%d");
		} else {
			sleep(1);
		}
	}

	pthread_exit(NULL);

}

void * pms_thread_display(void * threadid) {
	pthread_exit(NULL);
}

static void pms_start_threads() {
	pthread_t thread_mpd;
	pthread_t thread_display;
	if (pthread_create(&thread_mpd, NULL, pms_thread_mpd, NULL)) {
		fatal(PMS_EXIT_THREAD, "Cannot create MPD thread\n");
	}
	if (pthread_create(&thread_display, NULL, pms_thread_display, NULL)) {
		fatal(PMS_EXIT_THREAD, "Cannot create display thread\n");
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
	state = malloc(sizeof(struct pms_state_t));
	state->running = true;

	curses_init();
	console_init(options->console_size);
	pms_start_threads();
	signal_init();

	while(state->running) {
		curses_get_input();
	}

	curses_shutdown();

	return PMS_EXIT_SUCCESS;
}

