/* vi:set ts=8 sts=8 sw=8 noet:
 *
 * PMS  <<Practical Music Search>>
 * Copyright (C) 2006-2015  Kim Tore Jensen
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
 *
 *
 * command.cpp
 * 	mediates all commands between UI and mpd
 */

#include <unistd.h>
#include <math.h>
#include <mpd/client.h>

#include "command.h"
#include "queue.h"
#include "library.h"
#include "pms.h"

extern Pms *			pms;

#define EXIT_IDLE		if (!exit_idle()) { return false; }

#define NOIDLE_POLL_TIMEOUT	20  /* time to wait for zmq_poll() to finish after calling noidle() */


/*
 * Status class
 */
Mpd_status::Mpd_status()
{
	muted			= false;
	volume			= 0;
	repeat			= false;
	single			= false;
	random			= false;
	consume			= false;
	playlist_length		= 0;
	playlist		= -1;
	state			= MPD_STATE_UNKNOWN;
	crossfade		= 0;
	song			= MPD_SONG_NO_NUM;
	songid			= MPD_SONG_NO_ID;
	time_elapsed		= 0;
	time_elapsed_hires.tv_sec = 0;
	time_elapsed_hires.tv_nsec = 0;
	time_total		= 0;
	db_updating		= false;
	error			= 0;
	errstr.clear();

	bitrate			= 0;
	samplerate		= 0;
	bits			= 0;
	channels		= 0;

	artists_count		= 0;
	albums_count		= 0;
	songs_count		= 0;

	uptime			= 0;
	db_update_time		= 0;
	playtime		= 0;
	db_playtime		= 0;

	last_playlist		= playlist;
	last_db_update_time	= db_update_time;
	last_db_updating	= db_updating;
	update_job_id		= -1;
}

void
Mpd_status::assign_status(struct mpd_status * status)
{
	const struct mpd_audio_format	*format;
	uint32_t ms;

	volume			= mpd_status_get_volume(status);
	repeat			= mpd_status_get_repeat(status);
	single			= mpd_status_get_single(status);
	random			= mpd_status_get_random(status);
	consume			= mpd_status_get_consume(status);
	playlist_length		= mpd_status_get_queue_length(status);
	playlist		= mpd_status_get_queue_version(status);
	state			= mpd_status_get_state(status);
	crossfade		= mpd_status_get_crossfade(status);
	song			= mpd_status_get_song_pos(status);
	songid			= mpd_status_get_song_id(status);
	time_total		= mpd_status_get_total_time(status);
	db_updating		= mpd_status_get_update_id(status);

	/* Time elapsed */
#if LIBMPDCLIENT_CHECK_VERSION(2, 1, 0)
	ms = mpd_status_get_elapsed_ms(status);
#else
	ms = mpd_status_get_elapsed(status) * 1000;
#endif
	set_time_elapsed_ms(ms);

	/* Audio format */
	bitrate			= mpd_status_get_kbit_rate(status);
	format			= mpd_status_get_audio_format(status);

	if (!format) {
		return;
	}

	samplerate		= format->sample_rate;
	bits			= format->bits;
	channels		= format->channels;
}

void
Mpd_status::assign_stats(struct mpd_stats * stats)
{
	artists_count		= mpd_stats_get_number_of_artists(stats);
	albums_count		= mpd_stats_get_number_of_albums(stats);
	songs_count		= mpd_stats_get_number_of_songs(stats);

	uptime			= mpd_stats_get_uptime(stats);
	db_update_time		= mpd_stats_get_db_update_time(stats);
	playtime		= mpd_stats_get_play_time(stats);
	db_playtime		= mpd_stats_get_db_play_time(stats);
}

void
Mpd_status::set_time_elapsed_ms(uint32_t ms)
{
	time_elapsed_hires.tv_sec = ms / 1000;
	time_elapsed_hires.tv_nsec = (ms * 10e5) - (time_elapsed_hires.tv_sec * 10e8);
	time_elapsed = round(time_elapsed_hires.tv_sec);
	// pms->log(MSG_DEBUG, 0, "Time elapsed %dms converted to %lus %luns\n", ms, time_elapsed_hires.tv_sec, time_elapsed_hires.tv_nsec);
}

void
Mpd_status::increase_time_elapsed(struct timespec ts)
{
	time_t seconds;

	// pms->log(MSG_DEBUG, 0, "Increasing time elapsed by %lus %9luns\n", ts.tv_sec, ts.tv_nsec);
	time_elapsed_hires.tv_sec += ts.tv_sec;
	time_elapsed_hires.tv_nsec += ts.tv_nsec;

	seconds = time_elapsed_hires.tv_nsec / 10e8;
	if (seconds > 0) {
		time_elapsed_hires.tv_sec += seconds;
		time_elapsed_hires.tv_nsec -= (seconds * 10e8);
	}

	time_elapsed = round(time_elapsed_hires.tv_sec);
	// pms->log(MSG_DEBUG, 0, "Time elapsed set to %lus %9luns\n", time_elapsed_hires.tv_sec, time_elapsed_hires.tv_nsec);
}

bool
Mpd_status::alive() const
{
	/* FIXME: what is this? */
	assert(0);
	return false;
}

/*
 * Command class manages commands sent to and from mpd
 */
