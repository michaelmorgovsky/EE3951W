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

#define BUFSIZE 192
#define NUMSAMPLES 16

int adc_buffer[BUFSIZE];
int buffer_index = 0;

void pic24_init() { // Initializes T2, PINs, and CLK for pic24 operation
    _RCDIV = 0;
    AD1PCFG = 0xFFFF;
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
        lcd_setCursor(i,0); // for the first line
        lcd_printChar(s[i]); // Print the character
    }
}

void putVal(int ADCvalue) { // Puts a value from the ADCBUF into the circular buffer
    adc_buffer[buffer_index++] = ADCvalue;
    if(buffer_index >= BUFSIZE) {
        buffer_index = 0;
    }
}

void initBuffer() { // Initalizes all values in the circular buffer to 0
    int i;
    for(i=0; i < BUFSIZE; i++) {
        adc_buffer[i] = 0;
    }
}

void adc_init() { // ADC initialization
    AD1PCFGbits.PCFG11 = 0; // Pin 22, Analog pin 11
    AD1CHSbits.CH0SA = 11; // Pin 11
    
    AD1CON2bits.VCFG = 0b011; // Voltage reference from external pins 2 & 3 (Vref+ & Vref-)
    AD1CON3bits.ADCS = 1; // TAD = 125ns
    AD1CON1bits.SSRC = 0b010; // Sample on T3 Events
    AD1CON3bits.SAMC = 1; // 1 auto sample time bit
    AD1CON1bits.FORM = 0b00; // unsigned int
    
    AD1CON1bits.ASAM = 1; // auto sample
    AD1CON2bits.SMPI = 0b0000; //interrupt on every conversion of sequence
    AD1CON1bits.ADON = 1; // enable ADC
    
    // Enable ADC interrupts
    _AD1IF = 0;
    _AD1IE = 1;
    
    // Timer 3 configuration, 
    TMR3 = 0;
    T3CON = 0;
    T3CONbits.TCKPS = 0b10;
    PR3 = 3906; // 15624 = 16 samples/sec, 7812 = 32 samples/sec, 3906 = 64 samples/sec... etc (1953 = 128 samples/sec)
    T3CONbits.TON = 1;
}

void timer1_init() { // Initalize Timer1 for 100ms period to update LCD
    TMR1 = 0;
    T1CON = 0;
    T1CONbits.TCKPS = 0b10;
    PR1 = 24999;
    T1CONbits.TON = 1;
    _T1IP = 4;
    _T1IF = 0;
    _T1IE = 1;
}

void _ISR _ADC1Interrupt(void) { // On ADC1Interrupt put value into circular buffer
    _AD1IF = 0;
    putVal(ADC1BUF0);
}

double average(void) { // Calculate the average of the last NUMSAMPLES in buffer
    double temp = 0;
        for(int i=buffer_index - NUMSAMPLES; i < buffer_index; i++) { // from buffer index to NUMSAMPLES away
            if(i >= 0) { // If positive just sum voltage
                temp += adc_buffer[i];
            }
            else { // Otherwise "loop underflow" and grab from end of circular buffer, still last NUMSAMPLES samples
                temp += adc_buffer[BUFSIZE+i];
            }   
        }
    temp /= NUMSAMPLES; // SUM/NUM = average
    return temp;
}

void _ISR _T1Interrupt(void){ // 100ms interrupt, display voltage to LCD
    IFS0bits.T1IF = 0;
    char avg[20];
    sprintf(avg, "%3.2f C*", ((100/1024)*average()));
    lcd_printStr(avg);
}

int main(void) {
    pic24_init();
    lcd_init();
    adc_init();
    timer1_init();
    initBuffer();
    
    while(1);

    return 0;
}