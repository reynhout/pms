/* vi:set ts=8 sts=8 sw=8 noet:
 *
 * Practical Music Search
 * Copyright (c) 2006-2011  Kim Tore Jensen
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
 */

#include "window.h"
#include "curses.h"
#include "command.h"
#include "mpd.h"
#include "color.h"
#include "config.h"
#include "input.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdlib.h>
#include <math.h>

using namespace std;

extern MPD * mpd;
extern Config * config;
extern Input * input;

Windowmanager::Windowmanager()
{
	/* Setup static windows that are not in the window list */
	topbar = new Wtopbar;
	statusbar = new Wstatusbar;
	readout = new Wreadout;

	/* Setup static windows that appear in the window list */
	console = new Wconsole;
	console->title = "Console";
	playlist = new Wsonglist;
	playlist->title = "Playlist";
	library = new Wsonglist;
	library->title = "Library";
	windows.push_back(WMAIN(console));
	windows.push_back(WMAIN(playlist));
	windows.push_back(WMAIN(library));
}

void Windowmanager::init_ncurses()
{
	if ((initscr()) == NULL)
	{
		ready = false;
		return;
	}

	noecho();
	raw();
	nodelay(stdscr, true);
	keypad(stdscr, true);
	curs_set(0);

	if (has_colors())
	{
		start_color();
		use_default_colors();
	}
}

Windowmanager::~Windowmanager()
{
	clear();
	refresh();
	endwin();
}

void Windowmanager::detect_dimensions()
{
	vector<Wmain *>::iterator wit;

	topbar->set_dimensions(0, 0, config->topbar_height-1, COLS);
	statusbar->set_dimensions(LINES-1, 0, LINES-1, COLS-4);
	readout->set_dimensions(LINES-1, COLS-3, LINES-1, COLS);
	wit = windows.begin();
	while (wit != windows.end()) {
		(*wit)->set_dimensions(config->topbar_height, 0, LINES-2, COLS);
		++wit;
	}
}

void Windowmanager::draw()
{
	topbar->draw();
	statusbar->draw();
	readout->draw();
	active->draw();
	flush();
}

void Windowmanager::qdraw()
{
	if (input->mode != INPUT_MODE_COMMAND && !statusbar->need_draw &&
		(topbar->need_draw || active->need_draw || readout->need_draw))
	{
		statusbar->qdraw();
	}
	if (topbar->need_draw)
		topbar->draw();
	if (active->need_draw)
		active->draw();
	if (readout->need_draw)
		readout->draw();
	if (statusbar->need_draw)
		statusbar->draw();

	flush();
}

void Windowmanager::flush()
{
	vector<Wmain *>::iterator wit;

	topbar->flush();
	statusbar->flush();
	readout->flush();
	wit = windows.begin();
	while (wit != windows.end()) {
		(*wit)->flush();
		++wit;
	}
}

bool Windowmanager::activate(Wmain * nactive)
{
	Wsonglist * ws;
	unsigned int i;

	for (i = 0; i < windows.size(); ++i)
	{
		if (windows[i] == nactive)
		{
			if ((ws = WSONGLIST(nactive)) != NULL && config->playback_follows_window)
				mpd->activate_songlist(ws->songlist);

			if (nactive != active)
				last_active = active;
			active_index = i;
			active = nactive;
			context = active->context;
			active->clear();
			draw();
			return true;
		}
	}

	return false;
}

bool Windowmanager::toggle()
{
	return activate(last_active);
}

bool Windowmanager::go(string title)
{
	string tmp;
	vector<Wmain *>::const_iterator it;

	std::transform(title.begin(), title.end(), title.begin(), ::tolower);
	for (it = windows.begin(); it != windows.end(); ++it)
	{
		tmp = (*it)->title;
		std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
		if (tmp == title)
			return activate(*it);
	}

	return false;
}

bool Windowmanager::go(unsigned int index)
{
	if (index >= windows.size())
		return false;

	return activate(windows[index]);
}

void Windowmanager::cycle(int offset)
{
	Wsonglist * ws;

	if (offset >= 0)
		offset %= windows.size();
	else
		offset %= -windows.size();

	offset = active_index + offset;
	if (offset < 0)
		offset = windows.size() - offset;
	else if (offset >= (int)windows.size())
		offset -= windows.size();

	active_index = (unsigned int)offset;
	if (windows[active_index] != active)
		last_active = active;
	active = windows[active_index];
	context = active->context;

	if ((ws = WSONGLIST(active)) != NULL && config->playback_follows_window)
		mpd->activate_songlist(ws->songlist);

	active->draw();
	readout->draw();
}

void Windowmanager::update_column_length()
{
	Wsonglist * w;
	vector<Wmain *>::iterator i;
	for (i = windows.begin(); i != windows.end(); ++i)
	{
		if ((w = WSONGLIST(*i)) != NULL)
			w->update_column_length();
	}
}

void Windowmanager::bell()
{
	if (!config->use_bell)
		return;
	
	if (config->visual_bell)
	{
		if (flash() == ERR)
			beep();
	}
	else
	{
		if (beep() == ERR)
			flash();
	}
}
