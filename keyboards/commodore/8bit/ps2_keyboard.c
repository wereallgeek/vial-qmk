// ps2_keyboard.c - FIXED VERSION with better media key debugging
#include "ps2_keyboard.h"
#include "quantum.h"  // QMK main header with GPIO functions

#include "report.h"  // For report_keyboard_t, etc.

// Timing (in microseconds)
#define PS2_CLK_HALF_PERIOD 50  // 50us = 10kHz clock (was 40us = 12.5kHz)
#define PS2_INTER_BYTE_DELAY 2  // 2ms delay between bytes

// Previous keyboard report to detect key changes device
static report_keyboard_t previous_report = {0};

// Configuration
static uint8_t ps2_clk_pin;
static uint8_t ps2_data_pin;

// State variables
static ps2_state_t ps2_state = PS2_STATE_IDLE;
static bool ps2_enabled = true;
static ps2_led_state_t ps2_leds = {0};

// Special key send functions
bool ps2_keyboard_send_printscreen_make(void);
bool ps2_keyboard_send_printscreen_break(void);
bool ps2_keyboard_send_pause(void);  // Pause only sends on make, no break!
bool ps2_keyboard_send_raw_byte(uint8_t byte);

// Send buffer
#define PS2_SEND_BUFFER_SIZE 32
static uint8_t send_buffer[PS2_SEND_BUFFER_SIZE];
static uint8_t send_buffer_head = 0;
static uint8_t send_buffer_tail = 0;

// Previous media key to handle repeats
static uint16_t previous_media_key = 0;

// Convert Consumer Control usage code to PS/2 scancode
static ps2_mapping_t consumer_to_ps2_scancode(uint16_t usage) {
    for (size_t i = 0; i < PS2_CONSUMER_MAPPINGS_SIZE; i++) {
        if (ps2_consumer_mappings[i].usage_code == usage) {
            return ps2_consumer_mappings[i].mapping;
        }
    }
    // Unknown consumer control code
    uprintf("[PS2] UNMAPPED consumer control: 0x%04X\n", usage);
    return (ps2_mapping_t){0, false, PS2_KEY_NORMAL};
}

// Convert QMK keycode to PS/2 scancode
ps2_mapping_t qmk_to_ps2_scancode(uint16_t keycode) {
    // FIXED: Check basic keycodes in main lookup table
    // Only use the lookup table if keycode is within bounds AND < 256
    if (keycode < 0x100 && keycode < PS2_SCANCODE_LOOKUP_SIZE) {
        ps2_mapping_t mapping = ps2_scancode_lookup[keycode];
        if (mapping.scancode != 0) {
            return mapping;
        }
    }

    // Check extended keys (media, system, F13-F24)
    // This handles all keycodes >= 0x100 and any unmapped basic keys
    for (size_t i = 0; i < PS2_EXTENDED_KEYS_SIZE; i++) {
        if (ps2_extended_keys[i].qmk_keycode == keycode) {
            return ps2_extended_keys[i].mapping;
        }
    }

    // Unknown keycode - log it for debugging
    uprintf("[PS2] UNMAPPED keycode: 0x%04X\n", keycode);
    return (ps2_mapping_t){0, false, PS2_KEY_NORMAL};
}

typedef struct {
    uint8_t mod_bit;
    uint8_t scancode;
    bool needs_e0;
    ps2_special_key_type_t special_type;
} ps2_modifier_mapping_t;