Control::Control(Connection * n_conn)
{
	conn = n_conn;
	st = new Mpd_status();
	rootdir = new Directory(NULL, "");
	_song = NULL;
	st->last_playlist = -1;
	_queue = new Queue;
	_library = new Library;
	_active = NULL;
	_is_idle = false;
	command_mode = 0;
	mutevolume = 0;
	crossfadetime = pms->options->crossfade;

	/* Set all bits in mpd_idle event */
	set_mpd_idle_events((enum mpd_idle) 0xffffffff);
	finished_idle_events = 0;
}

Control::~Control()
{
	/* FIXME: these are deleted by the Display class.
	 * Who owns them, really? */
	/*
	delete _library;
	delete _queue;
	*/
	delete st;
}

/*
 * Finishes a command and debugs any errors
 */
bool		Control::finish()
{
	// FIXME: this function must die
	assert(0);
	return false;

	/*
	mpd_finishCommand(conn->h());
	st->error = conn->h()->error;
	st->errstr = conn->h()->errorStr;

	if (st->error != 0)
	{
		pms->log(MSG_CONSOLE, STERR, "MPD returned error %d: %s\n", st->error, st->errstr.c_str());

		// Connection closed
		if (st->error == MPD_ERROR_CONNCLOSED)
		{
			conn->disconnect();
		}

		clearerror();

		return false;
	}

	return true;
	*/
}

/*
 * Clears any error
 */
void
Control::clearerror()
{
	mpd_connection_clear_error(conn->h());
}

/*
 * Have a usable connection?
 */
bool		Control::alive()
{
	return (conn != NULL && conn->connected());
}

/*
 * Reports any error from the last command
 */
const char *	Control::err()
{
	static char * buffer = static_cast<char *>(malloc(1024));

	if (st->errstr.size() == 0)
	{
		if (pms->msg->code == 0)
			sprintf(buffer, _("Error: %s"), pms->msg->str.c_str());
		else
			sprintf(buffer, _("Error %d: %s"), pms->msg->code, pms->msg->str.c_str());

		return buffer;
	}

	return st->errstr.c_str();
}

/**
 * Return the success or error status of the last MPD command sent.
 */
bool
Control::get_error_bool()
{
	return (mpd_connection_get_error(conn->h()) == MPD_ERROR_SUCCESS);
}

bool
Control::exit_idle()
{
	return (!(is_idle() && (!noidle() || !wait_until_noidle())));
}

/**
 * Set pending updates based on which IDLE events were returned from the server.
 */
void
Control::set_mpd_idle_events(enum mpd_idle idle_reply)
{
	uint32_t event = 1;
	const char *idle_name;
	char buffer[2048];
	char *ptr = buffer;

	idle_events |= idle_reply;

	/* Code below only prints debug statement. TODO: return if not debugging? */
	do {
		idle_name = mpd_idle_name((enum mpd_idle) event);
		if (!idle_name) {
			break;
		}
		if (!(idle_reply & event)) {
			continue;
		}
		ptr += sprintf(ptr, "%s ", idle_name);

	} while(event = event << 1);

	*ptr = '\0';

	pms->log(MSG_DEBUG, 0, "Set pending MPD IDLE events: %s\n", buffer);
}

/**
 * Check if there are pending updates.
 *
 * Returns true if there are pending updates, false if not.
 */
bool
Control::has_pending_updates()
{
	return (idle_events != 0);
}

/**
 * Run all pending updates.
 *
 * Returns true on success, false on failure.
 */
bool
Control::run_pending_updates()
{
	/* MPD has new current song */
	if (idle_events & MPD_IDLE_PLAYER) {
		if (!get_current_song()) {
			return false;
		}
		/* MPD_IDLE_PLAYER will be subtracted below */
	}

	/* MPD has new status information */
	if (idle_events & (MPD_IDLE_PLAYER | MPD_IDLE_MIXER | MPD_IDLE_OPTIONS | MPD_IDLE_QUEUE)) {
		if (!get_status()) {
			return false;
		}
		set_update_done(MPD_IDLE_PLAYER);
		set_update_done(MPD_IDLE_MIXER);
		set_update_done(MPD_IDLE_OPTIONS);
		/* MPD_IDLE_QUEUE will be subtracted below */
	}

	/* MPD has updates to queue */
	if (idle_events & MPD_IDLE_QUEUE) {
		if (!update_queue()) {
			return false;
		}
		set_update_done(MPD_IDLE_QUEUE);
	}

	/* MPD has updates to a stored playlist */
	if (idle_events & MPD_IDLE_STORED_PLAYLIST) {
		if (!update_playlist_index()) {
			return false;
		}
		if (!update_playlists()) {
			return false;
		}
		set_update_done(MPD_IDLE_STORED_PLAYLIST);
	}

	/* MPD has new song database */
	if (idle_events & MPD_IDLE_DATABASE) {
		if (!update_library()) {
			return false;
		}
		set_update_done(MPD_IDLE_DATABASE);
	}

	/* Hack to make has_pending_updates() work smoothly without too much
	 * effort. We don't care about the rest of the events, so we just
	 * pretend they never happened. */
	idle_events = 0;

	return true;
}

/**
 * Mark an MPD IDLE update as retrieved.
 */
