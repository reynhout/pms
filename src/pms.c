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
    console("Shutting down...");
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

void * pms_thread_mpd(void * threadid) {
    struct mpd_connection * connection = NULL;
    enum mpd_idle flags;
    pms_mpd_connect(connection);
    while(state->running && connection) {
        mpd_send_idle(connection);
        flags = mpd_recv_idle(connection, false);
        console("mpd_recv_idle(): flags=%d");
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
    curses_thread_lock();
    curses_destroy_windows();
    curses_init_windows();
    curses_thread_unlock();
}

static void signal_init() {
    if (signal(SIGWINCH, signal_resize) == SIG_ERR) {
        console("Error in signal_init(): window resizing will not work!");
    }
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

