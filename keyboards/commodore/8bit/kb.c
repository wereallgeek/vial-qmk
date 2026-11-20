// kb.c - USB / PS/2 dual-mode keyboard (mode chosen by MODE_SWITCH_PIN: High = USB, Low = PS/2).
// Optional build flag CAPS_LATCHING=yes: the off-matrix Caps Lock key is a latching switch.
#include "kb.h"
#include "ps2_keyboard.h"
#include "print.h"
#include "host.h"

#ifdef CAPS_LATCHING
// Matrix row of the off-matrix (direct pin) keys: 8 on both the C16 and C64 builds
#    ifndef CAPS_LATCH_ROW
#        define CAPS_LATCH_ROW 8
#    endif

// Caps Lock state as best we know it. The host LED report is the truth (USB and PS/2
// hosts that send 0xED); whenever it changes we adopt it. Between reports we trust our
// own taps, so a host that never reports LEDs still gets sensible behaviour.
static bool    caps_assumed   = false;
static bool    caps_last_led  = false;
static uint8_t caps_last_seq  = 0;

static bool caps_lock_now(void) {
    bool led = host_keyboard_led_state().caps_lock;

    // USB: the host always reports the real state, so the LED is the truth. No memory.
    if (is_usb_mode()) {
        caps_last_led = led;
        caps_assumed  = led;
        return led;
    }

    // PS/2: a host does not necessarily report every change, but when it does report
    // (even with the same value as before) that report is the truth. Count reports so
    // an unchanged value is not mistaken for "no news". Between reports we trust our taps.
    uint8_t seq = ps2_keyboard_led_report_count();
    if (led != caps_last_led || seq != caps_last_seq) {
        caps_last_led = led;
        caps_last_seq = seq;
        caps_assumed  = led;
    }
    return caps_assumed;
}
#endif

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

static void write_led_pins(led_t led) {
    writePin(LED_CAPS_LOCK_PIN, led.caps_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
    writePin(LED_NUM_LOCK_PIN, led.num_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
    writePin(LED_SCROLL_LOCK_PIN, led.scroll_lock ? LED_PIN_ON_STATE : !LED_PIN_ON_STATE);
}

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
    // First pass after power-up: read the mode switch and act at once (no 50 ms debounce,
    // no USB clean-up delay) so the PS/2 side is up as early as possible.
    static bool first_run = true;
    bool current_mode = readPin(MODE_SWITCH_PIN);

    // Check for mode mismatch (current pin vs last known mode)
    // This handles both runtime switching AND initial boot detection
    if (current_mode != last_mode) {
        if (mode_change_time == 0 && !first_run) {
            mode_change_time = timer_read32();
        } else if (first_run || timer_elapsed32(mode_change_time) > 50) {
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
                // (not at power-up: nothing to clear, and it would forget held keys)
                if (!first_run) {
                    clear_keyboard();
                    wait_ms(20);
                }

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
        // Host finished its init: announce keys that were already held at power-up
        if (ps2_keyboard_replay_due()) {
            send_keyboard_report();
        }
    }
    update_status_led();
    first_run = false;

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

#ifdef CAPS_LATCHING
    // Latching Caps Lock switch (build with CAPS_LATCHING=yes).
    // The switch stays closed while latched. Contact made = Caps Lock must be ON,
    // contact released = Caps Lock must be OFF, whatever the state was before:
    // we only send KC_CAPS when the current state differs from what we want.
    // Only the direct-pin row is affected, a KC_CAPS placed on the normal matrix
    // keeps its usual behaviour.
    if (keycode == KC_CAPS && record->event.key.row == CAPS_LATCH_ROW) {
        bool want = record->event.pressed;
        bool now  = caps_lock_now();
        uprintf("[CAPS] switch %s: caps_now=%d want=%d -> %s\n", want ? "closed" : "open",
                now, want, (now != want) ? "toggle" : "nothing");
        if (now != want) {
            tap_code_delay(KC_CAPS, 10);
            caps_assumed = want;  // host LED report will confirm
        }
        return false;
    }
#endif

    return true;
}

bool led_update_kb(led_t led_state) {
    write_led_pins(led_state);

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
#ifdef CAPS_LATCHING
    // Follow the host Caps Lock LED on every scan, not only when the switch moves,
    // otherwise a change made on the PC between two switch events is missed.
    caps_lock_now();
#endif

    // Read the current lock state
    led_t current_led_state = host_keyboard_led_state();
    uint8_t raw_state = current_led_state.raw;

    // If lock state changes while we are in PS/2 mode
    if (!usb_mode && raw_state != last_ps2_led_state) {
        last_ps2_led_state = raw_state;
        
        // Push the state to the physical pins (the PS/2 host never goes through led_update_kb)
        write_led_pins(current_led_state);
    }

    // Keep your user-level keymap scanning active
    matrix_scan_user();
}