void
Control::set_update_done(enum mpd_idle flags)
{
	idle_events &= ~flags;
	finished_idle_events |= flags;
}

/**
 * Check whether _any_ MPD IDLE updates are retrieved.
 */
bool
Control::has_any_finished_updates()
{
	return (finished_idle_events != 0);
}

/**
 * Check whether an MPD IDLE update is retrieved.
 */
bool
Control::has_finished_update(enum mpd_idle flags)
{
	return (finished_idle_events & flags);
}

/**
 * Remove a finished MPD IDLE update.
 */
void
Control::clear_finished_update(enum mpd_idle flags)
{
	finished_idle_events &= ~flags;
}

/*
 * Return authorisation level in mpd server
 */
int		Control::authlevel()
{
	int	a = AUTH_NONE;

	if (commands.update)
		a += AUTH_ADMIN;
	if (commands.play)
		a += AUTH_CONTROL;
	if (commands.add)
		a += AUTH_ADD;
	if (commands.status)
		a += AUTH_READ;

	return a;
}

/*
 * Retrieve available commands
 */
bool
Control::get_available_commands()
{
	mpd_pair * pair;

	EXIT_IDLE;

	if (!mpd_send_allowed_commands(conn->h())) {
		return false;
	}

	memset(&commands, 0, sizeof(commands));

	while ((pair = mpd_recv_command_pair(conn->h())) != NULL)
	{
		// FIXME: any other response is not expected
		assert(!strcmp(pair->name, "command"));

		if (!strcmp(pair->value, "add")) {
			commands.add = true;
		} else if (!strcmp(pair->value, "addid")) {
			commands.addid = true;
		} else if (!strcmp(pair->value, "clear")) {
			commands.clear = true;
		} else if (!strcmp(pair->value, "clearerror")) {
			commands.clearerror = true;
		} else if (!strcmp(pair->value, "close")) {
			commands.close = true;
		} else if (!strcmp(pair->value, "commands")) {
			commands.commands = true;
		} else if (!strcmp(pair->value, "consume")) {
			commands.commands = true;
		} else if (!strcmp(pair->value, "count")) {
			commands.count = true;
		} else if (!strcmp(pair->value, "crossfade")) {
			commands.crossfade = true;
		} else if (!strcmp(pair->value, "currentsong")) {
			commands.currentsong = true;
		} else if (!strcmp(pair->value, "delete")) {
			commands.delete_ = true;
		} else if (!strcmp(pair->value, "deleteid")) {
			commands.deleteid = true;
		} else if (!strcmp(pair->value, "disableoutput")) {
			commands.disableoutput = true;
		} else if (!strcmp(pair->value, "enableoutput")) {
			commands.enableoutput = true;
		} else if (!strcmp(pair->value, "find")) {
			commands.find = true;
		} else if (!strcmp(pair->value, "idle")) {
			commands.idle = true;
		} else if (!strcmp(pair->value, "kill")) {
			commands.kill = true;
		} else if (!strcmp(pair->value, "list")) {
			commands.list = true;
		} else if (!strcmp(pair->value, "listall")) {
			commands.listall = true;
		} else if (!strcmp(pair->value, "listallinfo")) {
			commands.listallinfo = true;
		} else if (!strcmp(pair->value, "listplaylist")) {
			commands.listplaylist = true;
		} else if (!strcmp(pair->value, "listplaylistinfo")) {
			commands.listplaylistinfo = true;
		} else if (!strcmp(pair->value, "listplaylists")) {
			commands.listplaylists = true;
		} else if (!strcmp(pair->value, "load")) {
			commands.load = true;
		} else if (!strcmp(pair->value, "lsinfo")) {
			commands.lsinfo = true;
		} else if (!strcmp(pair->value, "move")) {
			commands.move = true;
		} else if (!strcmp(pair->value, "moveid")) {
			commands.moveid = true;
		} else if (!strcmp(pair->value, "next")) {
			commands.next = true;
		} else if (!strcmp(pair->value, "notcommands")) {
			commands.notcommands = true;
		} else if (!strcmp(pair->value, "outputs")) {
			commands.outputs = true;
		} else if (!strcmp(pair->value, "password")) {
			commands.password = true;
		} else if (!strcmp(pair->value, "pause")) {
			commands.pause = true;
		} else if (!strcmp(pair->value, "ping")) {
			commands.ping = true;
		} else if (!strcmp(pair->value, "play")) {
			commands.play = true;
		} else if (!strcmp(pair->value, "playid")) {
			commands.playid = true;
		} else if (!strcmp(pair->value, "playlist")) {
			commands.playlist = true;
		} else if (!strcmp(pair->value, "playlistadd")) {
			commands.playlistadd = true;
		} else if (!strcmp(pair->value, "playlistclear")) {
			commands.playlistclear = true;
		} else if (!strcmp(pair->value, "playlistdelete")) {
			commands.playlistdelete = true;
		} else if (!strcmp(pair->value, "playlistfind")) {
			commands.playlistfind = true;
		} else if (!strcmp(pair->value, "playlistid")) {
			commands.playlistid = true;
		} else if (!strcmp(pair->value, "playlistinfo")) {
			commands.playlistinfo = true;
		} else if (!strcmp(pair->value, "playlistmove")) {
			commands.playlistmove = true;
		} else if (!strcmp(pair->value, "playlistsearch")) {
			commands.playlistsearch = true;
		} else if (!strcmp(pair->value, "plchanges")) {
			commands.plchanges = true;
		} else if (!strcmp(pair->value, "plchangesposid")) {
			commands.plchangesposid = true;
		} else if (!strcmp(pair->value, "previous")) {
			commands.previous = true;
		} else if (!strcmp(pair->value, "random")) {
			commands.random = true;
		} else if (!strcmp(pair->value, "rename")) {
			commands.rename = true;
		} else if (!strcmp(pair->value, "repeat")) {
			commands.repeat = true;
		} else if (!strcmp(pair->value, "single")) {
			commands.single = true;
		} else if (!strcmp(pair->value, "rm")) {
			commands.rm = true;
		} else if (!strcmp(pair->value, "save")) {
			commands.save = true;
		} else if (!strcmp(pair->value, "filter")) {
			commands.filter = true;
		} else if (!strcmp(pair->value, "seek")) {
			commands.seek = true;
		} else if (!strcmp(pair->value, "seekid")) {
			commands.seekid = true;
		} else if (!strcmp(pair->value, "setvol")) {
			commands.setvol = true;
		} else if (!strcmp(pair->value, "shuffle")) {
			commands.shuffle = true;
		} else if (!strcmp(pair->value, "stats")) {
			commands.stats = true;
		} else if (!strcmp(pair->value, "status")) {
			commands.status = true;
		} else if (!strcmp(pair->value, "stop")) {
			commands.stop = true;
		} else if (!strcmp(pair->value, "swap")) {
			commands.swap = true;
		} else if (!strcmp(pair->value, "swapid")) {
			commands.swapid = true;
		} else if (!strcmp(pair->value, "tagtypes")) {
			commands.tagtypes = true;
		} else if (!strcmp(pair->value, "update")) {
			commands.update = true;
		} else if (!strcmp(pair->value, "urlhandlers")) {
			commands.urlhandlers = true;
		} else if (!strcmp(pair->value, "volume")) {
			commands.volume = true;
		}

		mpd_return_pair(conn->h(), pair);
	}

	return get_error_bool();
}

