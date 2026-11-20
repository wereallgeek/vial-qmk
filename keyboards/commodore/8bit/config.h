// keyboards/your_keyboard/config.h
#pragma once

#define NO_USB_STARTUP_CHECK  true  // Don't wait for USB connection

//Status LEDs (if needed)
#define LED_CAPS_LOCK_PIN   GP11
#define LED_NUM_LOCK_PIN    GP12
#define LED_SCROLL_LOCK_PIN GP10
#define LED_PIN_ON_STATE    1

// PS/2 Keyboard Pin definitions
#define PS2_KEYBOARD_CLOCK_PIN  GP16
#define PS2_KEYBOARD_DATA_PIN   GP17

// Mode switch pin (to toggle between USB and PS/2)
#define MODE_SWITCH_PIN GP14  // High = USB, Low = PS/2

// Debounce reduces chatter (can also be set in info.json)
#define DEBOUNCE 5

// Note: USB IDs, matrix configuration, and processor info
// are now defined in info.json instead of here

