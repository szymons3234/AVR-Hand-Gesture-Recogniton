#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>
#include "lcd_displ.h"
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

// definicja pinów klawiatury 4x4
#define ROWS 4
#define COLS 4

// piny klawiatury
#define ROW1 PD7
#define ROW2 PD6
#define ROW3 PD5
#define ROW4 PD4

#define COL1 PC0
#define COL2 PC1
#define COL3 PC2
#define COL4 PC3

#define LED_CZER (1<<PB0)
#define LED_ZIEL (1<<PB2)
#define PIN_SIZE 4

char get_keypad_key(void);
void init_ports();
void set_servo_angle(uint8_t angle);
void init_servo();
void uart_init(unsigned int ubrr);
void uart_transmit(char data);
unsigned char uart_receive(void);
void pwm_timer2_init();
void set_timer2_frequency(uint16_t frequency);
void beep();


int main(void) {
	lcd_init();
	init_ports();
	uart_init(MYUBRR);
	DDRB |= LED_ZIEL;
	DDRB |= LED_CZER;

	char key;
	char pin[PIN_SIZE + 1] = "";   // domyœlny PIN
	char correct_pin[] = "1234";    // poprawny PIN

	lcd_clear();
	lcd_gotoxy(0, 0);
	lcd_swrite("PIN:");
	init_servo();
	pwm_timer2_init();

	while (1) {
		key = get_keypad_key();  // pobierz klawisz
		PORTB |= LED_CZER;

		if (key) {
			if (key == '#') {
				if (strcmp(pin, correct_pin) == 0) {
					// PIN poprawny
					lcd_clear();
					lcd_gotoxy(0, 0);
					lcd_swrite("PIN POPRAWNY!");
					_delay_ms(1000);
					lcd_clear();
					lcd_gotoxy(0, 0);
					lcd_swrite("Wykonaj gest!");
					_delay_ms(500); // poczekaj na odpowiedŸ z Python
					char response = uart_receive(); // odbierz odpowiedŸ
					if (response == '1') {
						PORTB &= ~LED_CZER;
						PORTB |= LED_ZIEL;
						lcd_clear();
						lcd_gotoxy(0, 0);
						lcd_swrite("Zamek otwarty!");
						set_servo_angle(90);
						set_timer2_frequency(40);
						_delay_ms(2000);
						set_timer2_frequency(0);
						_delay_ms(6000);
						set_servo_angle(0);
						PORTB &= ~LED_ZIEL;
						PORTB |= LED_CZER;
					}
					} else {
					// PIN b³êdny
					lcd_clear();
					lcd_gotoxy(0, 1);
					lcd_swrite("BLEDNY PIN!");
					set_timer2_frequency(800);
					_delay_ms(2000);
					set_timer2_frequency(0);
					
					for (int i = 0; i < 20; i++) {
						PORTB &= ~LED_CZER;
						_delay_ms(100);
						PORTB |= LED_CZER;	
					}
				}
				memset(pin, '\0', sizeof(pin));
				lcd_clear();
				lcd_gotoxy(0, 0);
				lcd_swrite("PIN:");
				} else if (key == '*') {
				// usuwanie ost znaku PIN
				int len = strlen(pin);
				if (len > 0) {
					pin[len - 1] = '\0';  // usun last znak
				}
				lcd_clear();
				lcd_gotoxy(0, 0);
				lcd_swrite("PIN:");
				lcd_gotoxy(5, 0);
				lcd_swrite(pin);  // wyswietl aktualny PIN
				} else {
				// dodawanie znaków do PIN-u (sprawdzenie d³ugoœci bo by³ problem)
				if (strlen(pin) < PIN_SIZE) {
					size_t len = strlen(pin);
					pin[len] = key;
					pin[len + 1] = '\0'; //koniec ci¹gu 
					lcd_gotoxy(5 + len, 0); 
					lcd_printf("%c", key); 
					beep();
				}
			}
		}
	}
}



void init_ports() {
	// col jako wyjscie
	DDRC |= (1 << COL1 ) | (1 << COL2) | (1 << COL3) | (1 << COL4);
	
	// row jako wejscie z pullup
	DDRD &= ~((1 << ROW1) | (1 << ROW2) | (1 << ROW3) | (1 << ROW4));
	PORTD |= (1 << ROW1) | (1 << ROW2) | (1 << ROW3) | (1 << ROW4);  // Pull-up w³
}