/*
 * Play, pause, toggle, stop, next, prev
 */
bool
Control::play()
{
	EXIT_IDLE;

	return mpd_run_play(conn->h());
}

bool
Control::playid(song_t songid)
{
	EXIT_IDLE;

	return mpd_run_play_id(conn->h(), songid);
}

bool
Control::playpos(song_t songpos)
{
	EXIT_IDLE;

	return mpd_run_play_pos(conn->h(), songpos);
}

bool
Control::pause(bool tryplay)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Toggling pause, tryplay=%d\n", tryplay);

	switch(st->state)
	{
		case MPD_STATE_PLAY:
			return mpd_run_pause(conn->h(), true);
		case MPD_STATE_PAUSE:
			return mpd_run_pause(conn->h(), false);
		case MPD_STATE_STOP:
		case MPD_STATE_UNKNOWN:
		default:
			return (tryplay && play());
	}
}

bool
Control::stop()
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Stopping playback.\n");

	return mpd_run_stop(conn->h());
}

/*
 * Shuffles the playlist.
 */
bool
Control::shuffle()
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Shuffling playlist.\n");

	return mpd_run_shuffle(conn->h());
}

/*
 * Sets repeat mode
 */
bool
Control::repeat(bool on)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Set repeat to %d\n", on);

	return mpd_run_repeat(conn->h(), on);
}

/*
 * Sets single mode
 */
bool
Control::single(bool on)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Set single to %d\n", on);

	return mpd_run_single(conn->h(), on);
}

/*
 * Sets consume mode
 */
bool
Control::consume(bool on)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Set consume to %d\n", on);

	return mpd_run_consume(conn->h(), on);
}

/*
 * Set the volume to an integer between 0 and 100.
 *
 * Return true on success, false on failure.
 */
bool
Control::setvolume(int vol)
{
	if (vol < 0) {
		vol = 0;
	} else if (vol > 100) {
		vol = 100;
	}

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Setting volume to %d%%\n", vol);

	return mpd_run_set_volume(conn->h(), vol);
}

/*
 * Changes volume
 */
bool
Control::volume(int offset)
{
	return setvolume(st->volume + offset);
}

/*
 * Mute/unmute volume
 */
bool
Control::mute()
{
	if (muted()) {
		if (setvolume(mutevolume)) {
			mutevolume = 0;
		}
	} else {
		if (setvolume(0)) {
			mutevolume = st->volume;
		}
	}

	return get_error_bool();
}

/*
 * Is muted?
 */
bool
Control::muted()
{
	return (st->volume <= 0);
}

/*
 * Toggles MPDs built-in random mode
 */
bool
Control::random(int set)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Set random to %d\n", set);

	if (set == -1) {
		set = (st->random == false ? true : false);
	}

	return mpd_run_random(conn->h(), set);
}

/*
 * Appends a playlist to another playlist
 */
