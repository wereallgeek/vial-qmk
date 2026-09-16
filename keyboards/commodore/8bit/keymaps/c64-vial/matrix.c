#include "quantum.h"
#include <string.h>

// Define your specific RP2040 GPIO pins for the standalone keys
#define DIRECT_PIN_RESTORE  GP15  // Maps to matrix position [8,0]
#define DIRECT_PIN_CAPSLOCK GP8   // Maps to matrix position [8,1]

// Array containing your exact 8x8 matrix pins from info.json
static const pin_t row_pins[] = { GP18, GP19, GP20, GP21, GP22, GP26, GP27, GP28 };
static const pin_t col_pins[] = { GP0,  GP1,  GP2,  GP3,  GP4,  GP5,  GP6,  GP7  };

void matrix_init_custom(void) {
    // 1. Initialize the standalone RESTORE and CAPSLOCK pins
    setPinInputHigh(DIRECT_PIN_RESTORE);
    setPinInputHigh(DIRECT_PIN_CAPSLOCK);

    // 2. Initialize your diodeless 8x8 matrix pins
    // Set columns as constant Inputs with Pull-ups (COL2ROW default state)
    for (uint8_t i = 0; i < 8; i++) {
        setPinInputHigh(col_pins[i]);
    }
    // Set rows as high-impedance floating inputs initially
    for (uint8_t i = 0; i < 8; i++) {
        setPinInput(row_pins[i]);
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool changed = false;
    matrix_row_t temp_matrix[9] = {0}; // Sized to 9 to hold all row data safely

    // ==========================================
    // STEP 1: SCAN THE DIODELESS 8x8 MATRIX ACTIVE ROWS
    // ==========================================
    for (uint8_t r = 0; r < 8; r++) {
        // Set the active row to Output Low to drive it
        setPinOutput(row_pins[r]);
        writePinLow(row_pins[r]);
        
        // Minor microsecond delay to give the electrical signals time to settle on bare pins
        wait_us(30);

        matrix_row_t row_bits = 0;
        // Read each column on this active row
        for (uint8_t c = 0; c < 8; c++) {
            // COL2ROW: Active low reading (0V) means the key is pressed
            if (!readPin(col_pins[c])) {
                row_bits |= (1 << c);
            }
        }
        temp_matrix[r] = row_bits;

        // Reset the row back to a safe floating Input state to isolate it
        setPinInput(row_pins[r]);
    }

    // ==========================================
    // STEP 2: SCAN THE 2 DIRECT PINS (ROW 8)
    // ==========================================
    matrix_row_t row_8_bits = 0;
    if (!readPin(DIRECT_PIN_RESTORE)) {
        row_8_bits |= (1 << 0); // Position [8,0]
    }
    if (!readPin(DIRECT_PIN_CAPSLOCK)) {
        row_8_bits |= (1 << 1); // Position [8,1]
    }
    temp_matrix[8] = row_8_bits;

    // ==========================================
    // STEP 3: FLUSH CHANGES INTO THE CORE MATRIX
    // ==========================================
    if (memcmp(current_matrix, temp_matrix, sizeof(temp_matrix)) != 0) {
        memcpy(current_matrix, temp_matrix, sizeof(temp_matrix));
        changed = true;
    }

    return changed;
}
