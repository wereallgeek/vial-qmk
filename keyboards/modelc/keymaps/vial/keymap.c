/* Model C is a Commodore 16 matrix implementation inside a Model-H controller
 * Copyrights 2026 Marsupial
 * 
 * Based on model-h by John Hawthorn 2020
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

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT_all(
    //ESC                                                                                                [--CURSOR---KEYS---ARE---HERE--] BackSpace
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_LEFT, KC_RGHT, KC_UP,   KC_DOWN, KC_BSPC,   KC_F1,
    
    // CTRL                                                                                              @          +        -      HOME
    KC_LCTL, KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_MAIL, KC_PPLS, KC_PMNS, KC_HOME,            KC_F2,
    
    // PAUSE                                                                                               :         ;        *      ENTER
    KC_PAUS,           KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_LBRC, KC_RBRC, KC_PAST, KC_ENT,            KC_F3,
    
    // WIN     shift                                                                     ,      .        /          britishpound       =                F4
    KC_LGUI, KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,          KC_NUBS,  KC_PEQL,           KC_F4,
     
    //SPCBAR
    KC_SPC
  )
};

