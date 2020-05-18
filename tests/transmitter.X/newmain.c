/*
 * File:   newmain.c
 * Author: umnikos
 *
 * Created on May 13, 2020, 3:36 PM
 *
 * PIN NOTES:
 * D2 - LED OUT
 * C2 - BUTTON IN
 * C3 - SCL
 * C4 - SDI
 * C5 - SDO
 * E1 - TRANSMITTER CSN
 * E2 - TRANSMITTER CE
 */

#include <xc.h>
#include <pic16f1519.h>

#define _XTAL_FREQ 8000000 // 8 MHz
#pragma config WDTE=OFF // turn off watchdog timer

// all output pins as LAT abbreviations
#define LATLED LATDbits.LATD2
#define LATSCL LATCbits.LATC3
#define LATSDO LATCbits.LATC5
#define LATCSN LATEbits.LATE1
#define LATCE LATEbits.LATE2

// CSN pin needs to be set to low before
// this command and set high after you're done!
unsigned char writeSPIByte(unsigned char data) {
    SSPSTATbits.BF = 0; // set transmit/receiving to unfinished
    SSPBUF = data; // put data to be transmitted in the FIFO buffer
    while(SSPSTATbits.BF == 0){} // wait until transmit/receive is finished
    return SSPBUF;
}

unsigned char checkNRFAlive() {
    LATCSN = 0;
    writeSPIByte(0x3D); // feature register, idc about it
    writeSPIByte(0x04); // write some value to it
    LATCSN = 1;
    LATCSN = 0;
    writeSPIByte(0x1D); // then read it back
    unsigned char val = writeSPIByte(0xFF); // hopefully it's the same value
    LATCSN = 1;
    return val == 0x04;
}

void SPIGuard() {
    __delay_ms(10); // if it's about to die it better do so before the check.
    while(!checkNRFAlive()) {
        __delay_ms(5);
    }
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
    LATCE = 0; // In TX mode CE enables transmission
    __delay_ms(1);
    LATCSN = 1; // CSN is active-low, so set it high
    __delay_ms(100); // breathing time

    // set CONFIG TO PWR_UP, EN_CRC
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x20);
    writeSPIByte(0x0A);
    LATCSN = 1;

    // disable auto-ack, RX mode
    // shouldn't have to do this, but it won't TX if you don't
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x21);
    writeSPIByte(0x00);
    LATCSN = 1;

    // address width = 5
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x23);
    writeSPIByte(0x03);
    LATCSN = 1;

    // data rate = 1MB, signal strength 0dBm
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x26);
    writeSPIByte(0x06);
    LATCSN = 1;

    // 4 byte payload for pipe 0
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x31);
    writeSPIByte(0x04);
    LATCSN = 1;

    // auto retransmit on, 15 retransmits, 0.25ms delay
    // (accidentally fried the transceivers so they're faulty)
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x24);
    writeSPIByte(0x0F);
    LATCSN = 1;

    // set frequency channel to 5
    /*
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x25);
    writeSPIByte(0x05);
    LATCSN = 1;
     */

    // set TX address (different register for RX)
    const char *addr = "test1";
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x30);
    for (unsigned char j = 0; j < 5; j++) {
        writeSPIByte(addr[j]);
    }
    LATCSN = 1;
}

void nrf_transmit(const char* payload, unsigned char length) {
    if (length > 32) { // cannot transmit more than 32 bytes at a time!
        1/0; // too lazy to write error codes.
        return;
    }
    // load a payload
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0xA0); // W_TX_PAYLOAD
    for (int j = length-1; j >= 0; j--) {
        writeSPIByte(payload[j]);
    }
    LATCSN = 1;

    // pulse CE to start transmission
    LATCE = 1;
    __delay_ms(1);
    LATCE = 0;
}

void button_action(char output) {
    LATLED = output; // LED signal

    nrf_transmit("XXXXXXX", 7);
}

void watch_input(void(*action_func)(char param)) {
    /* remember the last 2 values of the input
     * for noise-free edge detection
     */
    char tailInput = 1;
    char lastInput = 1;
    char output = 0;
    button_action(output);

    // watch loop
    while (1) {
        const unsigned int noise_wait = 50; // ms
        char currentInput = PORTCbits.RC2;

        // falling edge
        if (tailInput == 1 && lastInput == 0 && currentInput == 0) {
            output = !output;
            button_action(output);
        }

        tailInput = lastInput;
        lastInput = currentInput;
        __delay_ms(noise_wait);
    }
}

void main() {

    OSCCON = 0b01110010; // set oscillator settings

    // setup button and LED I/O
    TRISDbits.TRISD2 = 0; // output LED
    TRISCbits.TRISC2 = 1; // input BUTTON
    ANSELCbits.ANSC2 = 0; // digital read C2

    spi_setup();
    nrf_setup();

    //watch_input(&button_action);
    char out = 1;
    while (1) {
        button_action(out);
        out = !out;
        __delay_ms(100);
    }
}
