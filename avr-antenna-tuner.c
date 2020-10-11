/*****************************************************************/
/*              Antennatuner with ATMega 16 V2                   */
/*  ************************************************************ */
/*  Microcontroller:  ATMEL AVR ATmega16, 8 MHz                  */
/*                                                               */
/*  Compiler:         GCC (GNU AVR C-Compiler)                   */
/*  Author:           Peter Rachow DK7IH                         */
/*  Last cahnge:      2020-10-09                                 */
/*****************************************************************/

/*    PORTS */
// O U T P U T 
// LCD 
// RS      = PD2
// E       = PD3
// D4...D7 = PD4..PD7

//Coil relays: PC0...PC6

//Motor direction relay: PD0
//Motor drive on/off: PD1
//Extra capacitor 200pF: PC7 (not yet!)

//I N P U T
//ADC0: SWR-Meter 0
//ADC1: SWR-Meter 1
//ADC2:
//ADC3: Keys
//ADC4: Potentiometer for Capacitor position

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <util/delay.h>

int main(void);

#define LCD_INST 0x00
#define LCD_DATA 0x01

#define MAXCAP 220
#define CAPDELAY 10
#define MAXBANDS 7

void lcd_write(char, unsigned char);
void set_rs(char);
void set_e(char);
void lcd_init(void);
void lcd_cls(void);
void lcd_line_cls(int);
void lcd_putchar(int, int, unsigned char);
void lcd_putstring(int, int, char*);
int lcd_putnumber(int, int, long, int);
void lcd_display_test(void);

//BAND DISPLAY & MISC
void show_band(int);
void show_meter(int, int, int);
void define_chars(void);
void show_inductance(int);
void show_xcoil(int);

//Coils & Cpas
int set_coils(int);
void set_cap(int);
void rotate_cap(int);
int get_cap(void);

//ADC
int get_adc(int);
int get_keys(void);

//Delay
void wait_ms(int);

//String
int int2str(long, int, char *, int);
int stringlen(char *);


long runsecs = 0;
int xcoil = 0;

/**************************************/
/* Funktionen und Prozeduren fuer LCD */
/**************************************/
// LCD 
// RS      = PD2
// E       = PD3
// D4...D7 = PD4..PD7

/* Ein Byte (Befehl bzw. Zeichen) zum Display senden */
void lcd_write(char lcdmode, unsigned char value)
{
    int x = 16, t1;
	
    set_e(0); 

    if(!lcdmode)
	{
        set_rs(0);    //RS=0 => INST
	}	
    else
	{
        set_rs(1);    // RS=1 => DATA
	}	

    wait_ms(4);
	
    //Hi nibble
    set_e(1);
	for(t1 = 0; t1 < 4; t1++)
	{
	    if(value & x)
	    {
	       PORTD |= x;
	    }
        else	
	    {
           PORTD &= ~(x);
	    }  
		
		x <<= 1;
	}	
	set_e(0);
	
	x = 16;

	// Lo nibble
	set_e(1);
	for(t1 = 0; t1 < 4; t1++)
	{
	    if((value & 0x0F) * 16 & x)
	    {
	       PORTD |= x;
	    }
        else	
	    {
           PORTD &= ~(x);
	    }  
		
		x <<= 1;
	}
    set_e(0);

}

//RS
void set_rs(char status) //PD2  
{
    if(status)
	{
        PORTD |= 4;
	}	
    else
	{
	    PORTD &= ~(4);
	}	
}

//E
void set_e(char status)  //PD3
{
    if(status)
	{
        PORTD |= 8;
	}	
    else
	{
	    PORTD &= ~(8);
	}	
}

/* Ein Zeichen (Char) zum Display senden, dieses in */
/* Zeile row und Spalte col positionieren           */
void lcd_putchar(int row, int col, unsigned char ch)
{
    lcd_write(LCD_INST, col + 128 + row * 0x40);
    lcd_write(LCD_DATA, ch);
}


/* Eine Zeichenkette direkt in das LCD schreiben */
/* Parameter: Startposition, Zeile und Pointer   */
void lcd_putstring(int row, int col, char *s)
{
    unsigned char t1;

    for(t1 = col; *(s); t1++)
	{
        lcd_putchar(row, t1, *(s++));
	}	
}


/* Display loeschen */
void lcd_cls(void)
{
    lcd_write(LCD_INST, 1);
}


