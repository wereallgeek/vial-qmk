/* Copyright 2022 Sandipratama
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include QMK_KEYBOARD_H

enum layers {
    _NUM = 0,
    _FN,
    _L2,
    _L3
};

/*
 * Right column (top -> bottom), the 2u footprints come from the old spacebar row.
 * A 2u key only has ONE switch, but both footprints get the same keycode so it
 * works whichever one the 2u centre hole is wired to.
 *
 * Default (A): 2u "+" (R0+R1), 1u "-" (R2), 2u Enter (R3+R4)
 * Uncomment the define below for (B): 1u "-" , 1u "+", 1u "=", 2u Enter
 */
// #define NUMPAD_1U_PLUS

#ifdef NUMPAD_1U_PLUS
#    define RC_R0 KC_PMNS
#    define RC_R1 KC_PPLS
#    define RC_R2 KC_PEQL
#else
#    define RC_R0 KC_PPLS
#    define RC_R1 KC_PPLS
#    define RC_R2 KC_PMNS
#endif
#define RC_R3 KC_PENT
#define RC_R4 KC_PENT

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

/*
 * Numpad, PCB rotated -90 deg (USB side of the board is at the top)
 * ,---------------------------.
 * | NUM  |  /   |  *   |  +   |
 * |------+------+------+  2u  |
 * |  7   |  8   |  9   |      |
 * |------+------+------+------|
 * |  4   |  5   |  6   |  -   |
 * |------+------+------+------|
 * |  1   |  2   |  3   |Enter |
 * |------+------+------+  2u  |
 * |  0   |  .   |BSPC/ |      |
 * |      |      | FN   |      |
 * `---------------------------'
 */
[_NUM] = LAYOUT_numpad_r90(
    KC_NUM,  KC_PSLS, KC_PAST, RC_R0,
    KC_P7,   KC_P8,   KC_P9,   RC_R1,
    KC_P4,   KC_P5,   KC_P6,   RC_R2,
    KC_P1,   KC_P2,   KC_P3,   RC_R3,
    KC_P0,   KC_PDOT, LT(_FN, KC_BSPC), RC_R4
),

/*
 * FN: RGB, media
 * ,---------------------------.
 * |RGB TG|RGB M+|RGB M-| VOLU |
 * |------+------+------+  2u  |
 * | HUI  | SAI  | VAI  |      |
 * |------+------+------+------|
 * | HUD  | SAD  | VAD  | MUTE |
 * |------+------+------+------|
 * | PREV | PLAY | NEXT | VOLD |
 * |------+------+------+  2u  |
 * |      |      |      |      |
 * `---------------------------'
 */
[_FN] = LAYOUT_numpad_r90(
    RGB_TOG, RGB_MOD, RGB_RMOD, KC_VOLU,
    RGB_HUI, RGB_SAI, RGB_VAI,  KC_VOLU,
    RGB_HUD, RGB_SAD, RGB_VAD,  KC_MUTE,
    KC_MPRV, KC_MPLY, KC_MNXT,  KC_VOLD,
    KC_NO,   KC_NO,   KC_TRNS,  KC_VOLD
),

/* Spare layers, free to use from the Vial GUI */
[_L2] = LAYOUT_numpad_r90(
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
),

[_L3] = LAYOUT_numpad_r90(
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
)
};