static const ps2_modifier_mapping_t modifier_mappings[] = {
    {0x01, PS2_LCTRL, false, PS2_KEY_NORMAL},   // MOD_LCTL
    {0x02, PS2_LSHIFT, false, PS2_KEY_NORMAL},  // MOD_LSFT
    {0x04, PS2_LALT, false, PS2_KEY_NORMAL},    // MOD_LALT
    {0x08, PS2_LGUI, true, PS2_KEY_NORMAL},     // MOD_LGUI (needs E0)
    {0x10, PS2_RCTRL, true, PS2_KEY_NORMAL},    // MOD_RCTL (needs E0)
    {0x20, PS2_RSHIFT, false, PS2_KEY_NORMAL},  // MOD_RSFT
    {0x40, PS2_RALT, true, PS2_KEY_NORMAL},     // MOD_RALT (needs E0)
    {0x80, PS2_RGUI, true, PS2_KEY_NORMAL},     // MOD_RGUI (needs E0)
};

// Typematic state (Needed because PS/2 device must handle repeats itself unlike USB)
static struct {
    uint16_t keycode;       // Which QMK keycode is held
    bool active;            // Is typematic armed?
    uint32_t press_time;    // When key was first pressed
    uint32_t last_repeat;   // When we last sent a repeat
    uint16_t delay_ms;      // Delay before repeating starts
    uint16_t rate_ms;       // Time between repeats
    ps2_mapping_t mapping;  // Full mapping info (scancode + E0 prefix flag)
} typematic_state = {
    .keycode = 0,
    .active = false,
    .press_time = 0,
    .last_repeat = 0,
    .delay_ms = 500,  // Default 500ms delay
    .rate_ms = 33,     // Default ~30Hz repeat rate
    .mapping = {0, false, PS2_KEY_NORMAL}
};

void ps2_keyboard_typematic_arm(uint16_t keycode, uint8_t scancode) {
    // Don't arm typematic for modifier keys
    if ((keycode >= KC_LCTL && keycode <= KC_RGUI) ||  // Modifiers
        keycode == KC_CAPS ||  // Caps Lock
        keycode == KC_NUM ||   // Num Lock
        keycode == KC_SCRL ||  // Scroll Lock
        keycode == KC_PSCR ||  // Print Screen
        keycode == KC_PAUS) {  // Pause
        return;
    }

    typematic_state.keycode = keycode;
    typematic_state.active = true;
    typematic_state.press_time = timer_read32();
    typematic_state.last_repeat = timer_read32();

    // Store the complete mapping to preserve E0 prefix info
    typematic_state.mapping = qmk_to_ps2_scancode(keycode);
}

void ps2_keyboard_typematic_stop(uint16_t keycode) {
    if (typematic_state.keycode == keycode) {
        typematic_state.active = false;
    }
}

void ps2_keyboard_typematic_disable(void) {
    // Completely disable typematic (used when switching modes)
    typematic_state.active = false;
    typematic_state.keycode = 0;
    typematic_state.mapping.scancode = 0;
    typematic_state.mapping.needs_e0_prefix = false;
    typematic_state.mapping.special_type = PS2_KEY_NORMAL;
}

void ps2_keyboard_typematic_task(void) {
    if (!typematic_state.active) return;

    uint32_t now = timer_read32();
    uint32_t held_time = now - typematic_state.press_time;

    // Has initial delay passed?
    if (held_time >= typematic_state.delay_ms) {
        uint32_t since_repeat = now - typematic_state.last_repeat;

        // Time for another repeat?
        if (since_repeat >= typematic_state.rate_ms) {
            uprintf("[PS2] Typematic repeat: keycode=0x%04X, scancode=0x%02X%s\n",
                    typematic_state.keycode, typematic_state.mapping.scancode,
                    typematic_state.mapping.needs_e0_prefix ? ", E0 prefix" : "");

            if (typematic_state.mapping.needs_e0_prefix) {
                ps2_keyboard_send_key_make(PS2_PREFIX_E0);
            }
            ps2_keyboard_send_key_make(typematic_state.mapping.scancode);
            typematic_state.last_repeat = now;
        }
    }
}

// Helper functions using QMK GPIO API
static inline void ps2_clk_high(void) {
    setPinInput(ps2_clk_pin);  // Release to pullup (high-Z with pullup)
}

