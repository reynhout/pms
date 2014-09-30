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

struct songlist_t {
    unsigned long count;
    unsigned long capacity;
    struct mpd_song ** songs;
};

struct songlist_t * songlist_new();
void songlist_free(struct songlist_t * songlist);
void songlist_clear(struct songlist_t * songlist);
void songlist_reserve(struct songlist_t * s, unsigned long length);
void songlist_add(struct songlist_t * songlist, struct mpd_song * song);
