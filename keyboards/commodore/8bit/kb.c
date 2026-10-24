// keyboards/bjl/ps2demo/kb.c - FIXED VERSION with proper USB driver restoration
#include "kb.h"
#include "ps2_keyboard.h"
#include "print.h"
#include "host.h"

// Fault indicator on the Pico on-board LED (GP25). Dark during normal operation;
// it only lights up when something is wrong with the PS/2 link:
//   5 Hz blink = host disabled the keyboard (0xF5) and never re-enabled it
//   solid on   = host has held CLK or DATA low for more than 1 s
// Optional: #define PS2_STATUS_HEARTBEAT_MS 10000 (in config.h) adds a short 50 ms blip
// at that interval while healthy, so that a hung Pico can be told apart from a healthy one.
// Use a different pin with: #define PS2_STATUS_LED_PIN GPxx
#ifndef PS2_STATUS_LED_PIN
#    define PS2_STATUS_LED_PIN GP25
#endif
#ifndef PS2_STATUS_HEARTBEAT_MS
#    define PS2_STATUS_HEARTBEAT_MS 0
#endif

static void update_status_led(void) {
    bool on = false;
    if (!is_usb_mode()) {
        switch (ps2_keyboard_status()) {
            case PS2_STATUS_BUS_HELD:
                on = true;
                break;
            case PS2_STATUS_DISABLED:
                on = (timer_read32() / 100) & 1;
                break;
            default:
#if PS2_STATUS_HEARTBEAT_MS > 0
                on = (timer_read32() % PS2_STATUS_HEARTBEAT_MS) < 50;
#endif
                break;
        }
    }
    writePin(PS2_STATUS_LED_PIN, on);
}

// Mode state
static bool usb_mode = true;
static bool last_mode = true;

// Store original USB driver to restore later
static host_driver_t *original_usb_driver = NULL;

// Track the last known LED state to prevent constant unneeded pin writes
static uint8_t last_ps2_led_state = 0xFF;

bool is_usb_mode(void) {
    return usb_mode;
}

bool is_ps2_mode(void) {
    return !usb_mode;
}

void keyboard_pre_init_kb(void) {
    setPinInputHigh(MODE_SWITCH_PIN);
    setPinOutput(PS2_STATUS_LED_PIN);
    writePinLow(PS2_STATUS_LED_PIN);
    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    static uint32_t mode_change_time = 0;
    bool current_mode = readPin(MODE_SWITCH_PIN);

    // Check for mode mismatch (current pin vs last known mode)
    // This handles both runtime switching AND initial boot detection
    if (current_mode != last_mode) {
        if (mode_change_time == 0) {
            mode_change_time = timer_read32();
        } else if (timer_elapsed32(mode_change_time) > 50) {
            // Mode stable for 50ms - switch!
            last_mode = current_mode;
            usb_mode = current_mode;
            mode_change_time = 0;

            uprintf("================================\n");
            uprintf("Mode switch: %s\n", usb_mode ? "USB" : "PS/2");
            uprintf("================================\n");

            if (!usb_mode) {
                // ===== Switching TO PS/2 =====

                // CRITICAL: Capture the driver here, where we know it is valid
                if (original_usb_driver == NULL) {
                    original_usb_driver = host_get_driver();
                }

                // Clear USB keyboard state while USB driver is still active
                clear_keyboard();
                wait_ms(20);

                // Now switch to PS/2
                ps2_keyboard_init(PS2_KEYBOARD_CLOCK_PIN, PS2_KEYBOARD_DATA_PIN);
                host_set_driver(&ps2_keyboard_host_driver);
                last_ps2_led_state = 0xFF;  // force LED pins to resync from the PS/2 state
                uprintf("[PS2] PS/2 driver activated\n");

            } else {
                // ===== Switching TO USB =====
                // Disable PS/2 typematic first
                ps2_keyboard_typematic_disable();

                // Restore the original USB driver
                if (original_usb_driver != NULL) {
                    host_set_driver(original_usb_driver);
                    uprintf("[USB] USB driver restored\n");
                } else {
                    uprintf("[USB] ERROR: original_usb_driver is NULL!\n");
                }

                wait_ms(20);

                // Clear keyboard state in USB mode
                clear_keyboard();
                send_keyboard_report();

                wait_ms(20);

                // Re-apply the last LED state the USB host gave us
                led_update_kb(host_keyboard_led_state());
            }
        }
    } else {
        mode_change_time = 0;
    }

    // Run PS/2 task only in PS/2 mode
    if (!usb_mode) {
        ps2_keyboard_task();
    }
    update_status_led();

    housekeeping_task_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // In PS/2 mode, ensure the correct driver is set BEFORE processing
    if (!usb_mode) {
        if (host_get_driver() != &ps2_keyboard_host_driver) {
            uprintf("[ERROR] Wrong driver in PS/2 mode! Fixing...\n");
            host_set_driver(&ps2_keyboard_host_driver);
        }
    }

    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (record->event.pressed) {
        uprintf("[DEBUG] Key pressed: keycode=0x%04X (%s mode)\n",
                keycode, usb_mode ? "USB" : "PS/2");
    } else {
        uprintf("[DEBUG] Key released: keycode=0x%04X (%s mode)\n",
                keycode, usb_mode ? "USB" : "PS/2");
    }

    return true;
}

bool led_update_kb(led_t led_state) {
    writePin(LED_CAPS_LOCK_PIN, led_state.caps_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
    writePin(LED_NUM_LOCK_PIN, led_state.num_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
    writePin(LED_SCROLL_LOCK_PIN, led_state.scroll_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);

    // In PS/2 mode, block LED updates from USB
    // This prevents USB from sending any HID reports
    if (!usb_mode) {
        return false;  // Don't process LED updates in PS/2 mode
    }
    return led_update_user(led_state);
}

void matrix_init_kb(void) {
    matrix_init_user();
}


void matrix_scan_kb(void) {
    // Read the current lock state
    led_t current_led_state = host_keyboard_led_state();
    uint8_t raw_state = current_led_state.raw;

    // If lock state changes while we are in PS/2 mode
    if (!usb_mode && raw_state != last_ps2_led_state) {
        last_ps2_led_state = raw_state;
        
        // Manually push the state to the physical pins
        writePin(LED_CAPS_LOCK_PIN, current_led_state.caps_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
        writePin(LED_NUM_LOCK_PIN, current_led_state.num_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
        writePin(LED_SCROLL_LOCK_PIN, current_led_state.scroll_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
    }

    // Keep your user-level keymap scanning active
    matrix_scan_user();
}



