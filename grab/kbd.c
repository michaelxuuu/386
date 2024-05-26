#include "kbd.h"
#include <types.h>
#include <pio.h>
#include <printf.h>


// Intel 8042 ps/2 controller data port
#define DATA_PORT 0x60

// Keyboard ring buffer size
// Has to be a power of 2
#define BUFFER_SIZE 128

// Max single-byte scan code (F12)
#define MAX_SCANCODE 0x58

// More on Intel 8042 ps/2 controller, https://wiki.osdev.org/%228042%22_PS/2_Controller
// More on ps/2 keyboard, https://wiki.osdev.org/PS/2_keyboard

// The compiler treats the 32-bit bit field differently from an 32-bit integer
// and returns the struct using the stack instead of %eax, so we need to
// make adjustments to handle this behavior
typedef uint32_t key_event_t;

// Macro to convert between struct and int
#define TO_INT(x) (*(key_event_t *)&(x))
#define TO_STRUCT(x) (*(struct key_event *)&(x))

// DFA parser states
typedef enum {
    STATE_INIT,
    STATE_PAUSE,
    STATE_EXTENDED,
    STATE_PRINT_MAKE,
    STATE_PRINT_BREAK
} parser_state_t;

// Functions called at DFA parser states
// do_sequecne() takes care of three states: PRSTATE_PAUSE,
// PRSTATE_PRNT_MAKE, and PRSTATE_PRNT_BREAK
key_event_t handle_init_state(uint8_t scancode);
key_event_t handle_extended_state(uint8_t scancode);
key_event_t handle_sequence_state(uint8_t scancode, const uint8_t *sequence, uint16_t length, 
parser_state_t next_state, int is_break, int key_code);

// Maps a SINGLE-byte make code to the corresponding key:
// map[makecode] -> key
// This mapping is yeild by sorting all the single-byte scan codes
// in the scan code set 1 by their make codes from low to high.
// Note: scan codes are assigned consecutively, beginning with 0,
// making it possible to use an array to do the mapping
static const uint8_t key_map[] = {
        KEY_UNKNOWN, 0x1B,        '1',         '2',     '3',        '4',
        '5',         '6',         '7',         '8',     '9',        '0',
        '-',         '=',         '\b',        '\t',    'q',        'w',
        'e',         'r',         't',         'y',     'u',        'i',
        'o',         'p',         '[',         ']',     '\n',       KEY_L_CTRL,
        'a',         's',         'd',         'f',     'g',        'h',
        'j',         'k',         'l',         ';',     '\'',       '`',
        KEY_L_SHFT,  '\\',        'z',         'x',     'c',        'v',
        'b',         'n',         'm',         ',',     '.',        '/',
        KEY_R_SHFT,  '*',         KEY_L_ALT,   ' ',     KEY_CAPS,   KEY_F1,
        KEY_F2,      KEY_F3,      KEY_F4,      KEY_F5,  KEY_F6,     KEY_F7,
        KEY_F8,      KEY_F9,      KEY_F10,     KEY_NUM, KEY_SCROLL, '7',
        '8',         '9',         '-',         '4',     '5',        '6',
        '+',         '1',         '2',         '3',     '0',        '.',
        KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_F11, KEY_F12,
};

// Global keyboard state
typedef struct {
    struct keyboard_state state;
    uint8_t buffer[BUFFER_SIZE];
    uint8_t driver_index;
    uint8_t device_index;
    parser_state_t parser_state;
    uint8_t sequence_index;
} keyboard_t;

static keyboard_t kbd;

// Long extended scan codes, used for sequence matching
static const uint8_t pause_sequence[] = { 0x1d, 0x45, 0xe1, 0x9d, 0xc5 };
static const uint8_t print_make_sequence[] = { 0x2a, 0xe0, 0x37 };
static const uint8_t print_break_sequence[] = { 0xb7, 0xe0, 0xaa };

// Check if the scan code is a make code
uint8_t is_make_code(uint8_t scancode)
{
    return !(scancode & 0x80);
}

// Convert break code to make code
uint8_t to_make_code(uint8_t breakcode)
{
    return breakcode & 0x7F;
}

