/* Copyright 2022 Sandipratama <https://github.com/nendezkombet>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

/*
 * USB ids, manufacturer, product name and the key layout live in info.json.
 *
 * Electrical matrix of ONE KFM PCB (4 rows x 5 columns), unchanged.
 * The PCB is mounted rotated 90 degrees counter-clockwise, this rotation is
 * described by the "matrix" entry of each key in info.json.
 *
 * Single board = the first five columns of the original 4x10 matrix
 * (COL1..COL5 on the PCB legend). If no key responds, try the other five:
 *   #define MATRIX_COL_PINS { C6, D7, E6, B4, B5 }
 *
 * The matrix size (4 rows x 5 cols) is declared in info.json ("matrix_size"),
 * not here, so the QMK CLI never has to evaluate MATRIX_ROWS/MATRIX_COLS.
 */
#define MATRIX_ROW_PINS { F5, F6, F7, B1 }
#define MATRIX_COL_PINS { B3, B2, D1, D0, D4 }

/* COL2ROW, ROW2COL */
#define DIODE_DIRECTION COL2ROW

/* RGB (SK6812 mini-E on the PCB data line) */
#define WS2812_DI_PIN F4
#define RGBLED_NUM 20 /* one SK6812 per key footprint on a single board */
#define RGBLIGHT_HUE_STEP 8
#define RGBLIGHT_SAT_STEP 8
#define RGBLIGHT_VAL_STEP 8
#define RGBLIGHT_LIMIT_VAL 200 /* The maximum brightness level */
#define RGBLIGHT_SLEEP  /* RGB off when the host sleeps */
#define RGBLIGHT_EFFECT_BREATHING
#define RGBLIGHT_EFFECT_RAINBOW_MOOD
#define RGBLIGHT_EFFECT_RAINBOW_SWIRL

/* Debounce reduces chatter (unintended double-presses) - set 0 if debouncing is not needed */
#define DEBOUNCE 5

#define TAPPING_TERM 200
