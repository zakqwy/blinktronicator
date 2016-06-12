/*
Blinktronicator

MIT License (MIT)
Copyright (c) 2015 Zach Fredin

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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

/* Global variables */
uint8_t faderCount = 0; //used for iterating through displayFader() [for PWM generation]
uint8_t valueLEDs[16] = {20,28,34,40,44,48,52,56,60,63,66,69,72,74,77,80}; //values from 0 to 255 for LEDs, L to R (starting with 630nm)

unsigned char currentLEDstate1 = 0;
unsigned char currentLEDstate2 = 0;

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


	for (i = 7; i >= 0; i--) { //first eight bits...
		if (ledOutput1 & (1<<i)) { //runs through the eight bits that make up ledOutput1
			PORTB |= (1<<PIN_DATA); //sets data pin if it should be high
		}


		PORTB |= (1<<PIN_CLOCK); //sets clock pin (after data pin!)
		PORTB &= ~((1<<PIN_DATA) + (1<<PIN_CLOCK)); //clears data pin regardless of its status. also clears clock pin.
	}

	for (i = 7; i >= 0; i--) {	
		if (ledOutput2 & (1<<i)) {
			PORTB |= (1<<PIN_DATA);
		}
		PORTB |= (1<<PIN_CLOCK);
		PORTB &= ~((1<<PIN_DATA) + (1<<PIN_CLOCK));
	}

	PORTB |= (1<<PIN_LATCH); //sets latch pin to update display
	PORTB &= ~(1<<PIN_LATCH);
}

void displayFader() {
// 8-bit PWM fader for LEDs
// uses global variable values to determine whether or not to turn various LEDs on or off

/* 	global varibales used
		faderCount (uint8_t)	
		valueLEDs[16] (uint8_t)
*/

	int counter; // which LED are we on? 

	currentLEDstate1 = 0;
	currentLEDstate2 = 0;

	for (counter = 0; counter <=7; counter++) { // cycle through both LED arrays

		if (valueLEDs[counter] > faderCount) {
			currentLEDstate1 |= (1<<counter);	
		}

		if (valueLEDs[counter + 8] > faderCount) {
			currentLEDstate2 |= (1<<counter);
		}

	}
	if (faderCount <= 100) {
		faderCount += 1;
	}
	else {
		faderCount = 0;
	}
}

int main(void) {
	SystemInit();

	long slowLoop = 0;
	long slowLoopOverflow = 500; //5000: speedy but visible


	int buttonLeft = 0;
	int buttonRight = 0;

	int counter = 0;

	for(;;) {

			if (slowLoop == slowLoopOverflow) {
			
			/*	demo routine to fade across LEDs	*/		
				for (counter = 0; counter <= 15; counter++) { //cycle through LEDs
					valueLEDs[counter] += 1;				
					if (valueLEDs[counter] > 20) {
						valueLEDs[counter] = 1;
					}
				}
			
				slowLoop = 0;
			}
		

			displayFader();
			updateLEDs(currentLEDstate1, currentLEDstate2);
			slowLoop += 1;
	
	}
}
