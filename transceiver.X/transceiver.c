/*
 * File:   newmain.c
 * Author: umnikos
 *
 * Created on May 21, 2020, 10:38 PM
 *
 * PIN NOTES:
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

/* <DEFINITIONS> */

#define _XTAL_FREQ 8000000 // 8 MHz
#pragma config WDTE=OFF // turn off watchdog timer

#define byte unsigned char

// all output pins as LAT abbreviations
#define LATLED LATDbits.LATD2
#define LATSCL LATCbits.LATC3
#define LATSDO LATCbits.LATC5
#define LATCSN LATEbits.LATE1
#define LATCE LATEbits.LATE2

char out = 1; // an output variable for the LED

char receive_buffer[receive_length];
#if receive_length > 32 // cannot transmit more than 32 bytes at a time
#error
#endif

// printable antipodal characters
// (their sum is 0b01111111)
#define CHAR_OFF 'N'
#define CHAR_ON  '1'

/* <CODE> */

// CSN pin needs to be set to low before
// this command and set high after you're done!
byte writeSPIByte(byte data) {
    // this function is not reentrant so it shouldn't be interrupted
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
    SLEEP(); // sleep until interrupt calls nrf_postreceive()
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
    byte count;
    count = 0;
    for (byte i=0; i<receive_length; i++) {
        if (receive_buffer[i] == CHAR_OFF) {
            count++;
        }
    }
    if (count >= correctness_threshold) {
        LATLED = 0;
    }
    count = 0;
    for (byte i=0; i<receive_length; i++) {
        if (receive_buffer[i] == CHAR_ON) {
            count++;
        }
    }
    if (count >= correctness_threshold) {
        LATLED = 1;
    }
}
#endif

void __interrupt() nrf_int() {
    if (IOCBFbits.IOCBF0) {
        IOCBF &= 0b11111110;
        // IRQ can be set when a packet is received...
        #if mode == 1
            nrf_postreceive();
        #endif
        // ...or when an ACK is received (disabled currently)
    }
}

#if mode == 0
void button_action() {
    LATLED = out; // LED signal
    if (out == 0) {
        for (int i=0; i<5000; i++) {
            nrf_transmit(CHAR_OFF);
        }
    } else {
        for (int i=0; i<5000; i++) {
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

void main() {
    OSCCON = 0b01110010; // set oscillator settings

    // setup button and LED I/O
    TRISDbits.TRISD2 = 0; // output LED
    TRISCbits.TRISC2 = 1; // input BUTTON
    ANSELCbits.ANSC2 = 0; // digital read C2

    spi_setup();
    int_setup();
    nrf_setup();

    #if mode == 0
        watch_input(&button_action);
    #endif
    #if mode == 1
        while (1) {
            nrf_receive();
            out = !out;
        }
    #endif
}

