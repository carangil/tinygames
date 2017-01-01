/*
 * SMALL.c
 *
 * Created: 11/22/2016 5:30:05 PM
 *  Author: Mark Sherman
 *  Copyright (c) 2016 Mark W. Sherman, GPL v2 or higher
 *  exceptions:  function 'wait_until' is Copyright (c) 2010 Myles Metzer
 */ 

/* Timing CRAP */

//8mhz clock
/*
#define F_CPU				8000000L
#define U_SECOND_CLOCKS		8
#define LINE_CLOCKS			508
#define HALF_LINE_CLOCKS	254
*/

//10ish mhz
#define F_CPU				10752000L
#define U_SECOND_CLOCKS		11
#define LINE_CLOCKS			682
#define HALF_LINE_CLOCKS	341


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define NTSC_PORT	PORTA
#define NTSC_PIN	PINA
#define NTSC_DDR	DDRA
#define NTSC_LUM	0b00000010
#define NTSC_SYNC	0b10000000
#define NTSC_DDR_MASK  (NTSC_LUM | NTSC_SYNC)

/* Setting NTSC_PORT to NTSC_BLACK or NTSC_WHITE will also leave the sync line high */
/* These are so you can set the output color with OUT instead of SBI/CBI and saves 1 clock cycle*/
#define NTSC_BLACK	NTSC_SYNC
#define NTSC_WHITE  (NTSC_LUM | NTSC_SYNC)

unsigned int line;

/* Taken from CAT-644 VGA routines and hacked up a bit */
void ntsc_init()
{
	TIMSK1=_BV(OCIE1A);
	TCCR1B = (1<<CS10)  | (1<<WGM12 );
	TCNT1 = 0x00; //zero timer count
	line = 0;
	OCR1A =LINE_CLOCKS;   
	NTSC_DDR |= NTSC_DDR_MASK;
	sei();
}

/* wait_until is copied from arduino tv-out library  */
/*
 Copyright (c) 2010 Myles Metzer

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/
static inline void wait_until(unsigned char time) {
	
	__asm__ __volatile__ (
	"sub	%[time], %[tcnt1l]\n\t"
	"subi	%[time], 10\n"
	"100:\n\t"
	"subi	%[time], 3\n\t"
	"brcc	100b\n\t"
	"subi	%[time], 0-3\n\t"
	"breq	101f\n\t"
	"dec	%[time]\n\t"
	"breq	102f\n\t"
	"rjmp	102f\n"
	"101:\n\t"
	"nop\n"
	"102:\n"
	:
	: [time] "a" (time),
	[tcnt1l] "a" (TCNT1L)
	);
}

#define HSYNCSTART 120


#define SCRCOLS 	9	/*number of items in a row */
#define ROWDIRMASK 0b10000  /* which bit in the position flips every row:  this determines the direction a bug will travel */

#define ENDOFLINE	255
#define INVALID	254

unsigned char mushrooms[]={10,15,20,ENDOFLINE,ENDOFLINE,ENDOFLINE,ENDOFLINE,ENDOFLINE,ENDOFLINE};
//unsigned int* mushpos=&mushrooms[0];


unsigned char bugs[]={1,ENDOFLINE,ENDOFLINE,ENDOFLINE,ENDOFLINE};
	//,2,3,4,5,6,7,8,9,10,11,12,13,14,21,22,23,ENDOFLINE};
//unsigned int* bugpos=&bugs[0];

unsigned char* drawpos=&mushrooms[0];


unsigned char field=0;

unsigned char scrpos=0;





unsigned char mushpxp[]={
	0b10000000,
	0b10111100,
	0b11111110,
	0b11111110,
	0b10011000,
	0b10011000,
	0b10011000,
	0b10000000
};


unsigned char bugpxp[]={
	0b10010100,
	0b10111100,
	0b11111110,
	0b11111110,
	0b11111110,
	0b11111110,
	0b10111100,
	0b10010100
};
  
  
  
volatile unsigned char sprite;

unsigned char playerx=0;
unsigned char bulletx=10;
unsigned char bullety=160;

