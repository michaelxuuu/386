/**
 * @file kbd.c
 * @author your name (you@domain.com)
 * @brief 
 * More on Intel 8042 ps/2 controller:
 * https://wiki.osdev.org/%228042%22_PS/2_Controller 
 * More on ps/2 keyboard:
 * https://wiki.osdev.org/PS/2_keyboard
 * @date 2024-05-22
 * 
 */

#include <kbd.h>
#include <pio.h>
#include <vga.h>
#include <types.h>
#include <printf.h>

/** Intel 8042 ps/2 controller data port */
#define DATAPORT 0x60
#define STUPORT 0x64
#define CMDPORT 0x64

typedef uint32_t key_event_int;

#define TO_INT(x) (*(key_event_int *)&(x))
#define TO_STRUCT(x) (*(struct key_event *)&(x))

// Keyboard ring buffer size
#define BSIZE 0x80

// Max single-byte scan code (F12)
#define MAX_SIMPLE_SCANCODE 0x58

// DFA parser states
enum parser_state {
        PRSTATE_INIT,
        PRSTATE_PAUSE,
        PRSTATE_NONPAUSE_EXT,
        PRSTATE_PRNT_MAKE,
        PRSTATE_PRNT_BREAK
};

// Functions called at DFA parser states
// do_sequecne() takes care of three states: PRSTATE_PAUSE,
// PRSTATE_PRNT_MAKE, and PRSTATE_PRNT_BREAK
key_event_int do_init(uint8_t scancode);
key_event_int do_nonpause_ext(uint8_t scancode);
key_event_int do_seqence(uint8_t scancode, uint8_t *seqence, int len,
                         int current_state, int isbreak, int data);