/* LCD-Display initialisieren */
void lcd_init(void)
{
    /* Grundeinstellungen: 2 Zeilen, 5x7 Matrix, 4 Bit */
    lcd_write(LCD_INST, 40);
    lcd_write(LCD_INST, 40);
    lcd_write(LCD_INST, 40);

    //MAtrix 5*7
    lcd_write(LCD_INST, 8);

    /* Display on, Cursor off, Blink off */
    lcd_write(LCD_INST, 12);

    /* Entrymode !cursoincrease + !displayshifted */
    lcd_write(LCD_INST, 4);
	
	//4-Bit-Mode
    lcd_write(LCD_INST, 2);	
	
	lcd_cls();
}


//Write number with given amount on digits to LCD
int lcd_putnumber(int col, int row, long num, int dec)
{
    char *numstr = malloc(32);
    int l = 0;
    if(numstr != NULL)
    {
        int2str(num, dec, numstr, 16);
        lcd_putstring(col, row, numstr);
        l = stringlen(numstr);
        free(numstr);
        return l;
      
    } 
    return 0;
}


void lcd_line_cls(int ln)
{
    int t1;
	
	for(t1 = 0; t1 < 16; t1++)
	{
	    lcd_putchar(ln, t1, 32);
	}
}	
/*****************************************/
//           STRING FUNCTIONS
/*****************************************/
//INT 2 ASC
int int2str(long num, int dec, char *buf, int buflen)
{
    int i, c, xp = 0, neg = 0;
    long n, dd = 1E09;

    if(!num)
	{
	    *buf++ = '0';
		*buf = 0;
		return 1;
	}	
		
    if(num < 0)
    {
     	neg = 1;
	    n = num * -1;
    }
    else
    {
	    n = num;
    }

    //Fill buffer with \0
    for(i = 0; i < 12; i++)
    {
	    *(buf + i) = 0;
    }

    c = 9; //Max. number of displayable digits
    while(dd)
    {
	    i = n / dd;
	    n = n - i * dd;
	
	    *(buf + 9 - c + xp) = i + 48;
	    dd /= 10;
	    if(c == dec && dec)
	    {
	        *(buf + 9 - c + ++xp) = '.';
	    }
	    c--;
    }

    //Search for 1st char different from '0'
    i = 0;
    while(*(buf + i) == 48)
    {
	    *(buf + i++) = 32;
    }

    //Add minus-sign if neccessary
    if(neg)
    {
	    *(buf + --i) = '-';
    }

    //Eleminate leading spaces
    c = 0;
    while(*(buf + i))
    {
	    *(buf + c++) = *(buf + i++);
    }
    *(buf + c) = 0;
	
	return c;
}

//STRLEN
int stringlen(char *s)
{
   int t1 = 0;

   while(*(s + t1++));

   return (t1 - 1);
}

//IUnductance display
void show_inductance(int i)
{
	int i1;
	
	i1 = i + xcoil * 400;
	lcd_putstring(0, 0, "       ");
    lcd_putstring(0, lcd_putnumber(0, 0, i1, 2), "uH");
}

void show_xcoil(int status)
{
    if(status)
    {
	   lcd_putstring( 1, 14, "+L");
    }	   
    else
    {
		lcd_putstring( 1, 14, "--");
	}
}		

//BAND DISPLAY
void show_band(int b)
{

    char *band_str[] = {"80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m"};
    lcd_putstring(0, 13, band_str[b]);
}

//Meter (max. value = 25)
void show_meter(int value, int value_old, int pos)
{
	#define MAXBLOCKS 4
    int v1, v2, v3, i1;
	
    //Clear meter (5 chars) if new value > old value
    if(value < value_old)
    {
        for(i1 = 0; i1 < MAXBLOCKS; i1++)
        {
	        lcd_putchar(1, i1 + pos * 6, 32);
        }
	}
			
	v1 = (int) value / MAXBLOCKS; //Full blocks, 5 cols each
    v2 = value - v1 * MAXBLOCKS;  //Rest
	if(v1 > MAXBLOCKS)
	{
		v1 = MAXBLOCKS;
	}
	
	if(value >= value_old)
	{
	    v3 = (int) value_old / MAXBLOCKS; //Full blocks, 5 cols each, already drawn
	}
	else
	{  
		v3 = 0;
	}	    
	
	//Full blocks	
	for(i1 = v3; i1 < v1; i1++)
	{
	    lcd_putchar(1, i1 + pos * 6, 4); 
	}
	
	//Rest
	if(i1 < MAXBLOCKS)
	{
		if(v2)
	    {
	        lcd_putchar(1, i1 + pos * 6, v2 - 1);
	    }
        else
        {
            lcd_putchar(1, i1 + pos * 6, ' ');
        }
	}
}