char get_keypad_key() {
	char keys[4][4] = {
		{'1', '2', '3', 'A'},
		{'4', '5', '6', 'B'},
		{'7', '8', '9', 'C'},
		{'*', '0', '#', 'D'}
	};

	char row_pins[] = {ROW1, ROW2, ROW3, ROW4};
	char col_pins[] = {COL1, COL2, COL3, COL4};
	char key = 0;

	for (uint8_t col = 0; col < 4; col++) {
		// usttaw wszystkie kolumny w stan wysoki (nieaktywne)
		PORTC |= (1 << COL1) | (1 << COL2) | (1 << COL3) | (1 << COL4);

		// aktywuj bie¿¹ca kolumnê (ustaw niskie)
		PORTC &= ~(1 << col_pins[col]);
		_delay_us(50); // ustabilizowanie sygna³u

		for (uint8_t row = 0; row < 4; row++) {
			if (!(PIND & (1 << row_pins[row]))) { // jesli przycisk przycisniety
				_delay_ms(50); // eliminacja drgañ styków
				if (!(PIND & (1 << row_pins[row]))) { // potwierdzenie (w razie czego)
					key = keys[row][col]; // pobierz znak z tablicy
					while (!(PIND & (1 << row_pins[row]))); // czekaj na zwolnienie
					return key;
				}
			}
		}

		// deaktywuj bie¿¹c¹ col
		PORTC |= (1 << col_pins[col]);
	}

	return 0; // brak wciœniêcia
}

void init_servo() {
	DDRB |= (1 << PB1); // ustawienie PB1 jako wyjœcie dla serwa
	TCCR1A |= (1 << COM1A1) //clear OC1A/OC1B on compare match (zobacze opadaj¹ce)
	TCCR1A |= (1 << WGM11);  // tryb Fast PWM
	TCCR1B |= (1 << WGM13) | (1 << WGM12) | // Tryb Fast PWM
	TCCR1B |= (1 << CS11);  //preskaler 8
	ICR1 = 19999;  // Czêstotliwoœæ PWM 50 Hz
}

void set_servo_angle(uint8_t angle) {
	// convert syganlu PWM dla serwa (0°-180°)
	uint16_t pulse_width = (angle * 11) + 500; // range 500-2500 µs
	OCR1A = pulse_width;
}

void uart_init(unsigned int ubrr) {
	// predkosc tranmisji
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	
	// odbiornika i nadajnika w³
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	
	// Tryb 8 bitów danych, brak parzystoœci, 1 bit stopu
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // 8 bitów danych
	UCSR0C &= ~((1 << UPM01) | (1 << UPM00)); // Brak parzystoœci
	UCSR0C &= ~(1 << USBS0);                 // 1 bit stopu
}
void uart_transmit(char data) {
	while (!(UCSR0A & (1 << UDRE0)));//bufor nadawczy pusty
	UDR0 = data; // Wyœlij dane
}

unsigned char uart_receive(void) {
	
	while (!(UCSR0A & (1 << RXC0)));// czekanie na odebranie danych
	return UDR0; // odczyt danych z bufora
}
void pwm_timer2_init() {
	
	DDRB |= (1 << PB3); // PB3 - wyjscie buzzera
	TCCR2A |= (1 << WGM20) | (1 << WGM21);  // Fast PWM
	TCCR2A |= (1 << WGM22);					// Fast PWM
	TCCR2A |= (1 << COM2A1);                // Clear OC2A on compare match (opdadajce)
	TCCR2B |= (1 << CS21);                  // preskaler 8
}

void set_timer2_frequency(uint16_t frequency) {
	if (frequency == 0) {
		OCR2A = 0;  // Wy³¹cz PWM
		} else {
		uint16_t ocr_value = (F_CPU / (8 * frequency)) - 1;
		if (ocr_value > 255) {
			ocr_value = 255;  // Ograniczenie dla 8-bitowego rejestru
		}
		OCR2A = ocr_value;
	}
}
void beep() {
	set_timer2_frequency(1400); //  dŸwiêk freq 1,4kHz
	_delay_ms(100);             
	set_timer2_frequency(0);    // dŸwiek off
}