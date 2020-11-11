#include <C8051F000.h>

#define OSC_MHZ (12)

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
    // Set baud to 19200
    TH1 = 0xfd;
    // Timer 1 to mode 2
    TMOD |= 2 << 4;
    // Necessary?
    TR1 = 1;
    // Set SMOD to 1 for doubled baud rate
    PCON |= 1 << 7;
}

void sleep_max(void)
{
    TMR0 = 65535;
    TR0 = 1;
    PCON |= PCON_IDLE;
}

void sleep_50us(void)
{
    TMR0 = 65535 - (OSC_MHZ * 50 / 12);
    TR0 = 1;
    PCON |= PCON_IDLE;
}

void sleep_15ms(void)
{
    TMR0 = 65535 - (15000 / 12 * OSC_MHZ);
    TR0 = 1;
    PCON |= PCON_IDLE;
}

void main(void)
{
    P1_0 = 0;

    setup_uart();
    write_byte('1');
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

    P0_0 ^= 1;
    for (;;) {
        P0_0 ^= 1;
        P0_1 ^= 1;
        sleep_max();
    }
}
