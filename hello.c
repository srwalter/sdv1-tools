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
    // Set baud to 9600 (9469)
    TH1 = 245;
    // Timer 1 to mode 2
    TMOD |= 2 << 4;
    // Necessary?
    TR1 = 1;
    // Set SMOD to 1 for doubled baud rate
    PCON |= 1 << 7;
}

// XXX: actually seems to be more like 40ms
void sleep_50us(void)
{
    // XXX: assumes 20MHz
    TMR0 = 20 * 50;
    TR0 = 1;
    PCON |= PCON_IDLE;
}

__sfr __at (0xC4) SFRAL;
__sfr __at (0xC5) SFRAH;
__sfr __at (0xC6) SFRFD;
__sfr __at (0xC7) SFRCN;

char read_flash(short addr)
{
    SFRAL = addr & 0xff;
    SFRAH = addr >> 8;
    SFRCN = 0;
    sleep_50us();
    return SFRFD;
}

void main(void)
{
    char x;
    short addr = 0;

    P1_0 = 0;

    setup_uart();
    write_byte('1');
    x = read_byte();
    write_byte(x);
    write_byte('2');

    P1_1 = 0;
    
    // Setup Timer 0 to wake us up (mode 1)
    TMOD |= 1;
    TMR0 = 20 * 1000;
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
        read_byte();
        x = read_flash(addr);
        write_byte(x);
        addr++;
        P1_3 ^= 1;
    }
}