static inline void ps2_clk_low(void) {
    writePinLow(ps2_clk_pin);
    setPinOutput(ps2_clk_pin);
}

static inline void ps2_data_high(void) {
    setPinInput(ps2_data_pin);  // Release to pullup (high-Z with pullup)
}

static inline void ps2_data_low(void) {
    writePinLow(ps2_data_pin);
    setPinOutput(ps2_data_pin);
}

static inline bool ps2_clk_read(void) {
    return readPin(ps2_clk_pin);
}

static inline bool ps2_data_read(void) {
    return readPin(ps2_data_pin);
}

// ---------------------------------------------------------------------------
// Bus helpers
// ---------------------------------------------------------------------------

// Length of one clock phase (low or high). The original code used
// PS2_CLK_HALF_PERIOD * 2 everywhere, i.e. 100us per phase (~5 kHz clock).
#define PS2_PHASE_US (PS2_CLK_HALF_PERIOD * 2)

// Host->device receive results
#define PS2_RX_OK       0
#define PS2_RX_ABORT   -1   // host aborted / framing error, nothing to answer
#define PS2_RX_PARITY  -2   // bad parity: answer with RESEND

// Multi-byte host command (0xED, 0xF0, 0xF3) waiting for its argument byte
static uint8_t ps2_pending_cmd = 0;

static inline void ps2_release_bus(void) {
    ps2_data_high();
    ps2_clk_high();
}

// Bus is idle when the host is neither inhibiting (CLK low)
// nor requesting to send (DATA low).
static inline bool ps2_bus_idle(void) {
    return ps2_clk_read() && ps2_data_read();
}

// One clock cycle generated by the device. Returns false if CLK is still low
// after we released it: the host is inhibiting the bus and we must abort.
static bool ps2_clock_cycle(void) {
    ps2_clk_low();
    wait_us(PS2_PHASE_US);
    ps2_clk_high();
    wait_us(PS2_PHASE_US);
    return ps2_clk_read();
}

// ---------------------------------------------------------------------------
// Device -> host
// ---------------------------------------------------------------------------

// Returns false (nothing sent, or frame aborted) if the host is inhibiting the
// bus or asking to send. The caller keeps the byte and retries later.
static bool ps2_send_byte(uint8_t data) {
    uint8_t parity = 1;

    // Bus must be idle before we start
    if (!ps2_bus_idle()) return false;
    wait_us(50);
    if (!ps2_bus_idle()) return false;

    // Start bit
    ps2_data_low();
    wait_us(PS2_PHASE_US);  // Data setup time
    if (!ps2_clock_cycle()) goto aborted;

    // Data bits (LSB first)
    for (int i = 0; i < 8; i++) {
        if (data & (1 << i)) {
            ps2_data_high();
            parity ^= 1;
        } else {
            ps2_data_low();
        }
        wait_us(PS2_PHASE_US);  // Data setup time
        if (!ps2_clock_cycle()) goto aborted;
    }

    // Parity bit (odd parity)
    if (parity) {
        ps2_data_high();
    } else {
        ps2_data_low();
    }
    wait_us(PS2_PHASE_US);
    if (!ps2_clock_cycle()) goto aborted;

    // Stop bit - data MUST be high.
    // The host can only inhibit BEFORE this (11th) clock pulse. Once the stop
    // bit has been clocked the byte is delivered, and the host may pull CLK low
    // immediately to send a command (e.g. 0xED right after Caps Lock). That must
    // NOT be treated as an abort, otherwise the byte is resent and the host
    // sees it twice (caps ON then OFF, stray ACKs, desynchronised parser).
    ps2_data_high();
    wait_us(PS2_PHASE_US);
    ps2_clk_low();
    wait_us(PS2_PHASE_US);
    ps2_clk_high();
    wait_us(PS2_PHASE_US / 2);

    // Inter-byte delay with both lines idle
    ps2_release_bus();
    wait_us(300);
    return true;

aborted:
    // Host pulled CLK low mid-frame: let go of everything, the byte stays queued
    ps2_release_bus();
    return false;
}

