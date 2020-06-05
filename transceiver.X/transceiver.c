/*
 * File:   newmain.c
 * Author: umnikos
 *
 * Created on May 21, 2020, 10:38 PM
 *
 * PIN NOTES:
 * A0 - CAPACITOR POSITIVE
 * A1 - CAPACITOR ENABLE
 * A2 - CAPACITOR NEGATIVE
 * A3 - HBRIDGE 'N'
 * A4 - HBRIDGE '1'
 * B0 - IRQ INTERRUPT
 * D2 - LED OUT
 * C2 - BUTTON IN
 * C3 - SCL
 * C4 - SDI
 * C5 - SDO
 * E1 - TRANSMITTER CSN
 * E2 - TRANSMITTER CE
 */

/* <IMPORTS> */

#include <xc.h>
#include <pic16f1519.h>

/* <CONFIGURATION> */

// MODES:
// 0 - TX (transmitter)
// 1 - RX (receiver)
#define mode 1

// how long the transmitted/received message is
#define receive_length 1
// how many bytes need to be correct from the received message
#define correctness_threshold 1

// how long to recharge the voltage doubling capacitor
#define recharge_ms 50

/* <DEFINITIONS> */

#define _XTAL_FREQ 8000000 // 8 MHz
#pragma config WDTE=OFF // turn off watchdog timer

#define byte unsigned char

// all output pins as LAT abbreviations
#define nCAPPO LATAbits.LATA0
#define nCAPEN LATAbits.LATA1
#define CAPNEG LATAbits.LATA2
#define HBRN LATAbits.LATA3
#define HBR1 LATAbits.LATA4
#define LATLED LATDbits.LATD2
#define LATSCL LATCbits.LATC3
#define LATSDO LATCbits.LATC5
#define LATCSN LATEbits.LATE1
#define LATCE LATEbits.LATE2

// TX mode - what state to send next (CHAR_ON or CHAR_OFF)
char out = 1;

char receive_buffer[receive_length];
#if receive_length > 32 // cannot transmit more than 32 bytes at a time
#error
#endif

// printable antipodal characters
// (their sum is 0b01111111)
#define CHAR_OFF 'N' // 0b01001110
#define CHAR_ON  '1' // 0b00110001

/* <CODE> */

#if mode == 1
void capacitor_recharge() {
    nCAPPO = !1;
    CAPNEG = 1;
    __delay_ms(recharge_ms);
    nCAPPO = !0;
    CAPNEG = 0;
}

void relay_reset() {
    HBRN = 0;
    HBR1 = 0;
    nCAPEN = !0;
    nCAPPO = !0;
    CAPNEG = 0;
}

void relay_setup() {
    // set all pins to output
    TRISA = 0;
    // disable H-bridge
    HBRN = 0;
    HBR1 = 0;
    // charge capacitor
    nCAPEN = !0;
    CAPNEG = 1;
    nCAPPO = !1;
    __delay_ms(500);
    CAPNEG = 0;
    nCAPPO = !0;
}

void relay_n() {
    relay_reset();
    nCAPEN = !1;
    HBRN = 1;
    __delay_ms(50);
    HBRN = 0;
    nCAPEN = !0;
    capacitor_recharge();
    relay_reset();
}

void relay_1() {
    relay_reset();
    nCAPEN = !1;
    HBR1 = 1;
    __delay_ms(50);
    HBR1 = 0;
    nCAPEN = !0;
    capacitor_recharge();
    relay_reset();
}
#endif

// CSN pin needs to be set to low before
// this command and set high after you're done!
byte writeSPIByte(byte data) {
    // this function is non-reentrant so it shouldn't be interrupted
    byte previousGIE = INTCONbits.GIE;
    INTCONbits.GIE = 0;

    SSPBUF = data; // put data to be transmitted in the FIFO buffer
    SSPSTATbits.BF = 0; // set transmit/receiving to unfinished
    while(SSPSTATbits.BF == 0){} // wait until transmit/receive is finished

    byte result = SSPBUF;
    INTCONbits.GIE = previousGIE;
    return result;
}

