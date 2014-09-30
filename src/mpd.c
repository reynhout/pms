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

#include "pms.h"

struct mpd_connection * pms_mpd_connect(const char * server, unsigned int port, unsigned int timeout) {

    enum mpd_error status;
    const char * error_msg;
    struct mpd_connection * connection;

    console("Connecting to %s...", server);

    connection = mpd_connection_new(server, port, timeout);
    if (connection == NULL) {
        fatal(PMS_EXIT_MEMORY, "mpd connect: out of memory\n");
    }

    status = mpd_connection_get_error(connection);
    if (status == MPD_ERROR_SUCCESS) {
        console("Connected to %s", server);
    } else {
        error_msg = mpd_connection_get_error_message(connection);
        console("Error connecting to %s: error %d: %s", server, status, error_msg);
        mpd_connection_free(connection);
        connection = NULL;
    }
    
    return connection;
}

void pms_mpd_get_status(struct mpd_connection * connection, struct pms_state_t * state) {
    if (state->status) {
        mpd_status_free(state->status);
    }
    state->status = mpd_run_status(connection);
}

void pms_handle_mpd_idle_update(struct mpd_connection * connection, struct pms_state_t * state, enum mpd_idle flags) {

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
        pms_mpd_get_status(connection, state);
    }

    topbar_draw();

}