// Send a command response right now (bypasses the key buffer). Retries for a
// short while if the host is inhibiting the bus.
static void ps2_send_response(uint8_t byte) {
    uint32_t start = timer_read32();
    while (!ps2_send_byte(byte)) {
        if (timer_elapsed32(start) > 20) {
            uprintf("[PS2] Response 0x%02X dropped (bus busy)\n", byte);
            return;
        }
        wait_us(50);
    }
}

// ---------------------------------------------------------------------------
// Host -> device
// ---------------------------------------------------------------------------

// Host request-to-send: CLK released (high) while DATA is held low.
static bool ps2_host_request_pending(void) {
    if (!ps2_clk_read() || ps2_data_read()) return false;
    wait_us(10);  // small glitch filter
    return ps2_clk_read() && !ps2_data_read();
}

// Clock in one byte from the host. The device generates the clock; the host
// changes DATA while CLK is low, and we sample it once CLK is back high.
// Call only after ps2_host_request_pending() returned true.
static int8_t ps2_receive_byte(uint8_t *out) {
    uint8_t data = 0;

    wait_us(PS2_PHASE_US / 2);

    // 8 data bits, LSB first
    for (int i = 0; i < 8; i++) {
        if (!ps2_clock_cycle()) goto aborted;
        if (ps2_data_read()) data |= (1 << i);
    }

    // Parity bit
    if (!ps2_clock_cycle()) goto aborted;
    uint8_t parity_bit = ps2_data_read() ? 1 : 0;

    // Stop bit (host releases DATA, must read high)
    if (!ps2_clock_cycle()) goto aborted;
    if (!ps2_data_read()) goto aborted;

    // ACK: pull DATA low for one more clock cycle
    ps2_data_low();
    wait_us(10);
    ps2_clock_cycle();
    ps2_release_bus();

    *out = data;
    // Odd parity: data bits + parity bit must contain an odd number of ones
    if (((__builtin_popcount(data) + parity_bit) & 1) == 0) {
        return PS2_RX_PARITY;
    }
    return PS2_RX_OK;

aborted:
    ps2_release_bus();
    return PS2_RX_ABORT;
}

static void ps2_reset_defaults(void) {
    typematic_state.delay_ms = 500;
    typematic_state.rate_ms  = 33;
    ps2_keyboard_typematic_disable();
}

// Set Typematic argument: bits 0-4 rate, bits 5-6 delay
static void ps2_apply_typematic(uint8_t arg) {
    uint8_t a = arg & 0x07;
    uint8_t b = (arg >> 3) & 0x03;
    typematic_state.delay_ms = (uint16_t)((((arg >> 5) & 0x03) + 1) * 250);
    typematic_state.rate_ms  = (uint16_t)((((8 + a) << b) * 417UL) / 100);  // (8+A) * 2^B * 4.17ms
}