void spi_setup() {
    SSPCON1bits.SSPEN = 0; // disable SPI while configuring

    // setup pins I/O
    TRISEbits.TRISE1 = 0; // output CSN
    TRISEbits.TRISE2 = 0; // output CE, set to high in nrf_setup()

    TRISCbits.TRISC3 = 0; // SCK is the serial clock output
    TRISCbits.TRISC4 = 1; // MISO / SDI is serial data input
    ANSELCbits.ANSC4 = 0; // digital read SDI
    TRISCbits.TRISC5 = 0; // MOSI / SDO serial data output

    SSPCON1bits.CKP = 0; // Idle state for clock is a 0 level
    SSPSTATbits.CKE = 1; // Transmit occurs on ACTIVE to IDLE clock state
    SSPSTATbits.SMP = 1; // Input data sampled at END data output time

    SSPCON1bits.SSPM = 0b0000; // SPI Master, clock = FOSC/4

    PIE1bits.SSPIE = 0; // Disable SPI interrupt (for now)

    SSPCON1bits.SSPEN = 1; // enable SPI
}

void nrf_setup() {
    LATCE = 0; // enables receiving in RX mode and transmitting in TX mode
    __delay_ms(1);
    LATCSN = 1; // CSN is active-low, so set it high
    __delay_ms(2); // breathing time

    // set CONFIG.
    LATCSN = 0;
    writeSPIByte(0x20);
    #if mode == 0
        writeSPIByte(0b00000010); // TX -> PWR_UP, PTX
    #endif
    #if mode == 1
        writeSPIByte(0b00000011); // RX -> PWR_UP, PRX
    #endif
    LATCSN = 1;
    // you need 50 nanoseconds between transmissions
    // but an instruction takes longer to execute than that.

    // disable auto-ack
    LATCSN = 0;
    writeSPIByte(0x21);
    writeSPIByte(0x00);
    LATCSN = 1;

    // set CONFIG again so that CRC is disabled
    // (auto-ack forces CRC to be enabled)
    LATCSN = 0;
    writeSPIByte(0x20);
    #if mode == 0
        writeSPIByte(0b00000010); // TX -> PWR_UP, PTX
    #endif
    #if mode == 1
        writeSPIByte(0b00000011); // RX -> PWR_UP, PRX
    #endif
    LATCSN = 1;

    // set frequency channel to 2
    LATCSN = 0;
    writeSPIByte(0x25);
    writeSPIByte(0x02);
    LATCSN = 1;

    // disable auto retransmit
    LATCSN = 0;
    writeSPIByte(0x24);
    writeSPIByte(0x00);
    LATCSN = 1;

    // address width = 5
    LATCSN = 0;
    writeSPIByte(0x23);
    writeSPIByte(0x03);
    LATCSN = 1;

    // data rate = 1MB, signal strength 0dBm
    LATCSN = 0;
    writeSPIByte(0x26);
    writeSPIByte(0x06);
    LATCSN = 1;

    // 1 byte payload for pipe 0
    LATCSN = 0;
    writeSPIByte(0x31);
    writeSPIByte(0x01);
    LATCSN = 1;

    // set address for pipe 0
    const char *addr = "test1";
    LATCSN = 0;
    // different addresses for TX and RX
    #if mode == 0
        writeSPIByte(0x30);
    #endif
    #if mode == 1
        writeSPIByte(0x2A);
    #endif
    for (byte j = 0; j < 5; j++) {
        writeSPIByte(addr[j]);
    }
    LATCSN = 1;
}

// configure interrupts (both internal and external)
void int_setup() {
    TRISBbits.TRISB0 = 1; // set INT pin to read (bruh)
    ANSELBbits.ANSB0 = 0; // digital read

    INTCONbits.GIE = 1; // global interrupt enable
    INTCONbits.PEIE = 1; // enable interrupt from peripherals
    //INTCONbits.INTE = 1; // interrupt enable
    //OPTION_REGbits.INTEDG = 0; // falling edge detect
    INTCONbits.IOCIE = 1; // interrupt on change enable
    IOCBNbits.IOCBN0 = 1; // falling edge detect
}

void timer1_reset() {
    TMR1H = 240; // preset for timer1 MSB register
    TMR1L = 221; // preset for timer1 LSB register
}

// Timer0 is disabled during sleep so we use Timer1
#if mode == 1
void timer1_setup() {
    //Timer1 Registers Prescaler= 1 - TMR1 Preset = 61661 - Freq = 2.00 Hz - Period = 0.500000 seconds
    T1CONbits.T1CKPS1 = 0;   // bits 5-4  Prescaler Rate Select bits
    T1CONbits.T1CKPS0 = 0;   // bit 4
    T1CONbits.T1OSCEN = 1;   // bit 3 Timer1 Oscillator Enable Control bit 1 = on
    T1CONbits.nT1SYNC = 1;   // bit 2 Timer1 External Clock Input Synchronization Control bit...1 = Do not synchronize external clock input
    T1CONbits.TMR1CS = 0b11; // bit 1 Timer1 Clock Source Select bit...0b11 = LFINTOSC

    TMR1H = 0; // clear offset registers before enabling interrupts
    TMR1L = 0;
    PIR1bits.TMR1IF = 0;
    PIE1bits.TMR1IE = 1; // enable Timer1 interrupts on overflow
    timer1_reset();      // (re)set offset registers
    T1CONbits.TMR1ON = 1; // bit 0 enables timer
}
#endif

