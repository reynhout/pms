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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "pms.h"

struct songlist_t * songlist_new() {

    struct songlist_t * songlist;
    if ((songlist = malloc(sizeof(struct songlist_t))) == NULL) {
        fatal(PMS_EXIT_MEMORY, "Out of memory");
    }
    memset(songlist, 0, sizeof(*songlist));
    return songlist;
}

void songlist_free(struct songlist_t * songlist) {

    assert(songlist != NULL);
    songlist_clear(songlist);
    free(songlist);
}

void songlist_clear(struct songlist_t * songlist) {

    while (songlist->count > 0) {
        mpd_song_free(songlist->songs[--songlist->count]);
    }

    songlist_reserve(songlist, 0);
}

void songlist_reserve(struct songlist_t * songlist, unsigned long length) {

    assert(length >= songlist->count);

    if ((songlist->songs = realloc(songlist->songs, sizeof(struct mpd_song *)*length)) == NULL && length > 0) {
        fatal(PMS_EXIT_MEMORY, "Out of memory");
    }

    songlist->capacity = length;
}

void songlist_add(struct songlist_t * songlist, struct mpd_song * song) {

    if (songlist->capacity <= songlist->count) {
        songlist_reserve(songlist, songlist->capacity*2);
    }

    songlist->songs[songlist->count++] = song;
}
