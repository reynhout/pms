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
#include <string.h>

static command_t command;

static int input_char_get_int(int ch) {
    if (ch >= 48 && ch <= 57) {
        return ch-48;
    } else {
        return -1;
    }
}

static int input_char_get_movement(int ch) {
    if (ch == KEY_UP || ch == 'k') {
        return INPUT_MOVEMENT_UP;
    } else if (ch == KEY_DOWN || ch == 'j') {
        return INPUT_MOVEMENT_DOWN;
    } else if (ch == 'G') {
        return INPUT_MOVEMENT_END;
    }
    return INPUT_MOVEMENT_NONE;
}

static int input_char_get_action(int ch) {
    if (ch == KEY_UP || ch == 'k') {
        return INPUT_ACTION_GO;
    } else if (ch == KEY_DOWN || ch == 'j') {
        return INPUT_ACTION_GO;
    } else if (ch == 'q') {
        return INPUT_ACTION_QUIT;
    } else if (ch == 'g' || ch == 'G') {
        return INPUT_ACTION_GO;
    }
    return INPUT_ACTION_NONE;
}

static int input_command_append_multiplier(int multiplier) {
    multiplier = (command.multiplier*10)+multiplier;
    if (multiplier > command.multiplier) {
        command.multiplier = multiplier;
    }
    return command.multiplier;
}

static void input_command_append_movement(int movement) {
    command.movement = movement;
}

static void input_command_append_action(int action) {
    command.action = action;
    if (command.action != INPUT_ACTION_GO) {
        input_command_append_movement(INPUT_MOVEMENT_NA);
    }
}

static int input_ready() {
    return (command.movement != 0 && command.action != 0);
}

void input_reset() {
    memset(&command, 0, sizeof(command));
}

command_t * input_get() {
    int ch;
    int i;
    if ((ch = curses_get_input()) == ERR) {
        return;
    }
    if ((i = input_char_get_int(ch)) != -1) {
        input_command_append_multiplier(i);
    }
    if ((i = input_char_get_movement(ch)) != INPUT_MOVEMENT_NONE) {
        input_command_append_movement(i);
    }
    if ((i = input_char_get_action(ch)) != INPUT_ACTION_NONE) {
        if (command.action != INPUT_ACTION_NONE && command.action != i) {
            debug("Invalid input sequence with double actions (%d, %d), resetting.\n", command.action, i);
            input_reset();
            return NULL;
        }
        if (command.action == INPUT_ACTION_NONE) {
            input_command_append_action(i);
        } else if (command.movement == INPUT_MOVEMENT_NONE) {
            input_command_append_movement(INPUT_MOVEMENT_NA);
        }
    }
    if (input_ready()) {
        if (command.multiplier == 0) {
            command.multiplier = 1;
        }
        return &command;
    }
    return NULL;
}

int input_handle(command_t * command) {
    debug("input_handle(): multiplier=%d, movement=%d, action=%d\n", command->multiplier, command->movement, command->action);
    if (command->action == INPUT_ACTION_QUIT) {
        shutdown();
        return 0;
    } else if (command->action == INPUT_ACTION_GO) {
        if (command->movement == INPUT_MOVEMENT_UP) {
            console_scroll(-command->multiplier);
        } else if (command->movement == INPUT_MOVEMENT_DOWN) {
            console_scroll(command->multiplier);
        } else if (command->movement == INPUT_MOVEMENT_END) {
            console_scroll_to(-1);
            return 0;
        } else if (command->movement == INPUT_MOVEMENT_NA) {
            console_scroll_to(command->multiplier-1); // convert 1-indexed to 0-indexed
            return 0;
        }
    } else {
        return 0;
    }
    return 1;
}