ISR (TIM1_COMPA_vect ) {
	unsigned char i;
	unsigned char scrposo;

	signed char shifter;
	unsigned int* drawposold;
	
	// simplified vsync:  only do the vertical serrations
	// The 'equalization pulses' will be taken care of as regular sync
	// After failing a couple of times of the full even/odd 1/2 frame CRAP, I switched to a true analog CRT and ran hsync only.  After find the problem, I added the only real 'defining' characterisic
	// of vsync: the vertical serrations.  After that it sync up fine on both CRT and LCD.
	// Then later, I find the Atarti 2600 Stella chip manual, which tells us that using only vertical serrations for vsync works on 'all kinds of tvs'.
	

	if (line >=6 && line <12 ) {
		OCR1A = HALF_LINE_CLOCKS;  /* vsync lines are 1/2 length*/
		
		wait_until(HSYNCSTART-   (  ( U_SECOND_CLOCKS * 440L  ) /100 )   );
		NTSC_PORT |= NTSC_SYNC;  //sync high   //go up
		wait_until(HSYNCSTART);			 
		NTSC_PORT &= ~NTSC_SYNC;  //sync low    vsync is normally low.  This is timed so the low transition is the same as when it does for  hsync
		line++;
		return;  //scync was easy, maybe we can go back to the main program
	}

	if (line==12) {
		OCR1A = LINE_CLOCKS ; //back to regular sized lines
		scrpos=0; //reset screen pos counter
	}

	wait_until(HSYNCSTART);

	NTSC_PORT &= ~NTSC_SYNC;  

	wait_until(HSYNCSTART + (  ( U_SECOND_CLOCKS * 425L  ) /100 )   );

	NTSC_PORT |= NTSC_SYNC;  
	
	wait_until(HSYNCSTART + (  ( U_SECOND_CLOCKS * (425+800L)  ) /100 )   );
	if (line == bullety) {
		
		for (i=0;i< bulletx;i++) {  //delay
			NTSC_PORT=NTSC_BLACK;
		}
		
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_BLACK;
		
		line++;
		return;
	}
	
	if (line == 262 +3 ) { /* we are on the 263nd line. 262 because 1st line is 0.  Then +3 because vsync lines are double speed*/
			line=0;   //next up is vsync
			field^=1; //switch fields
			
			if (field){
				sprite = mushpxp[ line&7];
				drawpos=&mushrooms[0];
			}
			else{
				sprite = bugpxp[ line&7];	
				drawpos=&bugs[0];
			}
			
			return;
	}
	

	
	if ((line>64)&&(line <220))
	{
		/*Active video lines */	
		
		scrposo=scrpos; //in case we need to reset
		
		drawposold = drawpos; //in case we need to reset
				 
		if (field){
			sprite = mushpxp[ line&7];
		}
		else{
			sprite = bugpxp[ line&7];
		}
		
				 
		for (i=0;i<SCRCOLS;i++) {
			
			
			
			
			if (scrpos==*drawpos) {
				shifter=sprite;
				drawpos++;
			}
			else {
				shifter = 0b10000000;
				
			}
			
				NTSC_PORT=shifter;
				shifter=shifter>>1;
				NTSC_PORT=shifter;
				shifter=shifter>>1;
				NTSC_PORT=shifter;
				shifter=shifter>>1;
				NTSC_PORT=shifter;
				shifter=shifter>>1;
				NTSC_PORT=shifter;
				shifter=shifter>>1;
				NTSC_PORT=shifter;
				NTSC_PORT=NTSC_BLACK;
				///this last pixel is being stuck on
			
			
			scrpos++;
		}
		
		if ((line & 7 )!= 7) { //all but the 7th line of a sprite, reset this
			scrpos=scrposo;
			drawpos=drawposold;
		}
	
	}
	
	if (line >230) {
		
		
		for (i=0;i< playerx;i++) {  //delay
			NTSC_PORT=NTSC_BLACK;
		}
		
		
		NTSC_PORT|=NTSC_WHITE;
		NTSC_PORT|=NTSC_WHITE;

		NTSC_PORT=NTSC_BLACK;
		
		
				
		
	}
	
	line++;

}