song_t		Control::add(Songlist * source, Songlist * dest)
{
	song_t			first = MPD_SONG_NO_ID;
	song_t			result;
	unsigned int		i;

	assert(source != NULL);
	assert(dest != NULL);

	for (i = 0; i < source->size(); i++)
	{
		result = add(dest, source->song(i));
		if (result == MPD_SONG_NO_ID) {
			return result;
		}
		if (first == MPD_SONG_NO_ID) {
			first = result;
		}
	}

	return first;
}

/*
 * Add a song to a playlist
 * FIXME: return value
 */
song_t
Control::add(Songlist * list, Song * song)
{
	song_t		i = MPD_SONG_NO_ID;
	Song *		nsong;

	assert(list != NULL);
	assert(song != NULL);

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Adding song %s to list %s\n", song->file.c_str(), list->filename.c_str());

	if (list == _queue) {
		return mpd_run_add_id(conn->h(), song->file.c_str());
	} else if (list != _library) {
		if (list->filename.size() == 0) {
			return i;
		}
		return mpd_run_playlist_add(conn->h(), list->filename.c_str(), song->file.c_str());
	}

	return i;

	/* FIXME
	if (command_mode != 0) return i;
	if (finish())
	{
		nsong = new Song(song);
		if (list == _queue)
		{
			nsong->id = i;
			nsong->pos = playlist()->size();
			increment();
		}
		else
		{
			nsong->id = MPD_SONG_NO_ID;
			nsong->pos = MPD_SONG_NO_NUM;
			i = list->size();
		}
		list->add(nsong);
	}

	return i;
	*/
}

/*
 * Remove a song from the queue.
 *
 * Returns true on success, false on failure.
 */
bool
Control::remove(Songlist * list, Song * song)
{
	Playlist * playlist;

	assert(song != NULL);
	assert(list != NULL);
	assert(list != _library);

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Removing song with id=%d pos=%d uri=%s from list %s.\n", song->id, song->pos, song->file.c_str(), list->filename.c_str());

	/* Remove song from queue */
	if (list == _queue) {
		// All songs must have ID's
		// FIXME: version requirement
		assert(song->id != MPD_SONG_NO_ID);
		return mpd_run_delete_id(conn->h(), song->id);
	}

	/* Remove song from stored playlist */
	assert(list->filename.size() > 0);

	if (mpd_run_playlist_delete(conn->h(), (char *)list->filename.c_str(), song->pos)) {
		playlist = static_cast<Playlist *>(list);
		playlist->set_synchronized(false);
	}

	return get_error_bool();
}

/*
 * Clears the playlist
 */
int
Control::clear(Songlist * list)
{
	bool r;

	assert(list != NULL);

	/* FIXME: error message */
	if (list == _library) {
		return false;
	}

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Clearing playlist\n");

	if (list == _queue) {
		if ((r = mpd_run_clear(conn->h()))) {
			st->last_playlist = -1;
		}
		return r;
	}

	return mpd_run_playlist_clear(conn->h(), list->filename.c_str());
}

/*
 * Seeks in the stream
 */
bool
Control::seek(int offset)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Seeking by %d seconds\n", offset);

	/* FIXME: perhaps this check should be performed at an earlier stage? */
	if (!song()) {
		return false;
	}

	offset = st->time_elapsed + offset;

	return mpd_run_seek_id(conn->h(), song()->id, offset);
}

/*
 * Toggles crossfading
 * FIXME: return value changed from crossfadetime to boolean
 */
int
Control::crossfade()
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Toggling crossfade\n");

	if (st->crossfade == 0) {
		return mpd_run_crossfade(conn->h(), crossfadetime);
	}

	crossfadetime = st->crossfade;
	return mpd_run_crossfade(conn->h(), 0);
}

/*
 * Set crossfade time in seconds
 * FIXME: return value changed from crossfadetime to boolean
 */
int
Control::crossfade(int interval)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Set crossfade to %d seconds\n", interval);

	if (interval < 0) {
		return false;
	}

	crossfadetime = interval;
	return mpd_run_crossfade(conn->h(), crossfadetime);
}

/*
 * Move selected songs
 */
unsigned int
Control::move(Songlist * list, int offset)
{
	ListItem *	item;
	ListItemSong *	song_item;
	Song *		song;
	int		newpos;
	const char *	filename;
	unsigned int	moved = 0;


	/* FIXME: this function must be moved to the Songlist class */
	assert(false);

	/* Library is read only */
	/* FIXME: error message */
	if (list == _library || !list)
		return 0;

	filename = list->filename.c_str();

	if (offset < 0) {
		//item = list->get_next_selected();
	} else {
		//item = list->get_prev_selected();
	}

	song_item = dynamic_cast<ListItemSong *>(item);
	song = song_item->song;

	EXIT_IDLE;

	//list_start();

	while (song != NULL)
	{
		assert(song->pos != MPD_SONG_NO_NUM);

		newpos = song->pos + offset;

		if (!list->move(song->pos, newpos)) {
			break;
		}

		++moved;

		if (list != _queue) {
			if (!mpd_send_playlist_move(conn->h(), filename, song->pos, newpos)) {
				break;
			}
		} else {
			if (!mpd_run_move(conn->h(), song->pos, song->pos)) {
				break;
			}
		}

		if (offset < 0) {
			//item = list->get_next_selected();
		} else {
			//item = list->get_prev_selected();
		}

		song_item = dynamic_cast<ListItemSong *>(item);
		song = song_item->song;

	}

	return get_error_bool();
}


