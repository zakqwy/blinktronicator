/*
	Blinktronicator
	Z. Fredin
	12/27/2015
	MIT license

	more info: https://hackaday.io/project/8331-blinktronicator
*/

#include <avr/io.h>
#include <avr/interrupt.h>

/* Shift register pin definitions */
#define PIN_DATA 0	//14 DS			ATtiny45 pin 5, PB0 	schematic name PIN_SER
#define PIN_CLOCK 3 	//11 SHCP		ATtiny45 pin 2, PB3 	schematic name PIN_SRCLK
//	PIN_MR [H] 	//10 master reset	connected to VCC	schematic name PIN_SRCLR
#define PIN_LATCH 1  	//12 STCP		ATtiny45 pin 6, PB1	schematic name PIN_RCLK
//	PIN_OE [L]	//13 output enable	connected to GND	schematic name PIN_G

/* Testing pins */
#define PIN_MISO 1 	//ATtiny45 pin 6, PB1
#define PIN_MOSI 0	//ATtiny45 pin 5, PB0

/* Switch pins */
#define SW_LEFT 4	//ATtiny45 pin 3, PB4
#define SW_RIGHT 2	//ATtiny45 pin 7, PB2	

void SystemInit(void) {
/* Port B configuration */
	DDRB |= ((1 << PIN_DATA) + (1 << PIN_CLOCK) + (1 << PIN_LATCH));//sets three I/O pins as outputs to send signals to 74HC595
									//note that MISO is an output for testing (set via PIN_RCLK)
	PORTB = 0;	
}

void updateLEDs(unsigned char ledOutput1, unsigned char ledOutput2) {
	/* this could probably be more efficient. the second it started working, I saved the function and moved on. */

	int i;
	PORTB &= ~(1<<PIN_LATCH); //starts by clearing latch pin	
	
	for (i = 0; i <= 7; i++) { //first eight bits... 
		if (ledOutput1 & (1<<i)) { //runs through the eight bits that make up ledOutput1
			PORTB |= (1<<PIN_DATA); //sets data pin if it should be high
		}
		PORTB |= (1<<PIN_CLOCK); //sets clock pin (after data pin!)
		PORTB &= ~(1<<PIN_DATA); //clears data pin regardless of its status
		PORTB &= ~(1<<PIN_CLOCK); //clears clock pin
	}

	for (i = 0; i <= 7; i++) {	
		if (ledOutput2 & (1<<i)) {
			PORTB |= (1<<PIN_DATA);
		}
		PORTB |= (1<<PIN_CLOCK);
		PORTB &= ~(1<<PIN_DATA);
		PORTB &= ~(1<<PIN_CLOCK);
	}

	PORTB |= (1<<PIN_LATCH); //sets latch pin to update display
	PORTB &= ~(1<<PIN_LATCH);
}

/* this function would be cool:

void displayValue(unsigned char value) {
uint8_t lowerLED_index = value

//	turn off all LEDs

//	fading: pulling in an 8-bit number and displaying it on a 4-bit display (0-15)
//	each LED pair needs 16 intermediary values (try eight?)

//	determine LED pair

uint8_t lowerLED_index = (value >> 4) //shift right four to divide value by four, rounding down (lower LED of pair)
*/

int main(void) {
	SystemInit();

	// demo code: 2-LED-wide chaser 
	long counter = 0;
	long counterMax = 23;	
	unsigned char ledOutput1[24] = {0x80, 0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03,
					0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00 ,0x00 ,0x00, 0x00, 0x00, 0x00, 0x00
					}; //LSB is D9 (580nm), MSB is D16 (630nm)
	unsigned char ledOutput2[24] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x80, 0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03,
					0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
					}; //LSB is D1 (white), MSB is D8 (576nm)

	int fastLoop = 0;
	int fastLoopOverflow = 10;

	long slowLoop = 0;
	long slowLoopOverflow = 50000; //5000: speedy but visible


	for(;;) {
		if (fastLoop == fastLoopOverflow) {

/*	testing pushbutton interface
		if (PINB & (1<<SW_LEFT)) {
			ledOutput1 = 0x55;
		}
		else {
			ledOutput1 = 0xAA;
		}

		if (PINB & (1<<SW_RIGHT)) {
			ledOutput2 = 0x55;
		}
		else {
			ledOutput2 = 0xAA;
		}
*/

			if (slowLoop == slowLoopOverflow) {
				updateLEDs(ledOutput1[counter], ledOutput2[counter]);

				counter += 1;				
				if(counter == counterMax) {
					counter = 0;

				}
				slowLoop = 0;
			}
			slowLoop += 1;

			fastLoop = 0;
		}
		fastLoop += 1;
	
	}

}
