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
volatile uint8_t faderCount = 0; //used for iterating through displayFader() [for PWM generation]
volatile uint8_t valueLEDs[16] = {20,28,34,40,44,48,52,56,60,63,66,69,72,74,77,80}; //values from 0 to 255 for LEDs, L to R (starting with 630nm)

volatile unsigned char currentLEDstate1 = 0;
volatile unsigned char currentLEDstate2 = 0;
volatile uint8_t sw_left_history = 0;
volatile uint8_t sw_right_history = 0;

volatile uint8_t tick = 0;

ISR(TIMER0_COMPA_vect) {
/*	Makes ticks fire at regular times in the main loop */
	tick = 1;
}

void SystemInit(void) {
/* Port B configuration */
	DDRB |= ((1<<PIN_DATA) | (1<<PIN_CLOCK) | (1<<PIN_LATCH));//sets three I/O pins as outputs to send signals to 74HC595
	DDRB &= ~((1<<SW_LEFT) | (1<<SW_RIGHT));//sets two I/O pins as pushbutton inputs
	PORTB = 0;							

/* Timer/Counter0 configuration */
	TCCR0A |= (1<<WGM01); //CTC mode
	TCCR0B |= ((1<<CS00) | (1<<CS01)); //clk/64
	TIMSK |= (1<<OCIE0A); //enables Output Compare Match A ISR
	TCNT0 = 0;
	OCR0A = 15; //loop time = 8 * (OCR0A + 1) uS

/* misc */
	sei(); //enable global interrupts
}

uint8_t test_for_press_only(void) {
/*	Ultimate Debouncer by Elliot Williams, as discussed here:
	https://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/
	... modified slightly for this particular application. LSB is minus, LSB + 1 is plus. */
	uint8_t pressed = 0; //LSB is minus, LSB + 1 is plus
	sw_left_history = sw_left_history << 1; //shift button history left one
	sw_right_history = sw_right_history << 1;
	sw_left_history |= ((PINB & (1<<SW_LEFT)) == 0); //update to current button value
	sw_right_history |= ((PINB & (1<<SW_RIGHT)) == 0);
	if((sw_left_history & 0b11000111) == 0b00000111) { //check pattern
		pressed |= 0x01;
		sw_left_history = 0b11111111; //mask history
	}
	if((sw_right_history & 0b11000111) == 0b00000111) {
		pressed |= 0x02;
		sw_right_history = 0b11111111;
	}
	return pressed;
}

void updateLEDs(unsigned char ledOutput1, unsigned char ledOutput2) {
/* 	this could probably be more efficient. the second it started working, I saved the function and moved on.
	send it two unsigned characters, representing the LED state you currently want: ledOutput1 LSB is 630nm (red), while ledOutput2
	MSB is white. yeah, that seems a bit backwards, but it works with the + and - buttons. Note that this function is usually used as
	a subroutine for displayFader(), as that gives one the ability to actually dim the LEDs. */
	int i;
	PORTB &= ~(1<<PIN_LATCH); //starts by clearing latch pin	
	for (i = 0; i <= 7; i++) { //first eight bits...
		if (ledOutput1 & (1<<i)) { //runs through the eight bits that make up ledOutput1
			PORTB |= (1<<PIN_DATA); //sets data pin if it should be high
		}
		PORTB |= (1<<PIN_CLOCK); //sets clock pin (after data pin!)
		PORTB &= ~((1<<PIN_DATA) + (1<<PIN_CLOCK)); //clears data pin regardless of its status. also clears clock pin.
	}
	for (i = 0; i <= 7; i++) { // ...second eight bits	
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
/*	this function uses a global count variable (faderCount) to see where it is in the PWM cycle, then turns LEDs on or off depending on
	their value (0-100) in valueLEDs[16], where valueLEDs[0] is 630nm (red). */
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
	if (faderCount < 64) {
		faderCount += 1;
	}
	else {
		faderCount = 0;
	}
}

void clearLEDs(void) {
/*	this function is pretty self-explanatory */
	uint8_t counter;
	for (counter = 0; counter < 16; counter++) {
		valueLEDs[counter] = 0;
	}
	displayFader();
	updateLEDs(currentLEDstate1, currentLEDstate2);
}

int main(void) {
	SystemInit();
	clearLEDs();
	uint8_t currentLED = 0;	
	uint8_t buttonState = 0;
	uint8_t mode = 0; 
	int8_t direction = 0;
	for(;;) { 
	/* uses an idle function to absorb extra time--at OCR0A = 15, should run at 120 Hz (every 8.3 ms) */
		while(tick == 0) { } //idle
		tick = 0;
		buttonState = test_for_press_only();
	
		if (buttonState & 0x01) {
			valueLEDs[currentLED] = 0;
			if (currentLED == 0) {
				currentLED = 15;
			}
			else {
				currentLED--;
			}
			valueLEDs[currentLED] = 10;
		}
		if (buttonState & 0x02) {
			valueLEDs[currentLED] = 0;
			if (currentLED == 15) {
				currentLED = 0;
			}
			else {
				currentLED++;
			}
			valueLEDs[currentLED] = 10;
		}
		displayFader();
		updateLEDs(currentLEDstate1, currentLEDstate2);
	}
}
