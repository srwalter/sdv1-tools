#include <C8051F000.h>

#define OSC_MHZ (20)

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

__sfr __at (0xC4) SFRAL;
__sfr __at (0xC5) SFRAH;
__sfr __at (0xC6) SFRFD;
__sfr __at (0xC7) SFRCN;
__sfr __at (0xBF) CHPCON;
__sfr __at (0xF6) CHPENR;

char read_flash(short addr)
{
    SFRAL = addr & 0xff;
    SFRAH = addr >> 8;
    SFRCN = 0;
    sleep_50us();
    return SFRFD;
}

void program_flash(short addr, char val)
{
    SFRAL = addr & 0xff;
    SFRAH = addr >> 8;
    SFRFD = val;
    SFRCN = 0x21;
    sleep_50us();
}

void erase_flash(void)
{
    SFRCN = 0x22;
    sleep_15ms();
}

void enable_icsp() {
    CHPENR = 0x87;
    CHPENR = 0x59;
    CHPCON = 0x03;
    CHPENR = 0x00;
}

void main(void)
{
    char x;
    short addr = 0;

    P1_0 = 0;

    setup_uart();
    write_byte('1');
    write_byte('2');

    P1_1 = 0;
    
    // Needs a transition to idle to take effect
    enable_icsp();

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
        x = read_byte();
        switch (x) {
            // Read byte from APROM
            case 1:
                x = read_flash(addr);
                write_byte(x);
                addr++;
                break;

            // Set addr
            case 2:
                x = read_byte();
                addr = x;
                x = read_byte();
                addr |= x << 8;
                write_byte('K');
                break;

            // Erase APROM
            case 3:
                erase_flash();
                write_byte('K');
                break;

            // Program APROM
            case 4:
                x = read_byte();
                program_flash(addr, x);
                addr++;
                write_byte('K');
                break;

            // Exit
            case 5:
                CHPENR = 0x87;
                CHPENR = 0x59;
                CHPCON = 0x83;
                write_byte('F');
                break;

            default:
                write_byte('?');
                break;
        }
        P1_3 ^= 1;
    }
}
