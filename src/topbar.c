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
#include <string.h>

extern WINDOW * window_topbar;
extern struct pms_state_t * pms_state;

void topbar_draw() {

    if (!pms_state->status) {
        return;
    }

    pms_curses_lock();
    pms_status_lock();

    mvwprintw(window_topbar, 0, 0, "%s %s %s [%s]",
            PACKAGE_NAME, PACKAGE_VERSION,
            topbar_playing_str(),
            topbar_mode_str());
    wrefresh(window_topbar);

    pms_status_unlock();
    pms_curses_unlock();

}

const char * topbar_mode_str() {

    static char buffer[5];
    enum mpd_state s;

    buffer[0] = (mpd_status_get_repeat(pms_state->status) ? 'r' : '-');
    buffer[1] = (mpd_status_get_random(pms_state->status) ? 'z' : '-');
    buffer[2] = (mpd_status_get_single(pms_state->status) ? 's' : '-');
    buffer[3] = (mpd_status_get_consume(pms_state->status) ? 'c' : '-');
    buffer[4] = '\0';

    return (const char *) buffer;

}

const char * topbar_playing_str() {

    static char buffer[20];
    enum mpd_state s;

    s = mpd_status_get_state(pms_state->status);

    if (s == MPD_STATE_STOP) {
        strcpy(buffer, "stopped");
    } else if (s == MPD_STATE_PLAY) {
        strcpy(buffer, "playing");
    } else if (s == MPD_STATE_PAUSE) {
        strcpy(buffer, "paused");
    } else {
        strcpy(buffer, "unknown");
    }

    return (const char *) buffer;

}
