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
#define INVALID 254
#define ERASED	253

unsigned char mushrooms[]={20,46,77,ERASED,ERASED,ERASED,ENDOFLINE};
//unsigned int* mushpos=&mushrooms[0];


unsigned char bugs[]={0,1,2,3,4,5,ENDOFLINE,ENDOFLINE,ENDOFLINE,ENDOFLINE};
	
	
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
		//		sprite = mushpxp[ line&7];
				drawpos=&mushrooms[0];
			}
			else{
			//	sprite = bugpxp[ line&7];	
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
	
	if (line >240) {
		
		
		for (i=0;i< playerx;i++) {  //delay
			NTSC_PORT=NTSC_BLACK;
		}
		
		for (i=0;i<4;i++)
			NTSC_PORT|=NTSC_WHITE;
		
		NTSC_PORT=NTSC_BLACK;
		
		
				
		
	}
	
	line++;

}

/*
void check(unsigned char* bugp) 
{
	//enforce ordering, and delete duplicates
	
	unsigned char t;
	if ( *(bugp) >= *(bugp+1) ) {
		if (*(bugp) == *(bugp+1))
			t=ERASED;
		else
			t=*bugp;
		
		
		*bugp=*(bugp+1);
		*(bugp+1) = t;
	}
	
	
}
*/

void main(void)
{
	unsigned int i;
	int j;
	unsigned char t;

	unsigned char* bugp;
	unsigned char* mushp;
	unsigned char* nmushp;
	unsigned char bulletpos; 
	unsigned char abug=0;
	unsigned char tick=0;
	unsigned char insmush;
	volatile unsigned char* bx = &bulletx;
	volatile unsigned char* by = &bullety;

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
				
	//	_delay_ms(10);
//__asm__ __volatile__ ( "nop");
	for (i=55000;i;i++) {
	   __asm__ __volatile__ ( "nop");
	}
	//	__asm__ __volatile__ ( "nop");
		
		tick = (tick +1) & 0xF;  //tick counter 0 to 15
	//	tick++;  //rolls over 
		
	
		//advance the player
		
		*((volatile char*)(&playerx)) += 1;  //force update to interrupt handler
		
	//	playerx++;
		
		if (playerx>36)
			playerx=0;
			
		//advance the bullet

		bulletpos = INVALID;
	
		
		if (bullety<=64) {
			//refire bullet at player pos	
			
			//add read-port for firebutton here
			bulletx=playerx;   
			bullety=240;  
			
		} else {
			bullety-=2;  // very important we never interrupt ODD scanlines (every 7th line updates the sprite counters, and drawing the bullet suppresses a row of pixels)
			bulletpos = (bullety - 64) / 8 * SCRCOLS + (bulletx/4); //turn bullet x/y into approximate screen position number
		}
		
		*bx=bulletx;
		*by=bullety;
				
				
//enforce bug sort:
					
	/*		
	resort:
	
		for (bugp=bugs;*bugp!=ENDOFLINE;bugp++) {
						
			if (   (*bugp)  >= (*(bugp+1))  ) {
			
				t=*bugp;
				
				if ( (*bugp)  == (*(bugp+1)))
					t=INVALID;
								
				*bugp=*(bugp+1);
				*(bugp+1) = t;
				
				goto resort;
			}
			
			
		}
	*/
	
	resort:
		for (bugp=bugs; *bugp != ENDOFLINE; bugp++) {
			
		
			if (tick==0) {
				//we are on an update tick
				
					
				if ((*bugp/SCRCOLS)&1) {     //bug moves left
					if ((*bugp % SCRCOLS) ==0)
						*bugp += (SCRCOLS+1);
					 
					(*bugp)--;  //backward on odd rows
				}
				else  { //bug moves right
					
					if ((*bugp % SCRCOLS) ==(SCRCOLS-1))
						*bugp += (SCRCOLS-1);
					
					(*bugp)++;
					
				}
								
				
								
			} else {
				
				if (   (*bugp)  >= (*(bugp+1))  ) {
					
					t=*bugp;
					
					if ( (*bugp)  == (*(bugp+1)))
					t=INVALID;
					
					*bugp=*(bugp+1);
					*(bugp+1) = t;
					
					goto resort;
				}
				
				
			}
		
		
		
		
		
		
			//scan thru all mushrooms
			for (mushp=mushrooms;*mushp!=ENDOFLINE;mushp++) {
				
				if (tick==0) {
					//if on updatetick jump down when bug hits mushroom
					if (*mushp == *bugp)
						(*bugp)+=SCRCOLS;
					
				}
				
				
				if (bulletpos == *mushp) {	
					bullety=0;
					*mushp = ERASED; //erase a mushroom if touched by bullet
				}
				
				//swap if we find out of order
				//check(mushp);
				
				if (*mushp==ERASED){
					*mushp=insmush;
					insmush = ERASED;
				}
				
				if ( *(mushp) > *(mushp+1) ) {
					t=*mushp;
					*mushp=*(mushp+1);
					*(mushp+1) = t;
				}
				
				
			}
			
#if 1			
			//check if bug shot
				if (bulletpos == *bugp) {
					bullety=0;
					insmush = *bugp;  //set mushroom insert
				//	*(mushp++)= *bugp;
				//	*(mushp)= ENDOFLINE;
					*bugp = ERASED; //erase a bug if touched by bullet
										
				}
	#endif
				
			
			
		}
		
		
		
		
			
	
		
		
	}// end main looop

} //end main
 