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
volatile uint8_t valueLEDs[16]; //values from 0 to 255 for LEDs, L to R (starting with 630nm)

volatile unsigned char currentLEDstate1 = 0;
volatile unsigned char currentLEDstate2 = 0;

volatile uint8_t tick = 0;

ISR(TIMER0_COMPA_vect) {
/*	Makes ticks fire at regular times in the main loop */
	tick = 0;
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
//	sei(); //enable global interrupts
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
	their value (0-63) in valueLEDs[16], where valueLEDs[0] is 630nm (red). */
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

void updateButtonHistory(uint8_t *left_history, uint8_t *right_history) {
	*left_history = *left_history << 1;
	*left_history |= ((PINB & (1<<SW_LEFT)) == 0);
	*right_history = *right_history << 1;
	*right_history |= ((PINB & (1<<SW_RIGHT)) == 0);
}

uint8_t is_button_pressed(uint8_t *button_history) {
	uint8_t pressed = 0;
	if ((*button_history & 0b11000111) == 0b00000111) {
		pressed = 1;
		*button_history = 0b11111111;
	}
	return pressed;
}

uint8_t is_button_released(uint8_t *button_history) {
	uint8_t released = 0;
	if ((*button_history & 0b11000111) == 0b11000000) {
		released = 1;
		*button_history = 0b00000000;
	}
	return released;
}

uint8_t is_button_down(uint8_t *button_history) {
	return (*button_history == 0b00000000);
}

uint8_t is_button_up(uint8_t *button_history) {
	return (*button_history == 0b11111111);
}

void next_led(uint8_t *led) {
	if (*led == 15) {
		*led = 0;
	}
	else {
		*led = *led + 1;
	}
}

void prev_led(uint8_t *led) {
	if (*led == 0) {
		*led = 15;
	}
	else {
		*led = *led - 1;
	}
}

int main(void) {
	SystemInit();
	clearLEDs();
	uint8_t currentLED = 0;	
	uint8_t left_history = 0;
	uint8_t right_history = 0;
	uint8_t i = 0;
	uint8_t bouncer_update = 40; // n/8 ms
	uint8_t rollover_left_current = 0;
	uint8_t rollover_left_reset = 30;
	uint8_t rollover_right_current = 0;
	uint8_t rollover_right_reset = 30;
	uint8_t rollover_both_current = 0;
	uint8_t rollover_both_reset = 120;
	uint8_t mode = 0;
	uint8_t fade_delay = 0;
	uint8_t fade_delay_reset = 7;
	uint8_t j = 0;
	uint8_t k = 0;
	uint8_t brightness_array[] = {0,1,4,16,64,16,4,1};
	uint8_t next_mode_delay = 0;
	uint8_t next_mode_delay_reset = 25;	

	for(;;) { 
/*		while(tick == 0) { } //loop runs every 128 us
		tick = 1;
*/
		displayFader();
		updateLEDs(currentLEDstate1, currentLEDstate2);	
		if (i == bouncer_update) { //loop runs every bouncer_update/8 ms
			updateButtonHistory(&left_history, &right_history);
			if (mode == 0) {
				if (is_button_pressed(&left_history)) {
					valueLEDs[currentLED] = 0;
					prev_led(&currentLED);		
					valueLEDs[currentLED] = 16;
				}
				if (is_button_pressed(&right_history)) {
					valueLEDs[currentLED] = 0;
					next_led(&currentLED);
					valueLEDs[currentLED] = 16;
				}

				if (rollover_left_current >= rollover_left_reset) {
					valueLEDs[currentLED] = 0;
					prev_led(&currentLED);
					valueLEDs[currentLED] = 16;
					rollover_left_current = 0;
				}

				if (rollover_right_current >= rollover_right_reset) {
					valueLEDs[currentLED] = 0;
					next_led(&currentLED);
					valueLEDs[currentLED] = 16;
					rollover_right_current = 0;
				}

			}

			if (mode == 1) {
				if (fade_delay >= fade_delay_reset) {
					for (j=0;j<8;j++) {
						valueLEDs[currentLED] = brightness_array[j];
						next_led(&currentLED);
					}
					for (j=0;j<7;j++) {
						prev_led(&currentLED);
					}
					fade_delay = 0;
				}
				fade_delay++;
				
				if ((is_button_pressed(&right_history)) & (fade_delay_reset > 0)) {
					fade_delay_reset--;
				}
				if ((is_button_pressed(&left_history)) & (fade_delay_reset < 255)) {
					fade_delay_reset++;
				}

				if ((rollover_right_current >= rollover_right_reset) & (fade_delay_reset > 0)) {
					fade_delay_reset--;
					rollover_right_current = 0;
				}
				if ((rollover_left_current >= rollover_left_reset) & (fade_delay_reset < 255)) {
					fade_delay_reset++;
					rollover_left_current = 0;
				}

			}
	
			if (mode == 2) {

				if (is_button_pressed(&left_history)) {
					switch(valueLEDs[currentLED]) {
						case 0:
							valueLEDs[currentLED] = 1;
							break;
						case 1: 
							valueLEDs[currentLED] = 4;
							break;
						case 3:
							valueLEDs[currentLED] = 16;
							break;
						case 4:
							valueLEDs[currentLED] = 16;
							break;
						case 16: 
							valueLEDs[currentLED] = 64;
							break;
						default: 
							valueLEDs[currentLED] = 0;
					}
				}
				if (rollover_left_current >= rollover_left_reset) {
					switch(valueLEDs[currentLED]) {
						case 0:
							valueLEDs[currentLED] = 1;
							break;
						case 1: 
							valueLEDs[currentLED] = 4;
							break;
						case 3:
							valueLEDs[currentLED] = 16;
							break;
						case 4:
							valueLEDs[currentLED] = 16;
							break;
						case 16: 
							valueLEDs[currentLED] = 64;
							break;
						default: 
							valueLEDs[currentLED] = 0;
					}
					rollover_left_current = 0;
				}
				if (is_button_pressed(&right_history)) {
					if (valueLEDs[currentLED] == 3) {
						valueLEDs[currentLED] = 0;
					}
					next_led(&currentLED);
					if (valueLEDs[currentLED] == 0) {
						valueLEDs[currentLED] = 3;
					}
				}
	
				if (rollover_right_current >= rollover_right_reset) {
					if (valueLEDs[currentLED] == 3) {
						valueLEDs[currentLED] = 0;
					}
					next_led(&currentLED);
					if (valueLEDs[currentLED] == 0) {
						valueLEDs[currentLED] = 3;
					}
					rollover_right_current = 0;
				}
	
			}

			if (mode == 3) {

// jolly wrencher
				uint8_t display_array_length = 30;
				uint8_t display_array[30][2] = {
					{0b01000000,0b00000100},
					{0b01100000,0b00000101},
					{0b11110000,0b00001110},
					{0b00111000,0b00011100},
					{0b00011100,0b00111000},
					{0b00001011,0b11010000},
					{0b00000110,0b01100000},
					{0b00011111,0b01110000},
					{0b00001101,0b11110000},
					{0b00001101,0b11110000},
					{0b00011111,0b01110000},
					{0b00000110,0b01100000},
					{0b00001011,0b11010000},
					{0b00011100,0b00111000},
					{0b00111000,0b00011100},
					{0b11110000,0b00001110},
					{0b01100000,0b00000101},
					{0b01000000,0b00000100},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0},
					{0,0}
				};
				bouncer_update = 1;
				for (j=0;j<8;j++) {
					if ((0x80>>j) & (display_array[k][0])) {
						valueLEDs[j] = 64;
					}
					else {
						valueLEDs[j] = 0;
					}
					if ((0x80>>j) & (display_array[k][1])) {
						valueLEDs[j + 8] = 64;
					}
					else {
						valueLEDs[j + 8] = 0;
					}
				}
				k++;
				if (k > display_array_length) {
					k = 0;
				}
			}

			if ((is_button_down(&left_history)) & (is_button_down(&right_history))) {
				rollover_both_current++;
			}			

			if (rollover_both_current >= rollover_both_reset) {
				if ((mode < 3) || (next_mode_delay == next_mode_delay_reset)) {
					mode++;
					next_mode_delay = 0;
				}
				else {
					next_mode_delay++;
				}
				
				if (mode == 2) {
					clearLEDs();
				}
		
				if (mode > 3) {
					clearLEDs();
					bouncer_update = 40;
					valueLEDs[currentLED] = 31;
					mode = 0;
				}
				rollover_both_current = 0;
			}

			if (is_button_down(&right_history)) {
				rollover_right_current++;
			}

			if (is_button_down(&left_history)) {
				rollover_left_current++;
			}

			if (is_button_up(&left_history)) {
				rollover_left_current = 0;
				rollover_both_current = 0;
			}
			if (is_button_up(&right_history)) {
				rollover_right_current = 0;
				rollover_both_current = 0;
			}
	
			i = 0;
		}
		i++;
	}
}
