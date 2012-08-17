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

#include <math.h>
#include <string.h>
#include "window.h"
#include "config.h"

extern Windowmanager * wm;
extern Config * config;

Window::Window()
{
	window = NULL;
	memset(&rect, 0, sizeof rect);
}

void Window::set_dimensions(int top, int left, int bottom, int right)
{
	if (window) {
		delwin(window);
	}
	rect.top = top;
	rect.left = left;
	rect.bottom = bottom;
	rect.right = right;
	window = newwin(bottom-top+1, right-left+1, top, left);
}

void Window::flush()
{
	wrefresh(window);
}

void Window::print(Color * c, int y, int x, const char * fmt, ...)
{
	va_list			ap;
	char			buffer[1024];

	if (!c) {
		return;
	}
	
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);

	wattron(window, c->pair);
	mvwprintw(window, y, x, buffer);
	wattroff(window, c->pair);
}

void Window::clearline(int line, Color * c)
{
	wattron(window, c->pair | A_INVIS);
	mvwhline(window, line, rect.left, ' ', rect.right - rect.left);
	wattroff(window, c->pair | A_INVIS);
}

void Window::draw()
{
	unsigned int i, h;

	if (!visible())
		return;

	need_draw = false;

	h = height();

	for (i = 0; i <= h; i++)
		drawline(i);

	/*FIXME*/
	wrefresh(window);
}

void Window::qdraw()
{
	need_draw = true;
}

void Window::clear()
{
	wclear(window);
}

void Window::clear(Color * c)
{
	wclear(window);
	/*FIXME*/
	//clear(config->colors.standard);
}

unsigned int Window::height()
{
	return rect.bottom - rect.top;
}

Wmain::Wmain()
{
	position = 0;
	cursor = 0;
}

unsigned int Wmain::height()
{
	return rect.bottom - rect.top - (config->show_window_title ? 1 : 0);
}

void Wmain::draw()
{
	if (!visible())
		return;

	if (config->show_window_title)
	{
		clearline(0, config->colors.windowtitle);
		print(config->colors.windowtitle, 0, 0, title.c_str());
	}

	Window::draw();
	wm->readout->draw();
}

void Wmain::scroll_window(int offset)
{
	int limit = static_cast<int>(content_size() - height() - 1);

	if (limit < 0)
		limit = 0;

	offset = position + offset;

	if (offset < 0)
	{
		offset = 0;
		wm->bell();
	}
	if (offset > limit)
	{
		offset = limit;
		wm->bell();
	}
	
	if ((int)position == offset)
		return;

	position = offset;

	if (config->scroll_mode == SCROLL_MODE_NORMAL)
	{
		if (cursor < position)
			cursor = position;
		else if (cursor > position + height())
			cursor = position + height();
	}
	else if (config->scroll_mode == SCROLL_MODE_CENTERED)
	{
		cursor = position + (height() / 2);
	}
	
	qdraw();
}

void Wmain::set_position(unsigned int absolute)
{
	scroll_window(absolute - position);
}

void Wmain::move_cursor(int offset)
{
	offset = cursor + offset;

	if (offset < 0)
	{
		offset = 0;
		wm->bell();
	}
	else if (offset > (int)content_size() - 1)
	{
		offset = content_size() - 1;
		wm->bell();
	}

	if ((int)cursor == offset)
		return;

	cursor = offset;

	if (config->scroll_mode == SCROLL_MODE_NORMAL)
	{
		if (cursor < position)
			set_position(cursor);
		else if (cursor > position + height())
			set_position(cursor - height());
	}
	else if (config->scroll_mode == SCROLL_MODE_CENTERED)
	{
		offset = floorl(height() / 2);
		if ((int)cursor < offset) {
			position = 0;
		} else if (cursor + offset + 1 >= content_size()) {
			if (content_size() > height())
				position = content_size() - height() - 1;
			else
				position = 0;
		} else {
			position = cursor - offset;
		}
	}
	
	qdraw();
}

void Wmain::set_cursor(unsigned int absolute)
{
	move_cursor(absolute - cursor);
}

bool Wmain::visible()
{
	return wm->active == this;
}