//PROGRAM CUNSTOM CHARS FOR S-SMETER
void define_chars(void)
{
    int i1;
	unsigned char adr = 64;
						  
	unsigned char b1[] = {0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, //S-Meter blocks 
	                      0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
						  0x00, 0x00, 0x1C, 0x1C, 0x1C, 0x1C, 0x00, 0x00,
						  0x00, 0x00, 0x1E, 0x1E, 0x1E, 0x1E, 0x00, 0x00,
						  0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00,
						  	0x0,0x0,0x4,0xe,0x4,0x0,0x0, 0, //+
						  	0x4,0xe,0x4,0x0,0x4,0xe,0x4,0, //++
						  0x00, 0x01, 0x02, 0x12, 0x0A, 0x04, 0x00, 0x00  //Tick
						  
						  };					  
	//Dummy operation!
	lcd_write(LCD_INST, 0);
	lcd_write(LCD_DATA, 0);

    //Load data into CGRAM
	for(i1 = 0; i1 < 64; i1++)  
	{
	    lcd_write(LCD_INST, adr++);
		lcd_write(LCD_DATA, b1[i1]);
	}
}		


/////////////////////////////////////
//
// C O I L S & C A P S
//
/////////////////////////////////////
int set_coils(int pattern)
{
    int unsigned t1, p = pattern;
    
    //Inductance in uH  * 100
    int i0[] = {5, 10, 25, 50, 100, 200, 400}, i1 = 0;
    
    //lcd_putstring(0, 10, "    ");
    //lcd_putnumber(0, 10, p, -1, -1, 'l', 0);
    
    for(t1 = 0; t1 < 7; t1++)
    {
		if(p & (1 << t1))
        {
            PORTC &= ~(1 << t1);      
            i1 += i0[t1];
        }
        else
        {    
            PORTC |= (1 <<  t1);
        }
    }       
          
   return i1;
}

void set_cap(int cap)
{
               
    if(cap < 0 ||cap > MAXCAP)
    {
		return;
	}
	
	lcd_putstring(0, lcd_putnumber(0, 7, cap, -1) + 7, "pF");
	wait_ms(500);
	/*	
    lcd_putnumber(0, 0, get_adc(4), -1, -1, 'l', 0);
    lcd_putnumber(0, 8, cap, -1, -1, 'l', 0);
    lcd_putnumber(1, 0, (int) adc0, -1, -1, 'l', 0);	
    */       
    
    while(get_cap() > cap)
    {
		rotate_cap(0);
		lcd_putstring(0, 7, "      ");
        lcd_putstring(0, lcd_putnumber(0, 7, get_cap(), -1) + 7, "pF");
        if(get_keys() == 5 || get_keys() == 6) //Quit if band QSY key is pressed
        {
			return;
		}	
    }
    
    while(get_cap() < cap)
    {
		rotate_cap(1);
		lcd_putstring(0, 7, "      ");
        lcd_putstring(0, lcd_putnumber(0, 7, get_cap(), -1) + 7, "pF");
        if(get_keys() == 5 || get_keys() == 6) //Quit if band QSY key is pressed
        {
			return;
		}	
    }
}

//Measure real cap val from sensor
int get_cap(void)
{
	double val = (double) (get_adc(4) - 384) * MAXCAP / 256;
	
	return (int) val;
}
	
void rotate_cap(int direction)
{
	if(direction)
	{
		PORTD &= ~(1); //RelayOFF      
	}
	else
	{
	    PORTD |= 1;   //Relay ON	
	}	
	
	PORTD |= 2;   //Motor ON
	wait_ms(CAPDELAY);
	PORTD &= ~(2);  //Motor OFF      
	wait_ms(CAPDELAY);
}	


ISR(TIMER1_OVF_vect)   	   // Timer1 Überlauf
{ 
    runsecs++;
    TCNT1 = 57724;	  
} 

//***************************************************
//                      ADC
//***************************************************

int get_adc(int adcmode)
{
    int adc_val = 0;
	
	
	ADMUX = (ADMUX &~(0x1F)) | (adcmode & 0x1F);     // Kanal adcmode aktivieren PA0=TUNE
    wait_ms(3);
	
    ADCSRA |= (1<<ADSC);
	wait_ms(3);
	
	adc_val = ADCL;
    adc_val += ADCH * 256;   
	
	while(ADCSRA & (1<<ADSC));
	
	return adc_val;
}	