#if mode == 0
void nrf_transmit(const char payload) {
    // load a payload
    LATCSN = 0;
    writeSPIByte(0xA0); // W_TX_PAYLOAD
    for (int j = receive_length-1; j >= 0; j--) {
        writeSPIByte(payload);
    }
    LATCSN = 1;

    // pulse CE to start transmission
    LATCE = 1;
    __delay_us(20);
    LATCE = 0;
    // __delay_us(100); // delay between transmissions
}
#endif

#if mode == 1
void nrf_receive() {
    LATCE = 1; // enable receiving
    __delay_ms(1); // wait for a receive
    LATCE = 0; // disable receiving
    SLEEP(); // nothing received, go to sleep
}

void nrf_postreceive() {
    LATCE = 0; // stop receiving please.
    // perform check if any data was received (not sure if it's needed)
    LATCSN = 0;
    writeSPIByte(0x17);
    byte status = writeSPIByte(0xFF);
    LATCSN = 1;
    if ((status & 0x01) == 0) { // if RX_FIFO not empty
        // extract data from nrf into a buffer
        LATCSN = 0;
        writeSPIByte(0x61);
        for (int j = receive_length-1; j >= 0; j--) {
            receive_buffer[j] = writeSPIByte(0xFF);
        }
        LATCSN = 1;
    }
    // reset IRQ back to high
    LATCSN = 0;
    writeSPIByte(0x27);
    writeSPIByte(0xFF);
    LATCSN = 1;
    NOP(); // for debugging purposes
    // decode received message into a command
    byte on_count = 0;
    byte off_count = 0;
    for (byte i=0; i<receive_length; i++) {
        if (receive_buffer[i] == CHAR_ON) {
            on_count++;
        } else if (receive_buffer[i] == CHAR_OFF) {
            off_count++;
        }
    }
    if (on_count >= correctness_threshold) {
        LATLED = 1;
        relay_1();
        SLEEP();
    } else if (off_count >= correctness_threshold) {
        LATLED = 0;
        relay_n();
        SLEEP();
    } else {
        nrf_receive();
    }
}
#endif

void __interrupt() int_handler() {
    if (IOCBFbits.IOCBF0) {
        IOCBF &= 0b11111110;
        // IRQ can be set when a packet is received...
        #if mode == 1
            nrf_postreceive();
        #endif
        // ...or when an ACK is received (disabled currently)
        return;
    }
    if (PIR1bits.TMR1IF == 1) {
        timer1_reset();
        PIR1bits.TMR1IF = 0;
        #if mode == 1
            nrf_receive();
        #endif
    }
}

#if mode == 0
void button_action() {
    LATLED = out; // LED signal
    if (out == 0) {
        for (int i=0; i<5050; i++) {
            nrf_transmit(CHAR_OFF);
        }
    } else {
        for (int i=0; i<5050; i++) {
            nrf_transmit(CHAR_ON);
        }
    }
}
#endif

#if mode == 0
void watch_input(void(*action_func)()) {
    // remember the last 2 values of the input
    // for noise-free edge detection
    char tailInput = 1;
    char lastInput = 1;

    // watch loop
    while (1) {
        const unsigned int noise_wait = 50; // ms
        char currentInput = PORTCbits.RC2;

        // falling edge
        if (tailInput == 1 && lastInput == 0 && currentInput == 0) {
            button_action();
            out = !out;
        }

        tailInput = lastInput;
        lastInput = currentInput;
        __delay_ms(noise_wait);
    }
}
#endif

void led_setup() {
    TRISDbits.TRISD2 = 0; // output LED
    TRISCbits.TRISC2 = 1; // input BUTTON
    ANSELCbits.ANSC2 = 0; // digital read C2
    LATLED = 0;
}

void main() {
    OSCCON = 0b01110010; // set oscillator settings

    #if mode == 1
        relay_setup();
    #endif
    spi_setup();
    nrf_setup();
    led_setup();
    int_setup();

    #if mode == 0
        watch_input(&button_action);
    #endif
    #if mode == 1
        timer1_setup();
        SLEEP();
    #endif
}

