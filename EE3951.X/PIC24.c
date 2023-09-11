/* 
 * File:   morgo002_lab5_LCDLib.c
 * Author: Mike
 *
 * Created on November 10, 2022, 2:20 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <string.h>

#include "xc.h"
// CW1: FLASH CONFIGURATION WORD 1 (see PIC24 Family Reference Manual 24.1)
#pragma config ICS = PGx1          // Comm Channel Select (Emulator EMUC1/EMUD1 pins are shared with PGC1/PGD1)
#pragma config FWDTEN = OFF        // Watchdog Timer Enable (Watchdog Timer is disabled)
#pragma config GWRP = OFF          // General Code Segment Write Protect (Writes to program memory are allowed)
#pragma config GCP = OFF           // General Code Segment Code Protect (Code protection is disabled)
#pragma config JTAGEN = OFF        // JTAG Port Enable (JTAG port is disabled)


// CW2: FLASH CONFIGURATION WORD 2 (see PIC24 Family Reference Manual 24.1)
#pragma config I2C1SEL = PRI       // I2C1 Pin Location Select (Use default SCL1/SDA1 pins)
#pragma config IOL1WAY = OFF       // IOLOCK Protection (IOLOCK may be changed via unlocking seq)
#pragma config OSCIOFNC = ON       // Primary Oscillator I/O Function (CLKO/RC15 functions as I/O pin)
#pragma config FCKSM = CSECME      // Clock Switching and Monitor (Clock switching is enabled, 
// Fail-Safe Clock Monitor is enabled)
#pragma config FNOSC = FRCPLL      // Oscillator Select (Fast RC Oscillator with PLL module (FRCPLL))

void pic24_init() { // Initializes T2, PINs, and CLK for pic24 operation
    _RCDIV = 0;
    AD1PCFG = 0xFFFF;
    
    T2CON = 0;
    T2CONbits.TCKPS = 0b10;
    PR2 = 62500; // 1/8th s cycle
    TMR2 = 0;
    IEC0bits.T2IE = 1; // Enable T2 Interrupt
    _T2IF = 0;
    T2CONbits.TON = 1;
}

void delay_ms(unsigned int ms) { // Blocking delay for a given amount of miliseconds
    while (ms-- > 0) {
        asm("repeat #15998");
        asm("nop");
    }       
}

void blockingWait(void) { // Simple helper function to check whether an acknowledgement bit has been received
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
}

void lcd_cmd(char Package) { // Sends a command using ISR given an input, handles the addressing and control byte
    IFS3bits.MI2C2IF = 0; 
    
    I2C2CONbits.SEN = 1; // Start Condition
    while(I2C2CONbits.SEN == 1); // SEN clears when start is complete
    IFS3bits.MI2C2IF = 0; // Clear MI2C2 IF
    
    I2C2TRN = 0b01111100; //Write to address
    blockingWait();
    
    I2C2TRN = 0b00000000; //8 bit control byte
    blockingWait();
    
    I2C2TRN = Package; // 8 bit data byte
    blockingWait();
    
    I2C2CONbits.PEN = 1;
    while(I2C2CONbits.PEN == 1); // PEN clears when Stop is complete
}

void lcd_init(void) { // Initializes the LCD 
    I2C2CONbits.I2CEN = 0; // Disable I2C for saftey
    I2C2BRG = 0x009D; // Set I2C FRQ to 100 MHz
    IFS3bits.MI2C2IF = 0; // Clear IF
    I2C2CONbits.I2CEN = 1; // Enable I2C
    
    delay_ms(40);
    
    lcd_cmd(0b00111000); // function set
    lcd_cmd(0b00111001); // function set, advance instruction mode
    lcd_cmd(0b00010100); // interval osc
    lcd_cmd(0b01110000); // contrast Low
    lcd_cmd(0b01010110);
    lcd_cmd(0b01101100); // follower control
    
    delay_ms(200);
    
    lcd_cmd(0b00111000); // function set
    lcd_cmd(0b00001100); // Display on
    lcd_cmd(0b00000001); // Clear Display
    
    delay_ms(1);
}

void lcd_setCursor(char x, char y) { // Sets the cursor on the LCD
    char Cursor = (64*y) + x + 128; // Creates the command, 0b1y000xxx 
    lcd_cmd(Cursor); // MSB means writing to DRAM and y and xxx are values for the cursor
    
}

void lcd_printChar(char Package) { // Prints the character
    IFS3bits.MI2C2IF = 0; 
    
    I2C2CONbits.SEN = 1; // Start Condition
    while(I2C2CONbits.SEN == 1); // SEN clears when start is complete
    IFS3bits.MI2C2IF = 0; // Clear MI2C2 IF
    
    I2C2TRN = 0b01111100; // Write to address
    blockingWait();
    
    I2C2TRN = 0b01000000; // 8 bit control byte, RS = 1
    blockingWait();
    
    I2C2TRN = Package; // 8 bit data byte
    blockingWait();
    
    I2C2CONbits.PEN = 1;
    while(I2C2CONbits.PEN == 1); // PEN clears when Stop is complete
} 

void lcd_printStr(const char s[]) { // Prints a string to the LCD display
    for(int i=0; i < strlen(s); i++) { // Loop over length of the string
        if(i >= 8) { 
            lcd_setCursor(i-8,1); // For all positions of the string 8+, print them on the second LCD line
        }
        else {
            lcd_setCursor(i,0); // Otherwise for the first line
        }
        lcd_printChar(s[i]); // Print the character
    }
}

int main(void) {
    pic24_init();
    lcd_init();
    
    char string[] = "Lab5 LCD    Test";
    lcd_printStr(string);
    
    for(int i=0; i < 7; i++) {
        lcd_cmd(0b00011000);
    }
}