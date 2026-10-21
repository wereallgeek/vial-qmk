#include "quantum.h"
#include "matrix.h"

// Meme cablage que le scan standard QMK (info.json, COL2ROW):
// une rangée à la fois est tirée à LOW, les colonnes sont lues avec pull-up.
#define DIRECT_PIN_CAPSLOCK GP8   // interrupteur vers GND, actif à l'état bas (comme le C64)
#define CAPS_ROW 8
#define CAPS_COL 0

static const pin_t row_pins[] = { GP18, GP19, GP20, GP21, GP22, GP26, GP27, GP28 };
static const pin_t col_pins[] = { GP0,  GP1,  GP2,  GP3,  GP4,  GP5,  GP6,  GP7 };

void matrix_init_custom(void) {
    for (uint8_t c = 0; c < 8; c++) {
        setPinInputHigh(col_pins[c]);
    }
    for (uint8_t r = 0; r < 8; r++) {
        setPinInputHigh(row_pins[r]);  // rangée non sélectionnée = haute impédance
    }
    setPinInputHigh(DIRECT_PIN_CAPSLOCK);
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;

    for (uint8_t r = 0; r < 8; r++) {
        setPinOutput(row_pins[r]);
        writePinLow(row_pins[r]);
        wait_us(30);

        matrix_row_t row_bits = 0;
        for (uint8_t c = 0; c < 8; c++) {
            if (!readPin(col_pins[c])) {
                row_bits |= ((matrix_row_t)1 << c);
            }
        }

        setPinInputHigh(row_pins[r]);
        wait_us(30);

        if (current_matrix[r] != row_bits) {
            current_matrix[r] = row_bits;
            changed = true;
        }
    }

    // Touche directe (rangée 8)
    matrix_row_t direct_bits = readPin(DIRECT_PIN_CAPSLOCK) ? 0 : ((matrix_row_t)1 << CAPS_COL);
    if (current_matrix[CAPS_ROW] != direct_bits) {
        current_matrix[CAPS_ROW] = direct_bits;
        changed = true;
    }

    return changed;
}
