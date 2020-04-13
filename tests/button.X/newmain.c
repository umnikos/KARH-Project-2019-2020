/*
 * File:   newmain.c
 * Author: umnikos
 *
 * Created on 24 March 2020, 19:40
 */

#include <xc.h>
#include <pic16f1519.h>

#define _XTAL_FREQ 8000000 // 8 MHz
#pragma config WDTE=OFF // turn off watchdog timer

void main() {

    OSCCON = 0b01110010; // set oscillator settings

    TRISDbits.TRISD2 = 0; // output pin D2
    TRISCbits.TRISC3 = 1; // input pin
    ANSELCbits.ANSC3 = 0; // digital read
    
    char tailInput = 1;
    char lastInput = 1;
    char output = 0;
    LATDbits.LATD2 = output;
    
    while (1) {
       
        const unsigned int noise_wait = 50; // ms
        char currentInput = PORTCbits.RC3;
        
        if (tailInput == 1 && lastInput == 0 && currentInput == 0) {
            output = !output;
            LATDbits.LATD2 = output;
        }
         
        tailInput = lastInput;
        lastInput = currentInput;
        __delay_ms(noise_wait);
    } 
}