// Maps a SINGLE-byte make code to the corresponding key:
// map[makecode] -> key
// This mapping is yeild by sorting all the single-byte scan codes
// in the scan code set 1 by their make codes from low to high.
// Note: scan codes are assigned consecutively, beginning with 0,
// making it possible to use an array to do the mapping
static const uint8_t keymap[] = {
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

/** Global variable struct */
struct {
        // A global struct to keep track of the current state of the keyboard
        // and is updated by the processing of each scan code
        struct keyboard_state state;
        // Scan code buffer
        uint8_t buf[BSIZE];
        // Buffer reader index
        uint8_t buf_idx_driver;
        // Buffer writer index
        uint8_t buf_idx_device;
        // Global variables to keep track of the important states of the parser DFA,
        // especially during parsing an extended scan code
        // Scan code parser states
        enum parser_state prstate;
        // The index into the below-defined arrays when matching their corresponding scan codes
        uint8_t sequence_idx;
} kbd;

// Long extended scan codes, used for sequence matching
static uint8_t pause_sequence[] = { 0x1d, 0x45, 0xe1, 0x9d, 0xc5 };
static uint8_t prntmake_sequence[] = { 0x2a, 0xe0, 0x37 };
static uint8_t prntbreak_sequence[] = { 0xb7, 0xe0, 0xaa };

// Returns 1 if the given scan code is a make code, and 0 otherwise
uint8_t ismakecode(uint8_t scancode)
{
        return !(scancode & 0x80);
}

// Given a break code, returns the corresponding make code
// Note: A make code passed in will be returned as it is
static inline uint8_t tomakecode(uint8_t breakcode)
{
        return breakcode & 0x7F;
}

// Create a "NULL" event, returned by the parser when the result are:
// 1. A key break
// 2. A state-related key
// 3. A non-result - in the middle of parsing an extended scan code
static key_event_int nullevent()
{
        return TO_INT((struct key_event){ 0 });
}

// Create an data event, returned by the parser when it has successfully
// completed parsing a scan code, this scan code satisfies none of three the "NULL"
// properties listed above, no errors (unrecognized scan codes) have occurred during parsing
static key_event_int dataevent(int data, int keypad)
{
        struct key_event e = (struct key_event){ 0 };
        e.hasdata = 1;
        e.data = data;
        e.keypad = keypad;
        e.kbdstate = kbd.state;
        return TO_INT(e);
}

// Aka Error event
static key_event_int rawevent(int raw)
{
        struct key_event e = (struct key_event){ 0 };
        e.hasraw = 1;
        e.raw = raw;
        e.kbdstate = kbd.state;
        return TO_INT(e);
}

// Used by the parser to alter the intrnal keyboard states
// when it has identified a state-related key
static void updatestate(int key, int ismake)
{
        if (key == KEY_L_CTRL)
                kbd.state.lctrl = ismake;
        else if (key == KEY_R_CTRL)
                kbd.state.rctrl = ismake;
        else if (key == KEY_L_SHFT)
                kbd.state.lshft = ismake;
        else if (key == KEY_R_SHFT)
                kbd.state.rshft = ismake;
        else if (key == KEY_L_ALT)
                kbd.state.lalt = ismake;
        else if (key == KEY_R_ALT)
                kbd.state.ralt = ismake;
        else if (key == KEY_NUM)
                kbd.state.numlock ^= ismake;
        else if (key == KEY_CAPS)
                kbd.state.capslock ^= ismake;
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
static key_event_int parse_scancode(uint8_t scancode)
{
        switch (kbd.prstate) {
        case PRSTATE_INIT:
                return do_init(scancode);
        case PRSTATE_PAUSE:
                return do_seqence(scancode, pause_sequence,
                                  sizeof(pause_sequence), PRSTATE_PAUSE, 0,
                                  KEY_PAUSE);
        case PRSTATE_NONPAUSE_EXT:
                return do_nonpause_ext(scancode);
        case PRSTATE_PRNT_MAKE:
                return do_seqence(scancode, prntmake_sequence,
                                  sizeof(prntmake_sequence), PRSTATE_PRNT_MAKE,
                                  0, KEY_PRNT);
        case PRSTATE_PRNT_BREAK:
                return do_seqence(scancode, prntbreak_sequence,
                                  sizeof(prntbreak_sequence),
                                  PRSTATE_PRNT_BREAK, 1, KEY_PRNT);
        }
        return nullevent();
}

static inline int iskeypad(uint8_t scancode)
{
        return (0x47 <= scancode && scancode <= 0x53) || scancode == 0x37;
}

key_event_int do_init(uint8_t scancode)
{
        kbd.sequence_idx = 0;

        // Extended scan code (pause)
        if (scancode == 0xe1) {
                kbd.prstate = PRSTATE_PAUSE;
                return nullevent();
        }

        // Extended scan code (nonpause)
        if (scancode == 0xe0) {
                kbd.prstate = PRSTATE_NONPAUSE_EXT;
                return nullevent();
        }

        // Simpple scan code
        kbd.prstate = PRSTATE_INIT;

        // Invalid make/ break code
        if (tomakecode(scancode) > MAX_SIMPLE_SCANCODE)
                return rawevent(scancode);

        // Obtain the key code
        uint8_t key = keymap[tomakecode(scancode)];

        // Return a null event if it's a state-related key make
        // or a key break
        if (key > STATE_RELATED_KEY_START || !ismakecode(scancode)) {
                updatestate(key, ismakecode(scancode));
                return nullevent();
        }

        // Has to be a data event here
        return dataevent(key, iskeypad(scancode));
}

key_event_int do_nonpause_ext(uint8_t scancode)
{
        kbd.prstate = PRSTATE_INIT; // Next state of most of the below cases
        switch (scancode) {
        case 0x1c:
                return dataevent('/', 1);
        case 0x35:
                return dataevent('\n', 1);
        case 0x48:
                return dataevent(KEY_U_ARROW, 0);
        case 0x4b:
                return dataevent(KEY_L_ARROW, 0);
        case 0x4d:
                return dataevent(KEY_R_ARROW, 0);
        case 0x50:
                return dataevent(KEY_D_ARROW, 0);
        case 0x53:
                return dataevent(KEY_DELETE, 0);
        case 0x1d:
                updatestate(KEY_R_CTRL, 1); // Fall through
        case 0x9d:
                updatestate(KEY_R_CTRL, 0); // Fall through
        case 0x38:
                updatestate(KEY_R_ALT, 1); // Fall through
        case 0xb8:
                updatestate(KEY_R_ALT, 0); // Fall through
        case 0x2a:
                kbd.prstate = PRSTATE_PRNT_MAKE; // Fall through
        case 0xb7:
                kbd.prstate = PRSTATE_PRNT_BREAK; // Fall through
        // Key breaks with no actions
        case 0xb5:
        case 0xc8:
        case 0xcb:
        case 0xcd:
        case 0xd0:
        case 0xd3:
                return nullevent();
        default:
                return rawevent(scancode);
        }
}

key_event_int do_seqence(uint8_t scancode, uint8_t *seqence, int len,
                         int current_state, int isbreak, int data)
{
        kbd.prstate = PRSTATE_INIT;
        if (scancode != seqence[kbd.sequence_idx]) // Mismatched
                return rawevent(scancode);
        if (kbd.sequence_idx == len - 1) // Matched
                return isbreak ? nullevent() : dataevent(data, 0);
        kbd.prstate = current_state; // Continue matching the rest
        kbd.sequence_idx++;
        return nullevent();
}

char shiftkey(char c, key_event_int e)
{
        if (!TO_STRUCT(e).kbdstate.lshft && !TO_STRUCT(e).kbdstate.rshft)
                return c;

        switch (c) {
        case '`':
                return '~';
        case '1':
                return '!';
        case '2':
                return '@';
        case '3':
                return '#';
        case '4':
                return '$';
        case '5':
                return '%';
        case '6':
                return '^';
        case '7':
                return '&';
        case '8':
                return '*';
        case '9':
                return '(';
        case '0':
                return ')';
        case '[':
                return '{';
        case '\\':
                return '|';
        case ']':
                return '}';
        case '-':
                return '_';
        case '=':
                return '+';
        case ';':
                return ':';
        case '\'':
                return '\"';
        case ',':
                return '<';
        case '.':
                return '>';
        case '/':
                return '?';
        }

        return c;
}

int isprint(int c){
	return ((c) >= ' ') && ((c) <= 126);
}

int readchar(void)
{
    // Use the scan code parser to parse scan bytes in the buffer sequentially
    key_event_int e;

    while (1) {
        if (kbd.buf_idx_driver == kbd.buf_idx_device)
            return -1;
        e = parse_scancode(kbd.buf[kbd.buf_idx_driver]);
        if (TO_STRUCT(e).hasdata &&
            (isprint(TO_STRUCT(e).data) || TO_STRUCT(e).data == '\n' ||
                TO_STRUCT(e).data == '\b')) {
                kbd.buf_idx_driver = (kbd.buf_idx_driver + 1) & ~BSIZE;
                return shiftkey(TO_STRUCT(e).data, e);
        }
        kbd.buf_idx_driver = (kbd.buf_idx_driver + 1) & ~BSIZE;
    }
}

int readline(char *buf, int len)
{
    if (len < 0)
        return -1;

    int c;
    for (int i = 0; i < len;) {
        while ((c = readchar()) == -1){
            if (c == '\b') {
                i = i == 0 ? 0 : i - 1;
                continue;
            }
            buf[i++] = c;
            if (c == '\n')
                return i;
        }
    }
    return len;
}

// TODO:
/**
 * @brief keyboard isr
 * 
 */
// void *keyboard_isr(struct context * c){
//     (void) c;
//     return 0;
// }