/*
 * Starts mpd command list/queue mode
 * FIXME: not implemented
 */
bool
Control::list_start()
{
	assert(0);

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Entering command list mode.\n");

	if (mpd_command_list_begin(conn->h(), true)) {
		command_mode = 1;
	}

	return get_error_bool();
}

/*
 * Ends mpd command list/queue mode
 * FIXME: not implemented
 */
bool
Control::list_end()
{
	assert(0);

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Leaving command list mode.\n");

	if (mpd_command_list_end(conn->h())) {
		command_mode = 0;
	}

	return get_error_bool();
}

/*
 * Retrieves status about the state of MPD.
 */
bool
Control::get_status()
{
	mpd_status *	status;
	mpd_stats *	stats;

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Retrieving MPD status from server.\n");

	if ((status = mpd_run_status(conn->h())) == NULL) {
		/* FIXME: error handling? */
		pms->log(MSG_DEBUG, 0, "mpd_run_status returned NULL pointer.\n");
		delete _song;
		_song = NULL;
		st->song = MPD_SONG_NO_NUM;
		st->songid = MPD_SONG_NO_ID;
		return false;
	}

	st->assign_status(status);
	mpd_status_free(status);

	if ((stats = mpd_run_stats(conn->h())) == NULL) {
		/* FIXME ? */
		pms->log(MSG_DEBUG, 0, "mpd_run_stats returned NULL pointer.\n");
		return false;
	}

	st->assign_stats(stats);
	mpd_stats_free(stats);

	return true;
}

Directory::Directory(Directory * par, string n)
{
	parent_ = par;
	name_ = n;
	cursor = 0;
}

/*
 * Return full path from top-level to here
 */
string				Directory::path()
{
	if (parent_ == NULL)
		return "";
	else if (parent_->name().size() == 0)
		return name_;
	else
		return (parent_->path() + '/' + name_);
}

/*
 * Adds a directory entry to the tree
 */
Directory *			Directory::add(string s)
{
	size_t				i;
	string				t;
	vector<Directory *>::iterator	it;
	Directory *			d;

	if (s.size() == 0)
		return NULL;

	i = s.find_first_of('/');

	/* Within this directory */
	if (i == string::npos)
	{
		d = new Directory(this, s);
		children.push_back(d);
		return d;
	}

	t = s.substr(0, i);		// top-level
	s = s.substr(i + 1);		// all sub-level

	/* Search for top-level string in subdirectories */
	it = children.begin();
	while (it != children.end())
	{
		if ((*it)->name() == t)
		{
			return (*it)->add(s);
		}
		++it;
	}

	/* Not found, this should _not_ happen */
	pms->log(MSG_DEBUG, 0, "BUG: directory not found in hierarchy: '%s', '%s'\n", t.c_str(), s.c_str());

	return NULL;
}

/*
void		Directory::debug_tree()
{
	vector<Directory *>::iterator	it;
	vector<Song *>::iterator	is;

	pms->log(MSG_DEBUG, 0, "Printing contents of %s\n", path().c_str());

	is = songs.begin();
	while (is != songs.end())
	{
		pms->log(MSG_DEBUG, 0, "> %s\n", (*is)->file.c_str());
		++is;
	}

	it = children.begin();
	while (it != children.end())
	{
		(*it)->debug_tree();
		++it;
	}
}
*/

/*
 * Retrieves the entire song library from MPD
 */
bool
Control::update_library()
{
	uint32_t			total = 0;
	Song *				song;
	struct mpd_entity *		ent;
	const struct mpd_directory *	ent_directory;
	const struct mpd_song *		ent_song;
	const struct mpd_playlist *	ent_playlist;
	Directory *			dir = rootdir;

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Updating library from DB time %d to %d\n", st->last_db_update_time, st->db_update_time);
	st->last_db_update_time = st->db_update_time;

	if (!mpd_send_list_all_meta(conn->h(), "")) {
		return false;
	}

	_library->clear();

	while ((ent = mpd_recv_entity(conn->h())) != NULL)
	{
		switch(mpd_entity_get_type(ent))
		{
			case MPD_ENTITY_TYPE_SONG:
				ent_song = mpd_entity_get_song(ent);
				song = new Song(ent_song);
				song->id = MPD_SONG_NO_ID;
				song->pos = MPD_SONG_NO_NUM;
				_library->add_local(song);
				dir->songs.push_back(song);
				break;
			case MPD_ENTITY_TYPE_PLAYLIST:
				/* Issue #8: https://github.com/ambientsound/pms/issues/8 */
				ent_playlist = mpd_entity_get_playlist(ent);
				//pms->log(MSG_DEBUG, 0, "NOT IMPLEMENTED in update_library(): got playlist entity in update_library(): %s\n", mpd_playlist_get_path(ent_playlist));
				break;
			case MPD_ENTITY_TYPE_DIRECTORY:
				ent_directory = mpd_entity_get_directory(ent);
				dir = rootdir->add(mpd_directory_get_path(ent_directory));
				assert(dir != NULL);
				/*
				if (dir == NULL)
				{
					dir = rootdir;
				}
				*/
				break;
			case MPD_ENTITY_TYPE_UNKNOWN:
				pms->log(MSG_DEBUG, 0, "BUG in update_library(): entity type not implemented by libmpdclient\n");
				break;
			default:
				pms->log(MSG_DEBUG, 0, "BUG in update_library(): entity type not implemented by PMS\n");
				break;
		}

		mpd_entity_free(ent);

		++total;
	}

	pms->log(MSG_DEBUG, 0, "Processed a total of %d entities during library update\n", total);

	return get_error_bool();
}