// Process one byte received from the host (command or command argument)
static void ps2_handle_command(uint8_t cmd) {
    // Argument of a previous two-byte command?
    if (ps2_pending_cmd) {
        uint8_t pending = ps2_pending_cmd;
        ps2_pending_cmd = 0;

        // A byte with bit 7 set is never a valid argument: treat it as a new command
        if (!(cmd & 0x80)) {
            switch (pending) {
                case PS2_CMD_SET_LEDS:
                    // bit0 = Scroll, bit1 = Num, bit2 = Caps
                    ps2_leds.scroll_lock = (cmd >> 0) & 1;
                    ps2_leds.num_lock    = (cmd >> 1) & 1;
                    ps2_leds.caps_lock   = (cmd >> 2) & 1;
                    uprintf("[PS2] LEDs: scroll=%d num=%d caps=%d\n",
                            ps2_leds.scroll_lock, ps2_leds.num_lock, ps2_leds.caps_lock);
                    ps2_send_response(PS2_ACK);
                    break;

                case PS2_CMD_SET_TYPEMATIC:
                    ps2_apply_typematic(cmd);
                    ps2_send_response(PS2_ACK);
                    break;

                case PS2_CMD_SET_SCANCODE_SET:
                    if (cmd == 0) {
                        // Query current set
                        ps2_send_response(PS2_ACK);
                        wait_ms(PS2_INTER_BYTE_DELAY);
                        ps2_send_response(0x02);
                    } else if (cmd == 2) {
                        ps2_send_response(PS2_ACK);
                    } else {
                        ps2_send_response(PS2_RESEND);  // only Set 2 is implemented
                    }
                    break;
            }
            return;
        }
    }

    switch (cmd) {
        // Set LEDs: ACK now, the LED byte follows
        case PS2_CMD_SET_LEDS:
        case PS2_CMD_SET_SCANCODE_SET:
        case PS2_CMD_SET_TYPEMATIC:
            ps2_send_response(PS2_ACK);
            ps2_pending_cmd = cmd;
            break;

        // Echo back
        case PS2_CMD_ECHO:
            ps2_send_response(PS2_ECHO_RESPONSE);
            break;

        // Respond with keyboard ID (AB 83)
        case PS2_CMD_IDENTIFY:
            ps2_send_response(PS2_ACK);
            wait_ms(PS2_INTER_BYTE_DELAY);
            ps2_send_response(0xAB);
            wait_ms(PS2_INTER_BYTE_DELAY);
            ps2_send_response(0x83);
            break;

        // Enable/Disable commands
        case PS2_CMD_ENABLE:
            ps2_enabled = true;
            ps2_send_response(PS2_ACK);
            break;

        // Disable scanning (also restores defaults, per the PS/2 spec)
        case PS2_CMD_DISABLE:
            ps2_enabled = false;
            ps2_reset_defaults();
            ps2_send_response(PS2_ACK);
            break;

        // Set Defaults command
        case PS2_CMD_SET_DEFAULTS:
            ps2_reset_defaults();
            ps2_send_response(PS2_ACK);
            break;

        // Reset command: defaults, LEDs off, drop queued scancodes, then ACK + BAT
        case PS2_CMD_RESET:
            ps2_enabled = true;
            ps2_reset_defaults();
            ps2_leds.scroll_lock = 0;
            ps2_leds.num_lock    = 0;
            ps2_leds.caps_lock   = 0;
            send_buffer_head = send_buffer_tail = 0;
            ps2_send_response(PS2_ACK);
            wait_ms(PS2_INTER_BYTE_DELAY);
            ps2_send_response(PS2_BAT_SUCCESS);
            break;

        default:
            ps2_send_response(PS2_RESEND);
            break;
    }
}

void ps2_keyboard_init(uint8_t clk_pin, uint8_t data_pin) {
    ps2_clk_pin = clk_pin;
    ps2_data_pin = data_pin;

    // Set pins as inputs with pullups
    setPinInputHigh(ps2_clk_pin);
    setPinInputHigh(ps2_data_pin);

    ps2_enabled = true;
    ps2_state = PS2_STATE_IDLE;
    ps2_pending_cmd = 0;

    // Initialize LED state
    ps2_leds.caps_lock = 0;
    ps2_leds.num_lock = 0;
    ps2_leds.scroll_lock = 0;

    uprintf("[PS2] Device initialized on CLK=%d, DATA=%d\n", clk_pin, data_pin);
}

bool ps2_keyboard_send_printscreen_make(void) {
    // PrintScreen make: E0 12 E0 7C
    ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
    ps2_keyboard_send_raw_byte(0x12);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
    ps2_keyboard_send_raw_byte(PS2_PSCREEN);
    return true;
}

