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

void spi_setup() {
    SSPCON1bits.SSPEN = 0; // disable SPI while configuring
    
    // setup pins I/O
    TRISEbits.TRISE1 = 0; // output CSN
    TRISEbits.TRISE2 = 0; // output CE
    LATEbits.LATE1 = 1; // CSN is active-low, so set it high
    
    TRISCbits.TRISC3 = 0; // SCK is the serial clock output
    TRISCbits.TRISC4 = 1; // MISO / SDI is serial data input
    ANSELCbits.ANSC4 = 0; // digital read SDI
    TRISCbits.TRISC5 = 0; // MOSI / SDO serial data output
    
    SSPCON1bits.CKP = 0; // Idle state for clock is a LOW level
    SSPSTATbits.CKE = 1; // Transmit occurs on ACTIVE to IDLE clock state
    SSPSTATbits.SMP = 1; // Input data sampled at END data output time
    
    SSPCON1bits.SSPM = 0b0000; // SPI Master, clock = FOSC/4
    
    PIE1bits.SSPIE = 0; // Disable SPI interrupt (for now)
    
    SSPCON1bits.SSPEN = 1; // enable SPI
}

void button_action(char output) {
    LATDbits.LATD2 = output; // LED signal
    
    
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
    
    //watch_input(&button_action);
    char out = 1;
    while (1) {
        button_action(out);
        out = !out;
        __delay_ms(500);
    }
}