//Read keys via ADC0
int get_keys(void)
{

    int key_value[] = {18, 22, 29, 43, 74, 132, 235, 396, 497};  
       	
    int t1;
    int adcval = get_adc(3);
        
    //TEST display of ADC value 
    /*
    lcd_cls();
    lcd_putnumber(0, 0, get_adc(3), -1);
    return 0;    	
    */
    
    for(t1 = 0; t1 < 9; t1++)
    {
        if(adcval > key_value[t1] - 2 && adcval < key_value[t1] + 2)
        {
			 return t1 + 1;     
        }
    }
    
    return 0;
}

/***************************************************/
/* Wartezeit in Millisekunden bei fck = 8.000 MHz  */
/***************************************************/
void wait_ms(int ms)
{
  int t1, t2;

  for(t1 = 0; t1 < ms; t1++)
    for(t2 = 0; t2 < 137 * 8; t2++)
      asm volatile ("nop" ::);
}

int main()
{
	int t1;
	int key;     //Keystroke
	int adc_val; //Value of ADC
    int i = 65;  //L
    int band = 0;
    int c1 = 0;
    int inductance;
    
    //Standard values for DK7IH antenna
    //L=      - (128), 8uH(64), 4uH(32), 2uH(16), 1uH(8), 0.5uH(4), 0.25uH(2), 0.125uH(1)
    int std_l [] = {65, 33, 12, 8, 6, 4, 3, 2, 1};
    int std_c [] = {172, 41, 75, 110, 88, 55, 33, 17};
    
    //Meter data
    long s0 = 0, s1 = 0;
    
    double swr0;
    int swr1;
    int loopcnt0 = 0;;
       
    
	/* Set ports */
    /* OUTPUT */
    DDRB = 0x1F; //Relays 1.55 of cap switching
    DDRC = 0x7F; //Relays for coils             
    DDRD = 0xFF; //LCD data on PD4...PD7
                 //LCD RS:PD2, E:PD3
                 //PD0, PD1: Relay 6 and 7 of cap switches
    
    PORTA = 0x08; //Pullup resistor for keys' input   
    
    //Display
    lcd_init();
	wait_ms(500);
	lcd_cls();
	lcd_putstring(0, 0, " DK7IH Antenna");				
	lcd_putstring(1, 0, " Tuner Ver. 2.0");				
	wait_ms(500);
	lcd_cls();
			
	//Watchdog abschalten
	WDTCR = 0;
	WDTCR = 0;
    	
	//ADC initialisieren
    ADMUX = (1<<REFS0);     // Referenz = AVCC
    ADCSRA = (1<<ADPS2) | (1<<ADPS1) | (1<<ADEN); //Frequenzvorteiler 64 u. //ADC einschalten
	ADCSRA |= (1<<ADSC); //Eine Wandlung vornehmen
	while (ADCSRA & (1<<ADSC)); //Eine Wandlung abwarten
    adc_val = ADCL;
    adc_val += ADCH * 256;   //Wert auslesen
	adc_val = 0;
	
	//Timer 1
	TCCR1A = 0;                      // normal mode, keine PWM Ausgänge
    TCCR1B = (1<<CS12) + (1<<CS10) ;   // start Timer mit Systemtakt, Prescaler = /1024
	                                    //Auslösung des Overflow alle Sekunde sec.
    TIMSK = (1<<TOIE1);   // overflow aktivieren.
	TCNT1 = 57724;        //Startwert für Sekundentakt

    //Define custom chars for meter
    //define_chars();

    //Load standard data if eeprom cell empty
    for(t1 = 0; t1 < 8; t1++)
    {
		if(eeprom_read_byte((uint8_t*)(t1 * 2)) == 255)
		{ 
			eeprom_write_byte((uint8_t*)(t1 * 2), std_l[t1]);
			eeprom_write_byte((uint8_t*)(t1 * 2 + 1), std_c[t1]);
		}
	}		

    //Get recent data           		
    //Band
   	band = eeprom_read_byte((uint8_t*)32);
   	if(band < 0 || band > MAXBANDS)
	{
	    band = 3;
	}
	show_band(band);

    //Inductance	
	i = eeprom_read_byte((uint8_t*)(band * 2));
	if(i & 128)
	{
		xcoil = 1;
	}	
	else
	{
		xcoil = 0;
	}	
	show_xcoil(xcoil);
	
	i &= 0x7F;
	
	if(i >= 0 && i < 128)
	{
		inductance = set_coils(i);
    	show_inductance(inductance);	
	}
	else
	{
		set_coils(65);
		lcd_line_cls(1);
		lcd_putstring(0, 0, " -x-uH");
		wait_ms(1000);
	}
	
	//CAP
	c1 = eeprom_read_byte((uint8_t*)(band * 2) + 1);	
	
	if(c1 >= 0 && c1 < MAXCAP)
	{
		set_cap(c1);
	}
	else
	{
		set_cap(110);
		lcd_line_cls(1);
		lcd_putstring(0, 0, " -x-pF");
		wait_ms(1000);
	}	
    sei();

    show_band(band);
    
    for(;;) 
	{
		key = get_keys();
		if(i > 0 && key == 2)
		{
			i--;
		    show_inductance(set_coils(i));
		    wait_ms(2);
		}    
		
		if(i < 127 && key == 1)
		{
			i++;
		    show_inductance(set_coils(i));
		    wait_ms(2);
		}    
				
		while(get_cap() > 0 && key == 4) //C(-)
		{
			rotate_cap(0);
		    lcd_putstring(0, 7, "      ");
            lcd_putstring(0, lcd_putnumber(0, 7, get_cap(), -1) + 7, "pF");     
            wait_ms(100);
            key = get_keys();
		}    
		
		while(get_cap() < MAXCAP && key == 3) //C(+)
		{
			rotate_cap(1);
		    lcd_putstring(0, 7, "      ");
            lcd_putstring(0, lcd_putnumber(0, 7, get_cap(), -1) + 7, "pF");     
            wait_ms(100);
            key = get_keys();
		}    

        if(key == 5 && band < MAXBANDS) //Change band 1 up
		{
		    band++;
		    show_band(band);
			lcd_putstring(1, 0, "             ");
			while(get_keys());
		}
		
        if(band > 0 && key == 6)         //Preset Band (-)
        {
			band--;
			show_band(band);
			lcd_putstring(1, 0, "             ");
			while(get_keys());
		}
		
		if(key == 8)	//Execute new band set
		{
        		
			while(!eeprom_is_ready());
		    eeprom_write_byte((uint8_t*)32, band);
										
			lcd_putstring(1, 0, "Recalling.");
				
			i = eeprom_read_byte((uint8_t*)(band * 2));
			if(i & 128)
			{
				xcoil = 1;
				show_xcoil(xcoil);
			}	
			else
			{
				xcoil = 0;
				show_xcoil(xcoil);
			}
			i &= 0x7F;
						
	        if(i >= 0 && i < 128)
	        {
		        inductance = set_coils(i);
			    show_inductance(inductance);
	        }
	        else
	        {
		        set_coils(65);
		        lcd_putstring(0, 0, " -x-uH");
		    }    
									
			c1 = eeprom_read_byte((uint8_t*)(band * 2 + 1));
											
			if(c1 >= 0 && c1 <= MAXCAP)
			{
			    set_cap(c1);
			}	
			else
			{
				lcd_putstring(0, 7, "-x-pF");
			}	
				
			show_band(band);
			lcd_putstring(1, 0, "             ");
			while(get_keys());
		}
		
        if(key == 7) //SAVE
		{
		    show_band(band);
			lcd_line_cls(1);
			
		    while(!eeprom_is_ready());
			eeprom_write_byte((uint8_t*)(band * 2), i + (xcoil << 7)); 
				
			while(!eeprom_is_ready());
			eeprom_write_byte((uint8_t*)(band * 2 + 1), get_cap());
				
			while(!eeprom_is_ready());
			eeprom_write_byte((uint8_t*)32, band);
								
	        lcd_line_cls(1);
			lcd_putstring(1, 0, "Stored.");				
			wait_ms(1000);
			lcd_line_cls(1);
		}

        if(key == 9) //XCOIL
        {
			while(get_keys());
			if(xcoil)
			{
				xcoil = 0;
				show_xcoil(0);
				PORTB |= (1 << PB0);
			}
			else	
			{
				xcoil = 1;
				show_xcoil(1);          
				PORTB &= ~(1 << PB0);
			}
			inductance = set_coils(i);
			show_inductance(inductance);	
		}	
                
        key = 0;
		
		s0 += get_adc(0);
        s1 += get_adc(1);
            
		//Meter check
        if(loopcnt0++ > 16)
        { 
			if(get_adc(1) > 16)
			{
			    swr0 = (double) (s0 + s1) *  10 / (s0 - s1);
			    swr1 = (int) swr0;
			    if(swr1 >= 10 && swr1 <= 200)
			    {
			         lcd_putstring(1, 0, "SWR: 1:   "); 
			         lcd_putnumber(1, 7, swr1, 1);
			    }
			    else    
			    {
					lcd_putstring(1, 0, "SWR: undef. "); 
				}	
			}    
			else
			{
			    lcd_putstring(1, 0, "NO SIG!   "); 
			}    
			s0 = 0;
			s1 = 0;
			
            loopcnt0 = 0;
        }    
        
	}
	return 0;
}