bool ps2_keyboard_send_printscreen_break(void) {
    // PrintScreen break: E0 F0 7C E0 F0 12
    ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_F0);
    ps2_keyboard_send_raw_byte(PS2_PSCREEN);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_F0);
    ps2_keyboard_send_raw_byte(0x12);
    return true;
}

bool ps2_keyboard_send_pause(void) {
    // Pause make: E1 14 77 E1 F0 14 F0 77
    // Pause has NO break code - only sends on make!
    ps2_keyboard_send_raw_byte(PS2_PREFIX_E1);
    ps2_keyboard_send_raw_byte(0x14);
    ps2_keyboard_send_raw_byte(PS2_PAUSE);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_E1);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_F0);
    ps2_keyboard_send_raw_byte(0x14);
    ps2_keyboard_send_raw_byte(PS2_PREFIX_F0);
    ps2_keyboard_send_raw_byte(PS2_PAUSE);
    return true;
}

static bool ps2_buffer_has_space(uint8_t needed) {
    uint8_t used;
    if (send_buffer_head >= send_buffer_tail) {
        used = send_buffer_head - send_buffer_tail;
    } else {
        used = PS2_SEND_BUFFER_SIZE - (send_buffer_tail - send_buffer_head);
    }
    return (PS2_SEND_BUFFER_SIZE - used) >= needed;
}

void ps2_keyboard_task(void) {
    // 1. Host request-to-send has priority over our own traffic
    if (ps2_host_request_pending()) {
        uint8_t cmd = 0;
        int8_t res = ps2_receive_byte(&cmd);
        if (res == PS2_RX_OK) {
            uprintf("[PS2] Host byte: 0x%02X\n", cmd);
            ps2_handle_command(cmd);
        } else if (res == PS2_RX_PARITY) {
            uprintf("[PS2] Host byte 0x%02X: bad parity\n", cmd);
            ps2_send_response(PS2_RESEND);
        } else {
            uprintf("[PS2] Host transmission aborted\n");
        }
        return;
    }

    // 2. Normal scancode output (skipped automatically while host inhibits)
    if (send_buffer_head != send_buffer_tail) {
        uint8_t byte = send_buffer[send_buffer_tail];
        if (ps2_send_byte(byte)) {
            send_buffer_tail = (send_buffer_tail + 1) % PS2_SEND_BUFFER_SIZE;
        }
    }

    ps2_keyboard_typematic_task();
}

bool ps2_keyboard_send_raw_byte(uint8_t byte) {
    uint8_t next_head = (send_buffer_head + 1) % PS2_SEND_BUFFER_SIZE;
    if (next_head == send_buffer_tail) {
        uprintf("[PS2] WARNING: Send buffer full! Dropping byte 0x%02X\n", byte);
        return false;
    }

    send_buffer[send_buffer_head] = byte;
    send_buffer_head = next_head;

    return true;
}

bool ps2_keyboard_send_key_make(uint8_t scancode) {
    if (!ps2_enabled) return false;

    uint8_t next_head = (send_buffer_head + 1) % PS2_SEND_BUFFER_SIZE;
    if (next_head == send_buffer_tail) {
        // Buffer full - this shouldn't happen in normal use!
        uprintf("[PS2] WARNING: Send buffer full! Dropping scancode 0x%02X\n", scancode);
        return false; // Buffer full
    }

    send_buffer[send_buffer_head] = scancode;
    send_buffer_head = next_head;

    return true;
}

bool ps2_keyboard_send_key_break(uint8_t scancode) {
    if (!ps2_enabled) return false;

    // Send break prefix (0xF0) then scancode
    uint8_t next_head = (send_buffer_head + 1) % PS2_SEND_BUFFER_SIZE;
    if (next_head == send_buffer_tail) return false;

    send_buffer[send_buffer_head] = PS2_PREFIX_F0;
    send_buffer_head = next_head;

    next_head = (send_buffer_head + 1) % PS2_SEND_BUFFER_SIZE;
    if (next_head == send_buffer_tail) return false;

    send_buffer[send_buffer_head] = scancode;
    send_buffer_head = next_head;

    return true;
}

