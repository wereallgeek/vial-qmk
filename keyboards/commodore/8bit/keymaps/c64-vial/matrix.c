#include "quantum.h"
#include "matrix.h"
#include "debounce.h"
#include <string.h>

#define DIRECT_PIN_RESTORE  GP15 
#define DIRECT_PIN_CAPSLOCK GP8  

static const pin_t row_pins[] = { GP18, GP19, GP20, GP21, GP22, GP26, GP27, GP28 };
static const pin_t col_pins[] = { GP0,  GP1,  GP2,  GP3,  GP4,  GP5,  GP6,  GP7 };

static matrix_row_t raw_matrix[MATRIX_ROWS];
static matrix_row_t debounced_matrix[MATRIX_ROWS];

void matrix_init_user(void) {}
void matrix_scan_user(void) {}

void matrix_init(void) {
    // 1. Initialisation des touches directes (actives à l'état bas vers GND)
    setPinInputHigh(DIRECT_PIN_RESTORE);
    setPinInputHigh(DIRECT_PIN_CAPSLOCK);

    // 2. Configuration des colonnes avec résistances de Pull-Down internes
    for (uint8_t i = 0; i < 8; i++) {
        setPinInputLow(col_pins[i]);
    }

    // 3. Configuration des lignes comme sorties initialisées à LOW
    for (uint8_t i = 0; i < 8; i++) {
        setPinOutput(row_pins[i]);
        writePinLow(row_pins[i]);
    }

    // 4. Nettoyage des buffers mémoire
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        raw_matrix[i] = 0;
        debounced_matrix[i] = 0;
    }

    debounce_init(MATRIX_ROWS);
    matrix_init_user();
}

uint8_t matrix_scan(void) {
    bool raw_changed = false;
    matrix_row_t temp_matrix[MATRIX_ROWS] = {0}; 

    // ==========================================
    // ETAPE 1: SCAN DIODELESS EN LOGIQUE ACTIVE-HIGH
    // ==========================================
    for (uint8_t r = 0; r < 8; r++) {
        // On pousse la ligne active à HIGH (3.3V)
        writePinHigh(row_pins[r]);
        
        // Délai de stabilisation pour laisser l'électricité s'établir sur les longs câbles du C64
        wait_us(40); 

        matrix_row_t row_bits = 0;

        // Lecture des colonnes (Un état HIGH signifie que la touche est pressée)
        for (uint8_t c = 0; c < 8; c++) {
            if (readPin(col_pins[c])) {
                row_bits |= (1 << c);
            }
        }
        temp_matrix[r] = row_bits;

        // ÉTAPE CRUCIALE POUR LE SHIFT : On force activement la ligne à LOW 
        // pour vider la capacité résiduelle avant de passer à la suite
        writePinLow(row_pins[r]);
        wait_us(5);
    }

    // ==========================================
    // ETAPE 2: LECTURE DES ENTRÉES DIRECTES (RANGÉE INTERNE 8)
    // ==========================================
    matrix_row_t row_8_bits = 0;
    if (!readPin(DIRECT_PIN_RESTORE)) {
        row_8_bits |= (1 << 0); 
    }
    if (!readPin(DIRECT_PIN_CAPSLOCK)) {
        row_8_bits |= (1 << 1); 
    }
    temp_matrix[8] = row_8_bits;

    // ==========================================
    // ETAPE 3: MISE À JOUR ET ANTI-REBOND (DEBOUNCE)
    // ==========================================
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        if (raw_matrix[r] != temp_matrix[r]) {
            raw_matrix[r] = temp_matrix[r];
            raw_changed = true;
        }
    }

    bool debounced_changed = debounce(raw_matrix, debounced_matrix, MATRIX_ROWS, raw_changed);
    matrix_scan_user();

    return (uint8_t)debounced_changed;
}

matrix_row_t matrix_get_row(uint8_t row) {
    return debounced_matrix[row];
}

bool matrix_is_on(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }
    return (debounced_matrix[row] & ((matrix_row_t)1 << col)) != 0;
}

void matrix_print(void) {}
