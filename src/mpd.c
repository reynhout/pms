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

extern struct pms_state_t * pms_state;

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

void pms_mpd_get_status(struct mpd_connection * connection) {

    if (pms_state->status) {
        mpd_status_free(pms_state->status);
    }
    pms_state->status = mpd_run_status(connection);
}

enum mpd_error pms_mpd_get_playlist(struct mpd_connection * connection, struct songlist_t * songlist) {

    struct mpd_song *song;

    songlist_clear(songlist);
    songlist_reserve(songlist, mpd_status_get_queue_length(pms_state->status));

    if (!mpd_send_list_queue_meta(connection)) {
        return -1;
    }

    while ((song = mpd_recv_song(connection)) != NULL) {
        songlist_add(songlist, song);
    }

    return mpd_connection_get_error(connection);
}

void pms_handle_mpd_idle_update(struct mpd_connection * connection, enum mpd_idle flags) {

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
        pms_mpd_get_status(connection);
    }

    if (flags & (MPD_IDLE_QUEUE)) {
        // TODO: use mpd_send_queue_changes_meta()
        pms_mpd_get_playlist(connection, pms_state->queue);
    }

    topbar_draw();

}