ps2_led_state_t ps2_keyboard_get_leds(void) {
    return ps2_leds;
}

bool ps2_keyboard_is_enabled(void) {
    return ps2_enabled;
}

static uint8_t ps2_keyboard_leds(void) {
    ps2_led_state_t leds = ps2_keyboard_get_leds();
    return (leds.caps_lock << 1) | (leds.num_lock) | (leds.scroll_lock << 2);
}

static void ps2_send_keyboard(report_keyboard_t *report) {
    if (report->keys[0] != 0 || report->keys[1] != 0) {
        uprintf("[PS2] Report contains keys: ");
        for (int i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
            if (report->keys[i] != 0) {
                uprintf("0x%02X ", report->keys[i]);
            }
        }
        uprintf("\n");
    }

    // Handle modifier changes
    uint8_t mod_changes = previous_report.mods ^ report->mods;

    if (mod_changes) {
        for (uint8_t i = 0; i < 8; i++) {
            const ps2_modifier_mapping_t *mapping = &modifier_mappings[i];

            if (mod_changes & mapping->mod_bit) {
                bool is_pressed = report->mods & mapping->mod_bit;

                if (is_pressed) {
                    uprintf("[PS2] Modifier pressed: 0x%02X (scancode: 0x%02X%s)\n",
                            mapping->mod_bit, mapping->scancode,
                            mapping->needs_e0 ? ", E0 prefix" : "");
                    if (mapping->needs_e0) {
                        ps2_keyboard_send_key_make(PS2_PREFIX_E0);
                    }
                    ps2_keyboard_send_key_make(mapping->scancode);
                // ... inside ps2_send_keyboard ...
                } else {
                    uprintf("[PS2] Modifier released: 0x%02X (scancode: 0x%02X%s)\n",
                            mapping->mod_bit, mapping->scancode,
                            mapping->needs_e0 ? ", E0 prefix" : "");

                    // Corrected Release Sequence for Extended Modifiers
                    if (mapping->needs_e0) {
                        ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
                    }
                    ps2_keyboard_send_key_break(mapping->scancode);
                }
            }
        }
    }

    // Handle regular key releases
    for (int i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
        uint8_t prev_keycode = previous_report.keys[i];
        bool still_pressed = false;

        if (prev_keycode == 0) continue;

        for (int j = 0; j < KEYBOARD_REPORT_KEYS; j++) {
            if (report->keys[j] == prev_keycode) {
                still_pressed = true;
                break;
            }
        }

        if (!still_pressed) {
            ps2_mapping_t mapping = qmk_to_ps2_scancode(prev_keycode);
            if (mapping.scancode != 0) {
                if (mapping.special_type == PS2_KEY_PRINTSCREEN) {
                    ps2_keyboard_send_printscreen_break();
                    continue;
                }

                if (mapping.special_type == PS2_KEY_PAUSE) {
                    continue;
                }

                uprintf("[PS2] Key released: keycode=0x%04X, scancode=0x%02X%s\n",
                        prev_keycode, mapping.scancode,
                        mapping.needs_e0_prefix ? ", E0 prefix" : "");
                if (mapping.needs_e0_prefix) {
                    ps2_keyboard_send_key_make(PS2_PREFIX_E0);
                }
                ps2_keyboard_send_key_break(mapping.scancode);
                ps2_keyboard_typematic_stop(prev_keycode);
            }
        }
    }

    // Handle regular key presses
    for (int i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
        uint8_t keycode = report->keys[i];
        bool was_pressed = false;

        if (keycode == 0) continue;

        for (int j = 0; j < KEYBOARD_REPORT_KEYS; j++) {
            if (previous_report.keys[j] == keycode) {
                was_pressed = true;
                break;
            }
        }

        if (!was_pressed) {
            ps2_mapping_t mapping = qmk_to_ps2_scancode(keycode);
            if (mapping.scancode != 0) {
                if (mapping.special_type == PS2_KEY_PRINTSCREEN) {
                    ps2_keyboard_send_printscreen_make();
                    continue;
                }

                if (mapping.special_type == PS2_KEY_PAUSE) {
                    ps2_keyboard_send_pause();
                    continue;
                }

                uprintf("[PS2] Key pressed: keycode=0x%04X, scancode=0x%02X%s\n",
                        keycode, mapping.scancode,
                        mapping.needs_e0_prefix ? ", E0 prefix" : "");
                if (mapping.needs_e0_prefix) {
                    ps2_keyboard_send_key_make(PS2_PREFIX_E0);
                }
                ps2_keyboard_send_key_make(mapping.scancode);
                ps2_keyboard_typematic_arm(keycode, mapping.scancode);
            }
        }
    }

    previous_report = *report;
}

