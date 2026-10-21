#pragma once

// Matrice logicielle: 8x8 (clavier) + 1 rangée "hors matrice" (Caps Lock sur GP8)
#undef MATRIX_ROWS
#define MATRIX_ROWS 9

#undef MATRIX_COLS
#define MATRIX_COLS 8

#define MATRIX_HAS_GHOST

#define VIAL_KEYBOARD_UID {0xDF, 0xC3, 0x7B, 0x2F, 0xFC, 0x79, 0xCC, 0x8E}

#define VIAL_UNLOCK_COMBO_ROWS { 1, 1, 7 }
#define VIAL_UNLOCK_COMBO_COLS { 0, 5, 6 }

// LAYOUT C16 + Caps Lock (rangée 8, colonne 0).
// Meme matrice 8x8 que LAYOUT_c16 (info.json); k21 = Caps Lock, entre Run/Stop et A.
#define LAYOUT_c16_caps( \
    k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k0A, k0B, k0C, k0D, k0E, k0F, k0G,  \
    k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k1A, k1B, k1C, k1D, k1E, k1F,  \
    k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k2A, k2B, k2C, k2D, k2E, k2F,  \
    k30, k31, k32, k33, k34, k35, k36, k37, k38, k39, k3A, k3B, k3C, k3D, k3E,  \
    k40 \
) { \
    { k39, k3A, k2B, k1D, k2A, k1A, k0D, k0E }, \
    { k20, k40, k30, k11, k10, k1E, k02, k01 }, \
    { k31, k32, k23, k13, k22, k12, k04, k03 }, \
    { k35, k36, k27, k17, k26, k16, k08, k07 }, \
    { k33, k34, k25, k15, k24, k14, k06, k05 }, \
    { k37, k38, k29, k19, k28, k18, k0A, k09 }, \
    { k3B, k00, k3D, k1C, k2C, k2D, k0C, k0B }, \
    { k1B, k0G, k1F, k2F, k3C, k2E, k3E, k0F }, \
    { k21, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO } \
}
