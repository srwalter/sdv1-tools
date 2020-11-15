#include <C8051F000.h>

#define OSC_MHZ (12)
#define SDA (P1_1)
#define SCL (P1_0)
#define RED_LED (P0_0)
#define GREEN_LED (P0_1)

// For a 40kHz i2c bus, that's about 25ms per cycle.  15ms is close enough for a half cycle
// inline because this function is shorter than 4 push + 4 pop
void inline sleep_15ms(void)
{
    TMR0 = 65535 - (15000 / 12 * OSC_MHZ);
    TR0 = 1;
    PCON |= PCON_IDLE;
}

// XXX: assumes 12MHz clock
void usleep(char x)
{
    while (x--);
}

// Pre-condition: SCL high
void inline i2c_start()
{
    SDA = 0;
    usleep(1000);
    SCL = 0;
}

void inline i2c_stop()
{
    SCL = 0;
    usleep(1000);
    SDA = 0;
    sleep_15ms();

    SCL = 1;
    // Wait for SCL to rise in case of clock stretching
    while(!SCL);

    usleep(1000);
    SDA = 1;
    sleep_15ms();
}

void send_bit(char val)
{
    SCL = 0;
    usleep(1000);
    if (val) {
        SDA = 1;
        while(!SDA);
    } else {
        SDA = 0;
        while(SDA);
    }

    sleep_15ms();
    SCL = 1;
    sleep_15ms();
    // Wait for SCL to rise in case of clock stretching
    while(!SCL);
    SCL = 0;
}

char recv_bit()
{
    int val;

    SCL = 0;
    // Let SDA float so the slave can drive ACK/NAK
    SDA = 1;
    sleep_15ms();
    SCL = 1;
    // Wait for SCL to rise in case of clock stretching
    while(!SCL);
    if (SDA)
        val = 1;
    else
        val = 0;
    sleep_15ms();
    return val;
}

int i2c_send_byte(char val)
{
    int i;

    for (i=0; i<8; i++) {
        if (val & 0x80)
            send_bit(1);
        else
            send_bit(0);
        val <<= 1;
    }

    return recv_bit();
}

int i2c_recv_byte()
{
    int i;
    int val = 0;

    for (i=0; i<8; i++) {
        if (recv_bit())
            val |= 1;
        val <<= 1;
    }

    // Send NAK because we only want one byte
    SCL = 0;
    sleep_15ms();
    send_bit(1);
    i2c_stop();

    return val;
}

char i2c_send(char addr, char subaddr, char val)
{
    int nak;

    i2c_start();
    nak = i2c_send_byte(addr << 1);
    if (nak) {
        i2c_stop();
        return 1;
    }
    nak = i2c_send_byte(subaddr);
    if (nak) {
        i2c_stop();
        return 1;
    }
    nak = i2c_send_byte(val);
    i2c_stop();
    return nak;
}

char i2c_recv(char addr, char subaddr)
{
    int nak;

    i2c_start();
    nak = i2c_send_byte(addr << 1);
    if (nak) {
        i2c_stop();
        return 0xff;
    }
    nak = i2c_send_byte(subaddr);
    if (nak) {
        i2c_stop();
        return 0xff;
    }

    i2c_start();
    nak = i2c_send_byte(addr << 1 | 1);
    if (nak) {
        i2c_stop();
        return 0xff;
    }

    return i2c_recv_byte();
}

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

void main(void)
{
    P1_0 = 0;
    char addr;
    char subaddr;
    char val;

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

    SCL = 1;
    SDA = 1;
    sleep_15ms();

    //i2c_send(0x42 >> 1, 0x3d, 0xc3);
    //val = i2c_recv(0x42 >> 1, 0x3d);
    //write_byte(val);

    P0_1 = 1;
    for (;;) {
        P0_0 ^= 1;
        SCL = 1;
        SDA = 1;
        val = read_byte();
        switch (val) {
            case 'r':
                addr = read_byte();
                subaddr = read_byte();
                val = i2c_recv(addr, subaddr);
                write_byte(val);
                break;

            case 'w':
                addr = read_byte();
                subaddr = read_byte();
                val = read_byte();
                val = i2c_send(addr, subaddr, val);
                write_byte(val);
                break;

            default:
                write_byte('?');
        }
    }
}
