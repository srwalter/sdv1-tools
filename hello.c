#include <C8051F000.h>

#define OSC (20 * 1000 * 1000)

char read_byte() {
    while (!RI) {}
    RI = 0;
    return SBUF;
}

void write_byte(char x) {
    TI = 0;
    SBUF = x;
    while (!TI);
}

void setup_uart() {
    SM1 = 1;
    REN = 1;
    // Set baud to ~33.6k (34722)
    TH1 = 253;
    // Timer 1 to mode 2
    TMOD = 2 << 4;
    // Necessary?
    TR1 = 1;
    // Set SMOD to 1 for doubled baud rate
    PCON |= 1 << 7;
}

void main(void)
{
    char x;

    P1_0 = 0;

    setup_uart();
    x = read_byte();
    write_byte(x);
    write_byte('!');

    P1_1 = 0;

    for (;;);
}
