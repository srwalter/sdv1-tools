#include <C8051F000.h>

#define OSC (20 * 1000 * 1000)

void timer0_isr (void) __interrupt (1)
{
}

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
    write_byte('1');
    x = read_byte();
    write_byte(x);
    write_byte('2');

    P1_1 = 0;
    
    // Setup Timer 0 to wake us up
    TL0 = 31;
    TH0 = 0xff;
    // Setup interrupts
    ET0 = 1;
    EA = 1;
    // And go
    TR0 = 1;

    // Go to sleep
    PCON |= PCON_IDLE;

    // Yay we made it back!
    P1_2 = 0;
    write_byte('3');

    for (;;) {
        PCON |= PCON_IDLE;
    }
}
