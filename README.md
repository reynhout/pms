# PMS
## Practical Music Search
Practical Music Search is an ncurses-based client for
[MPD](http://mpd.wikia.com/wiki/Music_Player_Daemon_Wiki). It has a command
line interface much like [Vim](http://www.vim.org), and supports custom colors,
layouts, and key bindings. [PMS](https://github.com/ambientsound/pms) aims to
be accessible and highly configurable.

### Compiling
If you are building from Git, you need GNU autoconf and automake.

    $ autoreconf --install
    $ ./configure
    $ make

### Dependencies
You need to have [MPD](http://mpd.wikia.com/wiki/Music_Player_Daemon_Wiki)
installed and working before using [PMS](https://github.com/ambientsound/pms),
but not neccessarily on the same machine. This client requires
[MPD](http://mpd.wikia.com/wiki/Music_Player_Daemon_Wiki) >= 0.15.0.

[PMS](https://github.com/ambientsound/pms) depends on the wide version of
ncurses. On Debian systems this package is called
<code>libncurses5w-dev</code>.

### Configuration
Your configuration file is <code>~/.config/pms/pms.conf</code>. A system-wide
config file can be put in <code>/etc/xdg/pms/pms.conf</code>.

Start PMS and type <code>:set</code> for a list of options available, or type
<code>:set color</code> for a list of color options.

#### Setting options
Options can be set with

    set option=value
    set option+=value
    set option-=value

Boolean options can be set, inverted or unset with

    set option
    set nooption
    set invoption
    set option!

Option values can be queried with

    set option?

#### Changing colors
Background colors are <code>black, red, green, brown, blue, magenta,
cyan</code> and <code>gray</code>.  Foreground colors are as background colors,
in addition to <code>brightred, brightgreen, yellow, brightblue, brightmagenta,
brightcyan, white</code>.

Set colors with

    set color.name=foreground [background]

### Copying
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

See the COPYING file for more information.

### Authors
Written by [Kim Tore Jensen](http://www.incendio.no). Copyright (c) 2006-2011.

A big thanks to contributors [Bart Nagel](https://github.com/tremby) and
[Murilo Pereira](https://github.com/mpereira).

<!--- vi: se ft=markdown et ts=4 sts=4 sw=4: -->