static void ps2_send_nkro(report_nkro_t *report) {
    // Not implemented
}

static void ps2_send_mouse(report_mouse_t *report) {
    // Not implemented
}

// Handle media/consumer keys - FIXED VERSION
static void ps2_send_extra(report_extra_t *report) {
    uint16_t current_media_key = 0;

    if (report->report_id == REPORT_ID_CONSUMER) {
        current_media_key = report->usage;
        uprintf("[PS2] Extra key report: usage=0x%04X\n", current_media_key);
    }

    if (current_media_key != previous_media_key) {
        // 1. Handle Release (Break)
        if (previous_media_key != 0) {
            // USE CONSUMER MAPPING for consumer control codes
            ps2_mapping_t mapping = consumer_to_ps2_scancode(previous_media_key);
            // Ensure space for: E0 (prefix) + F0 (break) + Scancode = 3 bytes
            if (mapping.scancode != 0 && ps2_buffer_has_space(3)) {
                uprintf("[PS2] Media key RELEASE: usage=0x%04X, scancode=0x%02X%s\n",
                        previous_media_key, mapping.scancode,
                        mapping.needs_e0_prefix ? ", E0 prefix" : "");
                if (mapping.needs_e0_prefix) {
                    ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
                }
                ps2_keyboard_send_key_break(mapping.scancode);
            } else if (mapping.scancode == 0) {
                uprintf("[PS2] WARNING: Previous consumer code 0x%04X has no PS/2 mapping!\n", previous_media_key);
            }
        }

        // 2. Handle Press (Make)
        if (current_media_key != 0) {
            // USE CONSUMER MAPPING for consumer control codes
            ps2_mapping_t mapping = consumer_to_ps2_scancode(current_media_key);
            // Ensure space for: E0 (prefix) + Scancode = 2 bytes
            if (mapping.scancode != 0 && ps2_buffer_has_space(2)) {
                uprintf("[PS2] Media key PRESS: usage=0x%04X, scancode=0x%02X%s\n",
                        current_media_key, mapping.scancode,
                        mapping.needs_e0_prefix ? ", E0 prefix" : "");
                if (mapping.needs_e0_prefix) {
                    ps2_keyboard_send_raw_byte(PS2_PREFIX_E0);
                }
                ps2_keyboard_send_key_make(mapping.scancode);

                // NOTE: We do NOT call ps2_keyboard_typematic_arm() here
                // because media keys should not repeat in PS/2.
            } else if (mapping.scancode == 0) {
                uprintf("[PS2] WARNING: Current consumer code 0x%04X has no PS/2 mapping!\n", current_media_key);
            }
        }
        previous_media_key = current_media_key;
    }
}

// Create the driver struct
host_driver_t ps2_keyboard_host_driver = {
    .keyboard_leds = ps2_keyboard_leds,
    .send_keyboard = ps2_send_keyboard,
    .send_nkro = ps2_send_nkro,
    .send_mouse = ps2_send_mouse,
    .send_extra = ps2_send_extra,
};
