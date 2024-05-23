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

#include <types.h>
#include <pio.h>
#include <vga.h>
#include <printf.h>

/** Intel 8042 ps/2 controller data port */
#define DATAPORT 0x60
#define STUPORT 0x64
#define CMDPORT 0x64

/** PS/2 Controller Commands */
#define PS2_READ_BYTE_0      0x20  // Read "byte 0" from internal RAM (Controller Configuration Byte)
#define PS2_DISABLE_SECOND_PORT  0xA7  // Disable second PS/2 port (only if 2 PS/2 ports supported)
#define PS2_ENABLE_SECOND_PORT   0xA8  // Enable second PS/2 port (only if 2 PS/2 ports supported)
#define PS2_TEST_SECOND_PORT     0xA9  // Test second PS/2 port (only if 2 PS/2 ports supported)
#define PS2_PORT_TEST_PASS      0x00  // Test passed
#define PS2_CLOCK_LINE_LOW      0x01  // Clock line stuck low
#define PS2_CLOCK_LINE_HIGH     0x02  // Clock line stuck high
#define PS2_DATA_LINE_LOW       0x03  // Data line stuck low
#define PS2_DATA_LINE_HIGH      0x04  // Data line stuck high
#define PS2_TEST_CONTROLLER     0xAA  // Test PS/2 Controller
#define PS2_CRTL_TEST_PASS      0x55  // Test passed
#define PS2_PORT_TEST_FAIL      0xFC  // Test failed
#define PS2_TEST_FIRST_PORT     0xAB  // Test first PS/2 port
#define PS2_DIAG_DUMP           0xAC  // Diagnostic dump (read all bytes of internal RAM)
#define PS2_DISABLE_FIRST_PORT  0xAD  // Disable first PS/2 port
#define PS2_ENABLE_FIRST_PORT   0xAE  // Enable first PS/2 port
#define PS2_READ_INPUT_PORT     0xC0  // Read controller input port
#define PS2_COPY_BITS_0_3       0xC1  // Copy bits 0 to 3 of input port to status bits 4 to 7
#define PS2_COPY_BITS_4_7       0xC2  // Copy bits 4 to 7 of input port to status bits 4 to 7
#define PS2_READ_OUTPUT_PORT    0xD0  // Read Controller Output Port
#define PS2_WRITE_OUTPUT_PORT   0xD1  // Write next byte to Controller Output Port
#define PS2_WRITE_FIRST_PORT_BUF 0xD2  // Write next byte to first PS/2 port output buffer (only if 2 PS/2 ports supported)
#define PS2_WRITE_SECOND_PORT_BUF 0xD3  // Write next byte to second PS/2 port output buffer (only if 2 PS/2 ports supported)
#define PS2_WRITE_SECOND_PORT_INPUT 0xD4  // Write next byte to second PS/2 port input buffer (only if 2 PS/2 ports supported)
#define PS2_PULSE_OUTPUT(mask)  (0xF0 + ((mask) & 0x0F))  // Pulse output line low for 6 ms. Bits 0 to 3 are used as a mask (0 = pulse line, 1 = don't pulse line)

/** 
 * PS/2 Controller Configuration Byte
 * 0	First PS/2 port interrupt (1 = enabled, 0 = disabled)
 * 1	Second PS/2 port interrupt (1 = enabled, 0 = disabled, only if 2 PS/2 ports supported)
 * 2	System Flag (1 = system passed POST, 0 = your OS shouldn't be running)
 * 3	Should be zero
 * 4	First PS/2 port clock (1 = disabled, 0 = enabled)
 * 5	Second PS/2 port clock (1 = disabled, 0 = enabled, only if 2 PS/2 ports supported)
 * 6	First PS/2 port translation (1 = enabled, 0 = disabled)
 * 7	Must be zero 
 */

/**
 * @brief Send an ps2 command and return
 * the response
 * 
 * @param cmd 
 * @return uint8_t 
 */
uint8_t ps2_ctl_command(uint8_t cmd){
    while (inb(STUPORT) & 0x02);
    outb(cmd, CMDPORT);
    while (!(inb(STUPORT) & 0x01));
    return inb(DATAPORT);
}

/**
 * @brief ps2 self test
 * 
 */
void ps2_test(){
    printf("PS/2 controller configuration: 0x%x\n", ps2_ctl_command(PS2_READ_BYTE_0));

    // Test PS/2 Controller
    uint8_t res = ps2_ctl_command(PS2_TEST_CONTROLLER);
    res == PS2_CRTL_TEST_PASS ? printf("PS/2 Controller Test Passed\n") : printf("PS/2 Controller Test Failed\n");

    // Test first PS/2 port
    res = ps2_ctl_command(PS2_TEST_FIRST_PORT);
    res == PS2_PORT_TEST_PASS ? printf("First PS/2 port Test Passed\n") : printf("First PS/2 port Test Failed\n");

    // Test second PS/2 port
    res = ps2_ctl_command(PS2_TEST_SECOND_PORT);
    res == PS2_PORT_TEST_PASS ? printf("Second PS/2 port Test Passed\n") : printf("Second PS/2 port Test Failed\n");
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