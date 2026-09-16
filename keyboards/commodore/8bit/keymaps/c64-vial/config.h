#pragma once

// Configuration de la taille de la matrice logicielle
#undef MATRIX_ROWS
#define MATRIX_ROWS 9

#undef MATRIX_COLS
#define MATRIX_COLS 8

// Protection anti-ghosting pour clavier diodeless C64
#define MATRIX_HAS_GHOST

#define VIAL_KEYBOARD_UID {0xDF, 0xC3, 0x7B, 0x2F, 0xFC, 0x79, 0xCC, 0x8E}

// Vial unlock mapped to Run/Stop (7,3), C= (7,5), and Restore (8,0)
#define VIAL_UNLOCK_COMBO_ROWS { 7, 7, 8 }
#define VIAL_UNLOCK_COMBO_COLS { 3, 5, 0 }

// LAYOUT MACRO: Cartographie stricte 1 pour 1 sans AUCUN duplicata
#define LAYOUT_c64( \
    k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k0A, k0B, k0C, k0D, k0E, k0F, k0G, \
    k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k1A, k1B, k1C, k1D,      k1G, \
    k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k2A, k2B, k2C, k2D,      k2G, \
    k30,      k31, k32, k33, k34, k35, k36, k37, k38, k39, k3A, k3B, k3C, k3D, k3E, k3G, \
    k40,                                                   k41, k42 \
) { \
    /* Row 0: DEL, Return, CRSR L/R, CRSR U/D, F1, F3, F5, F7 */ \
    { k0F, k2D, k3E, k3D, k0G, k1G, k2G, k3G }, \
    /* Row 1: 3, W, A, Shift Left, Z, S, E, 4 */ \
    { k03, k12, k21, k31, k32, k22, k13, k04 }, \
    /* Row 2: 5, R, D, X, C, F, T, 6 */ \
    { k05, k14, k23, k33, k34, k24, k15, k06 }, \
    /* Row 3: 7, Y, G, V, B, H, U, 8 */ \
    { k07, k16, k25, k35, k36, k26, k17, k08 }, \
    /* Row 4: 9, I, J, N, M, K, O, 0 */ \
    { k09, k18, k27, k37, k38, k28, k19, k0A }, \
    /* Row 5: +, P, L, ,, ., :, @, - */ \
    { k0B, k1A, k29, k39, k3A, k2A, k1B, k0C }, \
    /* Row 6: £, *, ;, /, Shift Right, =, ↑, Home */ \
    { k0D, k1C, k2B, k3B, k3C, k2C, k1D, k0E }, \
    /* Row 7: 1, Flèche Gauche, Control, Run/Stop, Space, C=, Q, 2 */ \
    { k01, k00, k10, k20, k40, k30, k11, k02 }, \
    /* Row 8: Touches directes (RESTORE sur GP15, CAPS sur GP8) */ \
    { k41, k42, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }  \
}
