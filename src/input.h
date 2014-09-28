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

#define INPUT_MOVEMENT_NA -1
#define INPUT_MOVEMENT_NONE 0
#define INPUT_MOVEMENT_UP 1
#define INPUT_MOVEMENT_DOWN 2
#define INPUT_MOVEMENT_END 3

#define INPUT_ACTION_NONE 0
#define INPUT_ACTION_QUIT 1
#define INPUT_ACTION_GO 2

typedef struct {
    int multiplier;
    int movement;
    int action;

} command_t;

/*
 * Reset input command state to default values.
 */
void input_reset();

/*
 * Retrieve input
 */
command_t * input_get();

/*
 * Perform an action based on a struct command_t.
 */
int input_handle(command_t * command);