// Create a "NULL" event, returned by the parser when the result are:
// 1. A key break
// 2. A state-related key
// 3. A non-result - in the middle of parsing an extended scan code
key_event_t null_event()
{
    return TO_INT((struct key_event){ 0 });
}

// Create an data event, returned by the parser when it has successfully
// completed parsing a scan code, this scan code satisfies none of three the "NULL"
// properties listed above, no errors (unrecognized scan codes) have occurred during parsing
key_event_t data_event(int data, int is_keypad)
{
    struct key_event e = { .hasdata = 1, .data = data, .keypad = is_keypad, .kbdstate = kbd.state };
    return TO_INT(e);
}

// Aka Error event
key_event_t raw_event(int raw)
{
    struct key_event e = { .hasraw = 1, .raw = raw, .kbdstate = kbd.state };
    return TO_INT(e);
}

// Check if the scan code corresponds to a keypad key
int is_keypad_key(uint8_t scancode)
{
    return (0x47 <= scancode && scancode <= 0x53) || scancode == 0x37;
}

static int isprint(int c)
{
	return ((c) >= ' ') && ((c) <= 126);
}

// Used by the parser to alter the intrnal keyboard states
// when it has identified a state-related key
void update_state(int key, int is_make)
{
    switch (key) {
        case KEY_L_CTRL: kbd.state.lctrl = is_make; break;
        case KEY_R_CTRL: kbd.state.rctrl = is_make; break;
        case KEY_L_SHFT: kbd.state.lshft = is_make; break;
        case KEY_R_SHFT: kbd.state.rshft = is_make; break;
        case KEY_L_ALT:  kbd.state.lalt = is_make; break;
        case KEY_R_ALT:  kbd.state.ralt = is_make; break;
        case KEY_NUM:    kbd.state.numlock ^= is_make; break;
        case KEY_CAPS:   kbd.state.capslock ^= is_make; break;
    }
}

// The main scan code parsing DFA:
// Each call to this function consumes 1 scan byte instead of 1 complete scan code,
// starting from the current driver index.
// Returns a non-NULL key_event if the scanbyte can be interrpreted as an data character;
// and a NULL event is reported back to the user routine in all other case, including:
// 1. It is a state-related key (such as ctrl and caplock), which only change the keyboard internal states
// 2. It is a break key (key release), which in most cases are ignored, and which, in some cases, also
// requires chhanging some internal states (such as ctrl and shift) of the keyboard, so that they won't
// be included in the later data events (events with data data) reported to the user
// 3. It is a header byte, marking the start of an extended scan code
key_event_t parse_scancode(uint8_t scancode)
{
    switch (kbd.parser_state) {
        case STATE_INIT:
            return handle_init_state(scancode);
        case STATE_PAUSE:
            return handle_sequence_state(scancode, pause_sequence, sizeof(pause_sequence), STATE_PAUSE, 0, KEY_PAUSE);
        case STATE_EXTENDED:
            return handle_extended_state(scancode);
        case STATE_PRINT_MAKE:
            return handle_sequence_state(scancode, print_make_sequence, sizeof(print_make_sequence), STATE_PRINT_MAKE, 0, KEY_PRNT);
        case STATE_PRINT_BREAK:
            return handle_sequence_state(scancode, print_break_sequence, sizeof(print_break_sequence), STATE_PRINT_BREAK, 1, KEY_PRNT);
        default:
            return null_event();
    }
}

key_event_t handle_init_state(uint8_t scancode)
{
    kbd.sequence_index = 0;

    if (scancode == 0xe1) {
        kbd.parser_state = STATE_PAUSE;
        return null_event();
    }

    if (scancode == 0xe0) {
        kbd.parser_state = STATE_EXTENDED;
        return null_event();
    }

    kbd.parser_state = STATE_INIT;

    uint8_t make_code = to_make_code(scancode);
    if (make_code > MAX_SCANCODE)
        return raw_event(scancode);

    uint8_t key = key_map[make_code];

    if (key > STATE_RELATED_KEY_START || !is_make_code(scancode)) {
        update_state(key, is_make_code(scancode));
        return null_event();
    }

    return data_event(key, is_keypad_key(make_code));
}

