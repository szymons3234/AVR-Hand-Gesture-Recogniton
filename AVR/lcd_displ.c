// 6 kabelk�w
// LCD_D4,LCD_D5,LCD_D6,LCD_D7  magistrala 4 bity
// EN -  enable signal
// RS -  register select (dane albo rozkazy)
//       RW - zapis/odczyt u nas tylko zapis = 0

//  dzia�a prawid�owo dla impuls�w skr�conych:  0.35 T (graniczne),    wyd�uzonych wi�cej ni� 10 x T,      T - d�ugo�� standardowa

#define F_CPU 11059200

#include <avr/io.h>
#include <util/delay.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>



#include "lcd_displ.h"

// procedura wysyla 4 bity na wyswietlacz
void lcd_send_4(unsigned char b) {
	set_lcd_en;

	// Wysterowanie poszczeg�lnych linii danych
	if (b & 1) set_lcd_d4; else clr_lcd_d4;
	if (b & 2) set_lcd_d5; else clr_lcd_d5;
	if (b & 4) set_lcd_d6; else clr_lcd_d6;
	if (b & 8) set_lcd_d7; else clr_lcd_d7;

	_delay_us(200);   // Zwi�kszenie op�nienia na ustabilizowanie sygna�u

	// zapis danych opadaj�cym zboczem EN
	clr_lcd_en;
	_delay_us(100);   // dodatkowe op�nienie dla wi�kszej stabilno�ci
}

void lcd_send_8(unsigned char b)
{
	lcd_send_4(b >> 4);   // najpierw cz�� bardziej znacz�ca
	_delay_us(100);       // dodaj op�nienie dla wi�kszej stabilno�ci
	lcd_send_4(b);
	_delay_us(100);       // dodaj op�nienie dla wi�kszej stabilno�ci
}

void lcd_send_com(unsigned char b)
{
	// rs = 0  wysy�anie rozkazu
	clr_lcd_rs;
	lcd_send_8(b);
	_delay_us(100);
}



void lcd_send_data(unsigned char b)
{
	// rs = 1  wysy�anie danej (do pami�ci)
	set_lcd_rs; // SET(PORT,LCD_RS);
	lcd_send_8(b);
	_delay_us(100);
}



void lcd_init(void) {
	// Kierunek port�w = 1 (wyj�cie)
	ddr_d4; ddr_d5; ddr_d6; ddr_d7; ddr_en; ddr_rs;

	_delay_ms(10);           // D�u�sze op�nienie na stabilizacj� LCD (10 ms)
	lcd_send_4(3); _delay_ms(1); lcd_send_4(3); _delay_ms(1);  // Prze��czanie na magistral� 4-bit
	lcd_send_4(3); _delay_ms(1); lcd_send_4(2); _delay_ms(1);  // Wy�lij 4-bitow� konfiguracj�
	lcd_send_com(0x28); _delay_ms(1);   // Konfiguracja 4-bit, 2 linie, 5x8 czcionka
	lcd_send_com(0x01); _delay_ms(2);   // Wyczy�� ekran
	lcd_send_com(0x06); _delay_us(100); // Inkrementacja, bez przesuni�cia
	lcd_send_com(0x0C); _delay_us(100); // W��cz, bez kursora i migania
}
void lcd_control( unsigned char on, unsigned char cur, unsigned char blink)
{
	unsigned char v=0;
	v = 1<<3 |     (on!=0)<<2 |  (cur!=0)<<1 | (blink!=0);
	lcd_send_com( v );
}

void lcd_clear(void)
{
	lcd_send_com(1);
	lcd_send_com(2);
}

void lcd_home(void)
{
	lcd_send_com(2);
	_delay_ms(2);
}

void lcd_gotoxy(unsigned char x, unsigned char y)
{
	unsigned char address = 0x80;  // Domy�lny adres DDRAM dla pierwszej linii
	if (y == 1) {
		address |= 0x40;  // Je�li druga linia, dodaj przesuni�cie 0x40
	}
	lcd_send_com(address | x);   // Ustawienie adresu DDRAM z przesuni�ciem poziomu
}
void lcd_swrite(char *s) {
	int i = 0;
	while (s[i] && i < 40) {  // Zwi�kszenie bufora do 40 znak�w
		lcd_send_data(s[i]);
		++i;
	}
}


void lcd_iwrite(int i)
{
	char s[10];
	itoa(  i, s, 10); lcd_swrite(s) ;
}

int lcd_printf(char *format, ...) {
	va_list argptr;
	char str[40];  // Wi�kszy bufor dla tekstu
	int n;

	va_start(argptr, format);
	n = vsprintf(str, format, argptr);
	va_end(argptr);

	lcd_swrite(str);  // Wy�lij tekst na LCD
	return n;
}