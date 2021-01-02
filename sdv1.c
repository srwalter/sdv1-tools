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
void inline sleep_half_cycle(void)
{
    usleep(12);
}

void inline i2c_start()
{
    SCL = 1;
    sleep_half_cycle();
    SCL = 0;
    usleep(1);
    SDA = 1;
    sleep_half_cycle();
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
    sleep_half_cycle();

    SCL = 1;
    // Wait for SCL to rise in case of clock stretching
    while(!SCL);

    usleep(1);
    SDA = 1;
    sleep_half_cycle();
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

    sleep_half_cycle();
    SCL = 1;
    sleep_half_cycle();
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
    sleep_half_cycle();
    SCL = 1;
    // Wait for SCL to rise in case of clock stretching
    while(!SCL);
    if (SDA)
        val = 1;
    else
        val = 0;
    sleep_half_cycle();
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
    sleep_half_cycle();
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

void timer_usleep(int x)
{
    TMR0 = 65535 - (OSC_MHZ * x / 12);
    TR0 = 1;
    PCON |= PCON_IDLE;
}

static const char adv7181_setup[][2] = {
    // Luma Gain Control 1
    {0x2F,0xB4},
    // ADC Control, power down all ADCs
    {0x3A,0x1E},
    // Luma Gain Control 1
    {0x2F,0xB4},
    // Contrast
    {0x08,0x80},
    // AGC Mode Control, enable LAGC.1 and CAGC.1
    {0x2C,0xAE},
    // Misc Gain Control
    {0x2B,0xF2},
    // Reserved...
    {0xD2,0x01},
    // Reserved...
    {0xD3,0x01},
    // Reserved...
    {0xDB,0x9B},
    // Reserved...
    {0x2C,0xFE},
    // Reserved...
    {0x02,0x04},
    // Video Selection, Enable VSync and HSync processing
    {0x01,0xC8},
    // Output Control, 8-bit @ LLC1 4:2:2 ITU-R BT.656 
    {0x03,0x8C},
    // Analog Clamp Control, Current clamp enable (default)
    {0x14,0x12},
    // Digital Clamp Control 1, digital clamp timing slow
    {0x15,0x00},
    // Shaping Filter Control, C shaping SH3, auto narrow notch for Y
    {0x17,0x41},
    // VS and Field Control 1, default
    {0x31,0x12},
    // CTI DNR Control 2, 
    {0x50,0x04},
    // Lock Count, 100 lines in-lock, 100 lines out-of-lock, lock set only by horizonal lock
    {0x51,0x24},
    // ADC Switch 2, enable manual input signal muxing, no-connect ADC2
    {0xC4,0x80},
    // ADC Switch 1, no-connect ADC0/1
    {0xC3,0x06},
    // ADC Controll, power up ADC0
    {0x3A,0x16},
    // Extended Output Control, default except enable SPL pin
    {0x04,0x57},
    // Pixel Delay Control, no delay, auto align
    {0x27,0x58},
    // Hue, default no adjust
    {0x0B,0x00},
    // Manual Window, default except reserved bit 7 is set
    {0x3D,0xC3},
    // Reserved
    {0x3F,0xE4},
    // Reserved
    {0xB3,0xFE},
    // Power Management, default except reserved bit 6 is set
    {0x0F,0x40},
    // ADI Control, default except reserved bit 7 is set
    {0x0E,0x85},
    // CVBS input example from Datasheet
    {0x89,0x0D},
    {0x8D,0x9B},
    {0x8F,0x48},
    {0xB5,0x8B},
    {0xD4,0xFB},
    {0xD6,0x6D},
    {0xE2,0xAF},
    {0xE8,0xF3},
    {0x0E,0x05},
    // End example
    // Extended Output Control, enable ITU-R BT656-3/4 compatibility
    {0x04,0xD7},
    // Reserved (saturation)
    {0x09,0x80},
    // Resample Control, set SFL compatible to 719x series
    {0x41,0x10},
    // SD Saturation Cb (differs from datasheet example)
    {0xE3,0x80},
    // SD Saturation Cr (differs from datasheet examle)
    {0xE4,0x80},
    // NTSC V Bit End
    {0xE6,0x11},
    // Default Y Value, Disable free run mode, default Y to black
    {0x0C,0x00},
    // Default C value, 
    {0x0D,0x88},
    // ADI Control, default except reserved bit 7 set
    {0x0E,0x85},
    // Reserved
    {0x52,0x00},
    // Reserved
    {0x86,0x02},
    // ADC Control, default
    {0x0E,0x05},
    // Hue, no adjust
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
    // Mode Register 0, 1.3MHz low pass, Extended Mode, NTSC
    {0x00,0x10},
    // Mode Register 1, Power up DAC's, no CC, interlaced
    {0x01,0x10},
    // Duplicate
    {0x01,0x10},
    // Mode Register 2, disable square pixels, enable RTC pin, 
    {0x02,0x06},
    // Mode Register 3, DAC output Y/Pb/Pr
    {0x03,0x08},
    // Mode Register 4, Y/Pb/Pr output, pedestal on
    {0x04,0x13},
    // Timing Mode Register 0, slave timing, mode 0, blank output enable, no delay
    {0x07,0x00},
    // Timing Mode Register 1, 
    {0x08,0x00},
    // Subcarrier Frequency 0, as per datasheet
    {0x09,0x1E},
    // Subcarrier Frequency 1, as per datasheet
    {0x0A,0x7C},
    // Subcarrier Frequency 2, as per datasheet
    {0x0B,0xF0},
    // Subcarrier Frequency 3, as per datasheet
    {0x0C,0x21},
    // Subcarrier Phase Register, 0 = normal operation
    {0x0D,0x00},
    // CC Even Field
    {0x0E,0x00},
    // CC Even Field
    {0x0F,0x00},
    // CC Odd Field
    {0x10,0x00},
    // CC Odd Field
    {0x11,0x00},
    // NTSC Pedestal Control
    {0x12,0x00},
    // NTSC Pedestal Control
    {0x13,0x00},
    // NTSC Pedestal Control
    {0x14,0x00},
    // NTSC Pedestal Control
    {0x15,0x00},
    // CGMS_WSS Register 0
    {0x16,0x00},
    // CGMS_WSS Register 1
    {0x17,0x00},
    // CGMS_WSS Register 2
    {0x18,0x00},
    // Teletext Request Control Register
    {0x19,0x00},
    // Timing Register Reset Sequence
    {0x07,0x00},
    // Timing Register Reset Sequence
    {0x07,0x80},
    // Timing Register Reset Sequence
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
    // Autodetect Enable, Disable SECAM detection, interferes with PAL C64
    {0x42,0x07,0x3F},
    // Analog Clamp Control, defaults
    {0x42,0x14,0x12},
    // AGC Mode Control, AGC auto override through white peak, Auto IRE, Automatic Gain
    {0x42,0x2C,0xCE},
    // Input Control, Auto-detect NTSC with pedestal
    {0x42,0x00,0x14},
    // ADC Switch 1, ADC0/1 no-connect
    {0x42,0xC3,0x06},
    // ADC Control, power up ADC 0
    {0x42,0x3A,0x16},
    // Reserved
    {0x42,0x1D,0x41},
    // Input Control, S-Video input, NTSC(M)
    {0x42,0x00,0x56},
    // Input Control, NTSC(M), lower nibble invalid?
    {0x42,0x00,0x54},
    // Mode Register 4, Y/Pb/Pr output, pedetal on
    {0x54,0x04,0x13},
    // Timing Mode Register 0, disable blank input control
    {0x54,0x07,0x08},
    // AGC Mode Control, AGC auto override through white peak, Auto IRE, Automatic Gain
    {0x42,0x2C,0xCE},
    // Reserved
    {0x42,0x02,0x04},
    // Contrast
    {0x42,0x08,0x72},
    // Brightness
    {0x42,0x0A,0x00},
    // SD Saturation Cb
    {0x42,0xE3,0x8C},
    // SD Saturation Cr
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
    // Input Control, S-Video
    {0x00, 0x16},
    // ADC Switch 1, ADC0 to AIN2, ADC1 to AIN4
    {0xc3, 0x41},
    // ADC Control, power up ADC0 and ADC1
    {0x3a, 0x12},
    // Reserved
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

static const char cvbs_input[][2] = {
    // Input Control, doesn't make sense per spec (XXX)
    {0x00, 0x14},
    // ADC Switch 1, ADC 0/1 no-connect (XXX)
    {0xc3, 0x06},
    // ADC Control, power up ADC 0
    {0x3a, 0x16},
    // Reserved
    {0x1d, 0x41}
};

void input_cvbs(void)
{
    int i;
    int nak;

    for (i=0; i<sizeof(cvbs_input)/sizeof(cvbs_input[0]); i++) {
        nak = i2c_send(0x21, cvbs_input[i][0], cvbs_input[i][1]);
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
    char locked = 0;

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
    sleep_half_cycle();

    setup_adv7181();
    setup_adv7179();
    do_final_setup();
    input_svideo();

    P0_1 = 0;
    P0_0 = 0;
    for (;;) {
        SCL = 1;
        SDA = 1;

        if (!RI) {
            // No UART data to process, do normal stuff
            if (!locked) {
                val = i2c_recv(0x21, 0x10);
                if (val & 1) {
                    // Got a lock
                    locked = 1;
                    if (val & 0x60) {
                        // PAL
                        P0_0 = 1;
                        // Mode Register 0, extended mode, PAL B/D/G/H/I
                        i2c_send(0x2a, 0, 0x11);
                    } else {
                        // NTSC
                        P0_1 = 1;
                    }
                }
            }

            sleep_max();
            continue;
        }

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
