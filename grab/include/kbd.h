#ifndef _kbd_h_
#define _kbd_h_

#include <types.h>

// Max ascii code
#define MAX_ASCII 127

// The enumeration of the keys related to the keyboard state
// (e.g. ctrl and shift and numlock) starts at integer 200
#define STATE_RELATED_KEY_START 200

// Internal states of the keyboard, including whether some special keys
// (e.g. ctrl and shift) are being held, whether some functionalities
// (e.g. caplock or numlock) have been activated, etc.
struct keyboard_state {
        uint8_t numlock : 1;
        uint8_t capslock : 1;
        uint8_t ralt : 1;
        uint8_t lalt : 1;
        uint8_t rctrl : 1;
        uint8_t lctrl : 1;
        uint8_t rshft : 1;
        uint8_t lshft : 1;
} __attribute__((packed));

// A key event struct returned to the user routines of the keyboard driver
// encompassing comprehensive information of a keyboard event
struct key_event {
        // Valid for read if the key has an ascii representation
        // regardless of whether it's printable or not, or if it is useful
        // to the user routine
        uint8_t data;
        // Valid for reading if the 'hasraw' field is set to 1,
        // meaning the ps/2 data is not interrpretable, even as a modifier key
        uint8_t raw;
        // Set to 1 if it's a make code (key press),
        // key release otherwise
        uint8_t make : 1;
        // Set to 1 if the event is from the keypad
        uint8_t keypad : 1;
        // Set to 1 if the scan code can be mapped to an data code
        // and make is set to 1 (data data is offered at key press not release)
        uint8_t hasdata : 1;
        // Set to 1 if the ps/2 data is not interrpretable, even as a modifier key
        uint8_t hasraw : 1;
        // Reserved for future use (some keys aren't imeplemented yet)
        uint8_t unused : 4;
        // A "snapshot" of the keyboard state at the time of the event
        // enabling the support for the modifier keys and key combinations,
        // and giving clear direction to the user routine - which
        // (left or right) modifier key it is, eliminating any ambiguity.
        struct keyboard_state kbdstate;
} __attribute__((packed));

// Here are the encodings (enumerations) of the keyboard keys.
// Only the nonprintable keys are encoded while the
// printable ones are already naturally and conveniently encoded by their 
// ascii values. For example, the key "A" is encoded by
// ascii 'a' (aka 97), and the same for the keys, "=", "\t" (TAB), 
// and "\b" (BACKSPACE), etc.
enum nonprintable_keys {
        KEY_UNKNOWN = MAX_ASCII + 1,
        KEY_F1,
        KEY_F2,
        KEY_F3,
        KEY_F4,
        KEY_F5,
        KEY_F6,
        KEY_F7,
        KEY_F8,
        KEY_F9,
        KEY_F10,
        KEY_F11,
        KEY_F12,
        KEY_KP_EN,
        KEY_PRNT,
        KEY_HOME,
        KEY_U_ARROW,
        // KEY_PG_UP,
        KEY_L_ARROW,
        KEY_R_ARROW,
        // KEY_END,
        // KEY_R_GUI,
        KEY_D_ARROW,
        // KEY_PG_DN,
        // KEY_INSERT,
        KEY_DELETE,
        // KEY_L_GUI,
        // KEY_APPS,
        KEY_PAUSE,

        KEY_L_CTRL = STATE_RELATED_KEY_START,
        KEY_R_CTRL,
        KEY_L_SHFT,
        KEY_R_SHFT,
        KEY_L_ALT,
        KEY_R_ALT,
        KEY_NUM,
        KEY_SCROLL,
        KEY_CAPS,
};

// void *keyboard_isr(struct icontext * c);
int readchar(void);
int readline(char *buf, int len);
int kbd_test_polling();

#endif