void main(void)
{
	unsigned int i;
	int j;

	unsigned char c;
	unsigned char d;
	unsigned char* bugp;
	unsigned char* mushp;
	unsigned char bulletpos; 
	

/*	
	DDRA=0b11000000;  //OCR0A pin output
	DDRB=0b10000100;  //OCR0B pin output
	
	
	//setup timer output pin behavior
	TCCR0A =		(1<<COM0A1)     //table 11-3, WGM mode , non-inverting A
				|	(1<<COM0B1)		//table 11-3, WGM mode , non-inverting B
				| (1<<WGM01) | (1<<WGM00) ;  //(table 11-8) Fast PWM 
	
	TCCR0B = (1<<CS00);  //table 11-9, clock select unscaled
	
	
	OCR0A=64;    //   1/4
	OCR0B=128;   //  1/2
*/
	
	scrpos=0;
	ntsc_init();

	while(1){

		bugp = bugs;
		mushp = mushrooms;
		
		
	
		
		
		for (bugp=bugs;(*bugp)!=ENDOFLINE;bugp++){
			
		
				
				//bug direction is based on row	
				if ((*bugp/SCRCOLS)&1)
					j=-1; 
				else
					j=1;
					
					
				
				if ((*bugp % SCRCOLS) ==0 &&  j==-1)
					*bugp += SCRCOLS;
							
				else if ((*bugp % SCRCOLS) ==(SCRCOLS-1) &&  j==1)
					*bugp += SCRCOLS;

				else (*bugp)+=j;
				
		#if 0	
				//if a bug pair is out of order, swap them
				//this is bubble sort.  every frame this will run ONCE 
				//if the sort order is violated, it should correct quickly
				if ((*bugp) > *(bugp+1)) {
					i = *bugp;
					*bugp = *(bugp+1);
					*(bugp+1) = i;	
				}
		#endif
					
		}
		
		bugp = bugs;
		mushp = mushrooms;
		
		//check mushroom-bug interactions
		while(1){
				
				
				if ( ((*bugp)==ENDOFLINE) && ((*mushp) == ENDOFLINE))
					break;
				
								
				if (*bugp == *mushp) {
					(*bugp)+=SCRCOLS;  //jump over it
					
					//advance both pointers.
					bugp++;
					mushp++;
					continue;
				}
				
				
				/* advance whichever pointer is behind */
				
				if ((*bugp) > (*mushp))
					mushp++;
				else 
					bugp++;
				
								
			}
		
		
		
		
		for (i=0;i<15;i++) {
			
			_delay_ms(10);
		
			playerx++;
		
			if (playerx==36)
				playerx=0;
		
			bullety-=2;
			if (bullety<=64) {
				bullety=220;
				bulletx=playerx;
			}
			
			{
			
			
			
				bulletpos = (bullety - 64) / 8 * SCRCOLS + (bulletx/4);		
				mushp = mushrooms;
				
				while (*mushp !=ENDOFLINE) {
				
				

				
					//mushroom bubble sort

					if ((*mushp) > *(mushp+1)) {
						i = *mushp;
						*mushp = *(mushp+1);
						*(mushp+1) = i;
					}
				
				
					
				
					//scan mushrooms for bullet interactions				
				
				
					if (*mushp == bulletpos) {
						*mushp = (ENDOFLINE-1);
						bullety=64; //end bullet 
						bulletpos = ENDOFLINE;
					}
				
					mushp++;	
				}
			
			
		
				//buglist
				bugp = bugs;
				while (*bugp !=ENDOFLINE) {
				
				

		
					//bug bubble sort

					if ((*bugp) > *(bugp+1)) {
						i = *bugp;
						*bugp = *(bugp+1);
						*(bugp+1) = i;
					}
				
				
		
				
					//scan bugs for bullet interactions
				
				
					if (*bugp == bulletpos) {
						*bugp = (ENDOFLINE-1);  //kill bug
						*(mushp++) = bulletpos; //create mushroom at end of list
						*(mushp++) = ENDOFLINE; //end the mushroom list
						bullety=64; //end bullet
					}
				
					bugp++;
				}
			
			}

			
			
		}
		
	}

}