/*
 * Retrieves the list of stored playlists.
 *
 * Returns true on success, false on failure.
 */
bool
Control::update_playlist_index()
{
	string				name;
	struct mpd_playlist *		playlist;
	Playlist *			local_playlist;
	vector<Playlist *>::iterator	local_playlist_iterator;

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Updating the list of stored playlists.\n");

	if (!mpd_send_list_playlists(conn->h())) {
		return false;
	}

	/* Mark all playlists as not belonging to MPD, in order to be able to
	 * detect a deleted playlist. */
	local_playlist_iterator = playlists.begin();
	while (local_playlist_iterator != playlists.end()) {
		(*local_playlist_iterator)->set_exists_in_mpd(false);
		++local_playlist_iterator;
	}

	while ((playlist = mpd_recv_playlist(conn->h())) != NULL) {

		name = mpd_playlist_get_path(playlist);

		local_playlist_iterator = playlists.begin();
		while (local_playlist_iterator != playlists.end()) {
			if ((*local_playlist_iterator)->filename == name) {
				local_playlist = *local_playlist_iterator;
				break;
			}
			++local_playlist_iterator;
		}

		if (local_playlist_iterator == playlists.end()) {
			local_playlist = new Playlist();
			local_playlist->filename = name;
			playlists.push_back(local_playlist);
			pms->disp->add_list(local_playlist);
		}

		local_playlist->assign_metadata_from_mpd(playlist);

		pms->log(MSG_DEBUG, 0, "Playlist '%s' was last modified on %u\n", local_playlist->filename.c_str(), local_playlist->get_last_modified());

		mpd_playlist_free(playlist);
	}

	return get_error_bool();
}

/**
 * Retrieves all playlist contents from MPD. Requires that the local playlist
 * index is updated using `update_playlist_index()`.
 *
 * Returns true on success, false on failure.
 */
bool
Control::update_playlists()
{
	vector<Playlist *>::iterator	playlist_iterator;
	Playlist *			playlist;

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Synchronizing all playlists with MPD\n");

	playlist_iterator = playlists.begin();

	while (playlist_iterator != playlists.end()) {

		playlist = *playlist_iterator;

		if (!playlist->exists_in_mpd()) {
			pms->disp->remove_list(playlist);
			delete playlist;
			playlist_iterator = playlists.erase(playlist_iterator);
			continue;
		} else if (!playlist->is_synchronized()) {
			if (!update_playlist(playlist)) {
				return false;
			}
			playlist->set_synchronized(true);
		} else {
			pms->log(MSG_DEBUG, 0, "Playlist %s is already synchronized.\n", playlist->filename.c_str());
		}
		++playlist_iterator;
	}

	pms->log(MSG_DEBUG, 0, "Playlist synchronization is finished.\n");

	return get_error_bool();
}

/*
 * Retrieve the contents of a stored playlist. Will synchronize local playlists
 * with the MPD server, overwriting the local versions.
 *
 * Returns true on success, false on failure.
 */
bool
Control::update_playlist(Playlist * playlist)
{
	Song *			song;
	mpd_entity *		ent;
	const mpd_song *	ent_song;

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Retrieving playlist %s from MPD server.\n", playlist->filename.c_str());

	if (!mpd_send_list_playlist_meta(conn->h(), playlist->filename.c_str())) {
		return false;
	}

	playlist->clear();

	while ((ent = mpd_recv_entity(conn->h())) != NULL)
	{
		switch(mpd_entity_get_type(ent))
		{
			case MPD_ENTITY_TYPE_SONG:
				ent_song = mpd_entity_get_song(ent);
				song = new Song(ent_song);
				song->id = MPD_SONG_NO_ID;
				song->pos = MPD_SONG_NO_NUM;
				playlist->add_local(song);
				break;
			case MPD_ENTITY_TYPE_UNKNOWN:
				pms->log(MSG_DEBUG, 0, "BUG in retrieve_lists(): entity type not implemented by libmpdclient\n");
				break;
			default:
				pms->log(MSG_DEBUG, 0, "BUG in retrieve_lists(): entity type not implemented by PMS\n");
				break;
		}
		mpd_entity_free(ent);
	}

	playlist->set_column_size();

	return get_error_bool();
}

/*
 * Find a playlist with the specified filename.
 *
 * Returns a pointer to the Playlist object, or NULL if not found.
 */
Playlist *
Control::find_playlist(string fn)
{
	vector<Playlist *>::iterator	i;

	i = playlists.begin();
	while (i != playlists.end()) {
		if ((*i)->filename == fn) {
			return *i;
		}
		++i;
	}

	return NULL;
}

/*
 * Create a new stored playlist.
 *
 * Returns true on success, false on failure.
 */
