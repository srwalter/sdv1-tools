#include <C8051F000.h>

#define OSC_MHZ (12)
#define SDA (P1_1)
#define SCL (P1_0)
#define RED_LED (P0_0)
#define GREEN_LED (P0_1)

// XXX: assumes 12MHz clock
void inline usleep(short x)
{
    // Divide by 2, two instructions to decrement and compare
    x >>= 1;
    while (x--);
}

// For a 40kHz i2c bus, that's about 25uS per cycle.  12uS is close enough for a half cycle
// inline because this function is shorter than 4 push + 4 pop
void inline sleep_15ms(void)
{
    usleep(12);
}

void inline i2c_start()
{
    SCL = 1;
    sleep_15ms();
    SCL = 0;
    usleep(1);
    SDA = 1;
    sleep_15ms();
    SCL = 1;
    usleep(1);
    SDA = 0;
    usleep(1);
}

void inline i2c_stop()
{
    SCL = 0;
    usleep(1);
    SDA = 0;
    sleep_15ms();

    SCL = 1;
    // Wait for SCL to rise in case of clock stretching
    while(!SCL);

    usleep(1);
    SDA = 1;
    sleep_15ms();
}

void send_bit(char val)
{
    SCL = 0;
    usleep(1);
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
        if (i != 7)
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

static const char adv7181_setup[][2] = {
    {0x2F,0xB4},
    {0x3A,0x1E},
    {0x2F,0xB4},
    {0x08,0x80},
    {0x2C,0xAE},
    {0x2B,0xF2},
    {0xD2,0x01},
    {0xD3,0x01},
    {0xDB,0x9B},
    {0x2C,0xFE},
    {0x02,0x04},
    {0x01,0xC8},
    {0x03,0x8C},
    {0x14,0x12},
    {0x15,0x00},
    {0x17,0x41},
    {0x31,0x12},
    {0x50,0x04},
    {0x51,0x24},
    {0xC4,0x80},
    {0xC3,0x06},
    {0x3A,0x16},
    {0x04,0x57},
    {0x27,0x58},
    {0x0B,0x00},
    {0x3D,0xC3},
    {0x3F,0xE4},
    {0xB3,0xFE},
    {0x0F,0x40},
    {0x0E,0x85},
    {0x89,0x0D},
    {0x8D,0x9B},
    {0x8F,0x48},
    {0xB5,0x8B},
    {0xD4,0xFB},
    {0xD6,0x6D},
    {0xE2,0xAF},
    {0xE8,0xF3},
    {0x0E,0x05},
    {0x04,0xD7},
    {0x09,0x80},
    {0x41,0x10},
    {0xE3,0x80},
    {0xE4,0x80},
    {0xE6,0x11},
    {0x0C,0x00},
    {0x0D,0x88},
    {0x0E,0x85},
    {0x52,0x00},
    {0x86,0x02},
    {0x0E,0x05},
    {0x0B,0x00}
};

void setup_adv7181(void)
{
    int i;
    int nak;

    for (i=0; i<sizeof(adv7181_setup)/sizeof(adv7181_setup[0]); i++) {
        nak = i2c_send(0x21, adv7181_setup[i][0], adv7181_setup[i][1]);
        if (nak) {
            write_byte('@');
	    return;
	} else {
            write_byte('.');
        }
    }

    write_byte('!');
}

static const char adv7179_setup[][2] = {
    {0x00,0x10},
    {0x01,0x10},
    {0x01,0x10},
    {0x02,0x06},
    {0x03,0x08},
    {0x04,0x13},
    {0x07,0x00},
    {0x08,0x00},
    {0x09,0x1E},
    {0x0A,0x7C},
    {0x0B,0xF0},
    {0x0C,0x21},
    {0x0D,0x00},
    {0x0E,0x00},
    {0x0F,0x00},
    {0x10,0x00},
    {0x11,0x00},
    {0x12,0x00},
    {0x13,0x00},
    {0x14,0x00},
    {0x15,0x00},
    {0x16,0x00},
    {0x17,0x00},
    {0x18,0x00},
    {0x19,0x00},
    {0x07,0x00},
    {0x07,0x80},
    {0x07,0x00}
};

void setup_adv7179(void)
{
    int i;
    int nak;

    for (i=0; i<sizeof(adv7179_setup)/sizeof(adv7179_setup[0]); i++) {
        nak = i2c_send(0x2A, adv7179_setup[i][0], adv7179_setup[i][1]);
        if (nak) {
            write_byte('@');
	    return;
	} else {
            write_byte('.');
        }
    }

    write_byte('!');
}

static const char final_setup[][3] = {
    {0x42,0x07,0x3F}, // Disable SECAM detection, interferes with PAL C64
    {0x42,0x14,0x12},
    {0x42,0x2C,0xCE},
    {0x42,0x00,0x14},
    {0x42,0xC3,0x06},
    {0x42,0x3A,0x16},
    {0x42,0x1D,0x41},
    {0x42,0x00,0x56},
    {0x42,0x00,0x54},
    {0x54,0x04,0x13},
    {0x54,0x07,0x08},
    {0x42,0x2C,0xCE},
    {0x42,0x02,0x04},
    {0x42,0x08,0x72},
    {0x42,0x0A,0x00},
    {0x42,0xE3,0x8C},
    {0x42,0xE4,0x8C}
};

void do_final_setup(void)
{
    int i;
    int nak;

    for (i=0; i<sizeof(final_setup)/sizeof(final_setup[0]); i++) {
        nak = i2c_send(final_setup[i][0] >> 1, final_setup[i][1], final_setup[i][2]);
        if (nak) {
            write_byte('@');
	    return;
	} else {
            write_byte('.');
        }
    }

    write_byte('!');
}

static const char svideo_input[][2] = {
    {0x00, 0x16},
    {0xc3, 0x41},
    {0x3a, 0x12},
    {0x1d, 0x40}
};

void input_svideo(void)
{
    int i;
    int nak;

    for (i=0; i<sizeof(svideo_input)/sizeof(svideo_input[0]); i++) {
        nak = i2c_send(0x21, svideo_input[i][0], svideo_input[i][1]);
        if (nak) {
            write_byte('@');
	    return;
	} else {
            write_byte('.');
        }
    }

    write_byte('!');
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

    setup_adv7181();
    setup_adv7179();
    do_final_setup();
    input_svideo();

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