key_event_t handle_extended_state(uint8_t scancode)
{
    kbd.parser_state = STATE_INIT;

    switch (scancode) {
        case 0x1c: return data_event('/', 1);
        case 0x35: return data_event('\n', 1);
        case 0x48: return data_event(KEY_U_ARROW, 0);
        case 0x4b: return data_event(KEY_L_ARROW, 0);
        case 0x4d: return data_event(KEY_R_ARROW, 0);
        case 0x50: return data_event(KEY_D_ARROW, 0);
        case 0x53: return data_event(KEY_DELETE, 0);
        case 0x1d: update_state(KEY_R_CTRL, 1); break;
        case 0x9d: update_state(KEY_R_CTRL, 0); break;
        case 0x38: update_state(KEY_R_ALT, 1); break;
        case 0xb8: update_state(KEY_R_ALT, 0); break;
        case 0x2a: kbd.parser_state = STATE_PRINT_MAKE; break;
        case 0xb7: kbd.parser_state = STATE_PRINT_BREAK; break;
        default: return raw_event(scancode);
    }

    return null_event();
}

key_event_t handle_sequence_state(uint8_t scancode, const uint8_t *sequence, uint16_t length, parser_state_t next_state, int is_break, int key_code)
{
    if (scancode != sequence[kbd.sequence_index]) {
        kbd.parser_state = STATE_INIT;
        return raw_event(scancode);
    }

    if (kbd.sequence_index == length - 1) {
        kbd.parser_state = STATE_INIT;
        return is_break ? null_event() : data_event(key_code, 0);
    }

    kbd.parser_state = next_state;
    kbd.sequence_index++;
    return null_event();
}

// Shift key handling
char apply_shift(char c, key_event_t event)
{
    if (!TO_STRUCT(event).kbdstate.lshft && !TO_STRUCT(event).kbdstate.rshft)
        return c;

    switch (c) {
        case '`': return '~';
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '[': return '{';
        case '\\': return '|';
        case ']': return '}';
        case '-': return '_';
        case '=': return '+';
        case ';': return ':';
        case '\'': return '\"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return c;
    }
}

// Read a character from the keyboard buffer
int readchar(void)
{
    while (kbd.driver_index != kbd.device_index) {
        key_event_t event = parse_scancode(kbd.buffer[kbd.driver_index]);
        kbd.driver_index = (kbd.driver_index + 1) % BUFFER_SIZE;

        if (TO_STRUCT(event).hasdata) {
            uint8_t data = TO_STRUCT(event).data;
            if (isprint(data) || data == '\n' || data == '\b') {
                return apply_shift(data, event);
            }
        }
    }

    return -1;
}

int readline(char *buf, int len)
{
    if (len <= 0) return -1;

    int index = 0;
    while (index < len) {
        int c = readchar();
        if (c == -1) continue;
        if (c == '\b') {
            if (index > 0) --index;
            continue;
        }
        buf[index++] = c;
        if (c == '\n') break;
    }

    return index;
}

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64

int keyboard_has_data() {
    return inb(PS2_STATUS_PORT) & 1;
}

uint8_t keyboard_read() {
    while (!keyboard_has_data());
    return inb(PS2_DATA_PORT);
}

int kbd_test_polling(){
    uint8_t scancode;
    while (1) {
        scancode = keyboard_read();
        key_event_t event = parse_scancode(scancode);
        kbd.driver_index = (kbd.driver_index + 1) % BUFFER_SIZE;

        if (TO_STRUCT(event).hasdata) {
            uint8_t data = TO_STRUCT(event).data;
            if (isprint(data) || data == '\n' || data == '\b') {
                return apply_shift(data, event);
            }
        }
    }
}

// // This imeplementation overwirtes the old data if the driver is not keeping up
// void keyboard_isr(struct icontext *c)
// {
//     uint8_t scan_byte = inb(DATA_PORT);
//     kbd.buffer[kbd.device_index] = scan_byte;
//     kbd.device_index = (kbd.device_index + 1) % BUFFER_SIZE;
// }