bool
Control::create_playlist(string name)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Creating playlist '%s'\n", name.c_str());

	/* Apparently, it is enough to run "playlistclear" in order to create a
	 * new playlist. However, we do not get protection against clearing an
	 * existing playlist. This is worked around by calling mpd_run_save()
	 * first, which will complain with a suitable error message. */
	if (mpd_run_save(conn->h(), name.c_str())) {
		mpd_run_playlist_clear(conn->h(), name.c_str());
	}

	return get_error_bool();
}

/*
 * Save the queue contents into a stored playlist.
 *
 * Returns true on success, false on failure.
 */
bool
Control::save_playlist(string name)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Saving the queue into playlist '%s'\n", name.c_str());

	return mpd_run_save(conn->h(), name.c_str());
}

/*
 * Permanently removes a stored playlist.
 *
 * Returns true on success, false on failure.
 */
bool
Control::delete_playlist(string name)
{
	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Deleting the playlist '%s'\n", name.c_str());

	return mpd_run_rm(conn->h(), name.c_str());
}

/*
 * Returns the active playlist
 */
Songlist *
Control::activelist()
{
	return _active;
}

/*
 * Sets the active playlist
 */
bool
Control::activatelist(Songlist * list)
{
	assert(list);
	_active = list;
	return true;
}

/*
 * Retrieves current playlist from MPD
 * TODO: implement missing entity types
 */
bool
Control::update_queue()
{
	bool			rc;
	Song *			song;
	struct mpd_entity *	ent;
	const struct mpd_song *	ent_song;

	EXIT_IDLE;

	pms->log(MSG_DEBUG, 0, "Updating playlist from version %d to %d\n", st->last_playlist, st->playlist);

	if (st->last_playlist == -1) {
		_queue->clear();
	}

	if (!mpd_send_queue_changes_meta(conn->h(), st->last_playlist)) {
		return false;
	}

	while ((ent = mpd_recv_entity(conn->h())) != NULL)
	{
		switch(mpd_entity_get_type(ent))
		{
			case MPD_ENTITY_TYPE_SONG:
				ent_song = mpd_entity_get_song(ent);
				song = new Song(ent_song);
				_queue->add_local(song);
				break;
			case MPD_ENTITY_TYPE_UNKNOWN:
				pms->log(MSG_DEBUG, 0, "BUG in update_queue(): entity type not implemented by libmpdclient\n");
				break;
			default:
				pms->log(MSG_DEBUG, 0, "BUG in update_queue(): entity type not implemented by PMS\n");
				break;
		}
		mpd_entity_free(ent);
	}

	if ((rc = get_error_bool()) == true) {
		_queue->truncate_local(st->playlist_length);
		st->last_playlist = st->playlist;
	}

	return rc;
}

/*
 * Retrieves the currently playing song from MPD, and caches it locally.
 *
 * Returns true on success, false on failure.
 */
bool
Control::get_current_song()
{
	bool status_ok;
	struct mpd_song * song;

	EXIT_IDLE;

	song = mpd_run_current_song(conn->h());
	status_ok = get_error_bool();

	if (!status_ok) {
		return status_ok;
	}

	if (_song != NULL) {
		delete _song;
		_song = NULL;
	}

	if (song) {
		_song = new Song(song);
		mpd_song_free(song);
	}

	return get_error_bool();
}

/*
 * Rescans entire library
 * FIXME: runs "update", there is also a "rescan" that can be implemented
 * FIXME: dubious return value
 */
bool
Control::rescandb(string dest)
{
	/* we can handle an MPD error if this is not supported */
	/*
	if (st->db_updating) {
		// FIXME: error message
		return false;
	}
	*/

	unsigned int job_id;

	EXIT_IDLE;

	job_id = mpd_run_update(conn->h(), dest.c_str());
	if (job_id == 0) {
		/* FIXME: handle errors */
		return false;
	}

	// FIXME?
	st->update_job_id = job_id;
	return job_id;
	//st->update_job_id = mpd_getUpdateId(conn->h());

	//return finish();
}

/*
 * Sends a password to the mpd server
 * FIXME: should retrieve updated privileges list?
 */
bool
Control::sendpassword(string pw)
{
	EXIT_IDLE;

	return mpd_run_password(conn->h(), pw.c_str());
}

/**
 * Set client in IDLE mode
 */
bool
Control::idle()
{
	if (is_idle()) {
		return true;
	}

	pms->log(MSG_DEBUG, 0, "Entering IDLE mode.\n");
	set_is_idle(mpd_send_idle(conn->h()));

	return is_idle();
}

/**
 * Take client out of IDLE mode
 */
bool
Control::noidle()
{
	if (!is_idle()) {
		return true;
	}

	pms->log(MSG_DEBUG, 0, "Leaving IDLE mode.\n");
	set_is_idle(mpd_send_noidle(conn->h()));

	return is_idle();
}

/**
 * Block until MPD is ready to receieve requests.
 */
bool
Control::wait_until_noidle()
{
	pms->poll_events(NOIDLE_POLL_TIMEOUT);
	return pms->run_has_idle_events();
}

bool
Control::is_idle()
{
	return _is_idle;
}

void
Control::set_is_idle(bool i)
{
	_is_idle = i;
}
