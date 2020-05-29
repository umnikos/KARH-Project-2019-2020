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

#include <xc.h>
#include <pic16f1519.h>

#define _XTAL_FREQ 8000000 // 8 MHz
#pragma config WDTE=OFF // turn off watchdog timer

#define byte unsigned char

// all output pins as LAT abbreviations
#define LATLED LATDbits.LATD2
#define LATSCL LATCbits.LATC3
#define LATSDO LATCbits.LATC5
#define LATCSN LATEbits.LATE1
#define LATCE LATEbits.LATE2

/* MODES:
 * 0 - TX (transmitter)
 * 1 - RX (receiver)
 */
const byte mode = 1;

char out = 1; // an output variable for the LED

// CSN pin needs to be set to low before
// this command and set high after you're done!
byte writeSPIByte(byte data) {
    SSPSTATbits.BF = 0; // set transmit/receiving to unfinished
    SSPBUF = data; // put data to be transmitted in the FIFO buffer
    while(SSPSTATbits.BF == 0){} // wait until transmit/receive is finished
    return SSPBUF;
}

byte somevalue = 0;
byte checkNRFAlive() {
    somevalue++;
    LATCSN = 0;
    writeSPIByte(0x2B); // RX P1's channel, disabled for this purpose
    writeSPIByte(somevalue); // write some value to it
    LATCSN = 1;
    LATCSN = 0;
    writeSPIByte(0x0B); // then read it back
    byte val = writeSPIByte(0xFF); // hopefully it's the same value
    LATCSN = 1;
    return val == somevalue;
}

void SPIGuard() {
    //__delay_ms(10); // if it's about to die it better do so before the check.
    /* this check has never ever failed
     * so let's just replace it with the actual message.
    while(!checkNRFAlive()) {
        __delay_ms(15);
    } */
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
    __delay_ms(100); // breathing time
    
    // disable RX pipe 1 for SPIGuard usage
    SPIGuard(); // this is before setting PWR_UP so it doesn't matter
    LATCSN = 0;
    writeSPIByte(0x22);
    writeSPIByte(0x01);
    LATCSN = 1;
    
    // set CONFIG.
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x20);
    if (mode == 0) { // TX -> PWR_UP, PTX
        writeSPIByte(0b00000010);
    }
    if (mode == 1) { // RX -> PWR_UP, PRX
        writeSPIByte(0b00000011);
    }
    LATCSN = 1;
    
    // disable auto-ack
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x21);
    writeSPIByte(0x00);
    LATCSN = 1;
    
    // set CONFIG again so that CRC is disabled
    // (auto-ack forces CRC to be enabled)
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x20);
    if (mode == 0) { // TX -> PWR_UP, PTX
        writeSPIByte(0b00000010);
    }
    if (mode == 1) { // RX -> PWR_UP, PRX
        writeSPIByte(0b00000011);
    }
    LATCSN = 1;
    
    // disable auto retransmit
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x24);
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

    // set address for pipe 0
    const char *addr = "test1";
    SPIGuard();
    LATCSN = 0;
    if (mode == 0) {
        writeSPIByte(0x30); 
    }
    if (mode == 1) {
        writeSPIByte(0x2A);
    }
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

void nrf_transmit(const char* payload, byte length) {
    // set frequency channel to 2
    // and reset lost packet count
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x25);
    writeSPIByte(0x02);
    LATCSN = 1;
    
    if (length > 32) { // cannot transmit more than 32 bytes at a time!
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

char* receive_buffer;
byte receive_length;
void nrf_receive(char* buffer, byte length) {
    receive_buffer = buffer;
    receive_length = length;
    LATCE = 1; // enable receiving
    SLEEP();
}

void nrf_postreceive() {
    LATCE = 0; // stop receiving please.
    char* buffer = receive_buffer;
    byte length = receive_length;
    // perform check if any data was received (not sure if it's needed)
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x17);
    byte status = writeSPIByte(0xFF);
    LATCSN = 1;
    if ((status & 0x01) == 0) { // if RX_FIFO not empty
        // extract data from nrf into a buffer
        SPIGuard();
        LATCSN = 0;
        writeSPIByte(0x61);
        for (int j = length-1; j >= 0; j--) {
            buffer[j] = writeSPIByte(0xFF);
        }
        LATCSN = 1;
    }
    // reset IRQ back to high
    SPIGuard();
    LATCSN = 0;
    writeSPIByte(0x27);
    writeSPIByte(0xFF);
    LATCSN = 1;
    // 7 variables for debugging purposes
    char a0,a1,a2,a3,a4,a5,a6;
    a0 = buffer[0];
    a1 = buffer[1];
    a2 = buffer[2];
    a3 = buffer[3];
    a4 = buffer[4];
    a5 = buffer[5];
    a6 = buffer[6];
    NOP();
    NOP();
    byte count = 0;
    for (byte i=0; i<length; i++) {
        if (buffer[i] == 'X') {
            count++;
        }
    }
    if (count >= 3) {
        LATLED = out; // LED signal
    }
}

void __interrupt(high_priority) nrf_int() {
    if (IOCBFbits.IOCBF0) {
        IOCBF &= 0b11111110;
        nrf_postreceive();
    }
}

void button_action(char output) {
    if (mode == 0) {
        LATLED = output; // LED signal
        nrf_transmit("XXXXXXX", 7);
    }
    if (mode == 1) {
        const byte length = 7;
        char buffer[7] = "lollol";
        nrf_receive(buffer, length);
    }
}

void led_success() {
    SPIGuard();
    LATCSN = 0;
    byte status = writeSPIByte(0xFF);
    LATCSN = 1;
    if (status & 0x20) {
        LATLED = 1;
    } else {
        LATLED = 0;
    }
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

void mainloop();
void main() {

    OSCCON = 0b01110010; // set oscillator settings

    // setup button and LED I/O
    TRISDbits.TRISD2 = 0; // output LED
    TRISCbits.TRISC2 = 1; // input BUTTON
    ANSELCbits.ANSC2 = 0; // digital read C2

    spi_setup();
    nrf_setup();
    int_setup();


    mainloop();
}

void mainloop() {
    //watch_input(&button_action);
    while (1) {
        button_action(out);
        out = !out;
    }
}

