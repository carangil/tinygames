/*
 * SMALL.c
 *
 * Source code for the 'tinygames' project.
 *
 * Created: 11/22/2016 5:30:05 PM
 *  Author: Mark Sherman
 *  Copyright (c) 2016/2017 Mark W. Sherman, GPL v2 or higher
 *  exceptions:  function 'wait_until' is Copyright (c) 2010 Myles Metzer
 */ 

/* Timing CRAP */
/* I am using a 10.7Mhz crystal, beacuse that's what I had sitting around. 
   If you change F_CPU, you need to scale the following constants proportionally 
*/
#define F_CPU				10752000L

/* How many clocks make about 1 microsecond */
#define U_SECOND_CLOCKS		11

/* How many clocks for 1 line of video (about 15 khz) */
#define LINE_CLOCKS			682

/* Half of LINE_CLOCKS*/
#define HALF_LINE_CLOCKS	341


/* Bring in standard stuff */

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


/* Which line is currently being drawn.  */
unsigned int line;


/* Taken from CAT-644 VGA routines and hacked up a bit */
inline void ntsc_init()
{
	TIMSK1=_BV(OCIE1A);					/* use timer interrupt */
	TCCR1B = (1<<CS10)  | (1<<WGM12 );	/* we use an undivided closk, and we reset when the timer value (TCNT1) reach the target number (OCR1A)*/
	
	/* I don't have the program space for the following 2 lines, but the program should shortly self-correct after a couple frames of CRAP */	
	//	TCNT1 = 0x00; //zero timer count
	//	line = 0;

	OCR1A = LINE_CLOCKS;		/* set timer to fire once per line */
	NTSC_DDR |= NTSC_DDR_MASK;	/* set the pins we need to be outputs */
	sei();						/* enable interrupts */
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

/* How many clocks into the interrupt to start the HSYNC pulse */
#define HSYNCSTART 120


#define SCRCOLS 	9	/*number of items in a row. Yes only 9 objects per row */

/* Marks the end of the mushrooms or bugs array */
#define ENDOFLINE	255

/* Entry is invalid */
#define INVALID 254

/* Entry is erased */
#define ERASED	253



/* These two arrays keep track of the screen positions of each mushroom or bug 
   Each array should be sorted at all times to display on the screen properly.
   It is OK to be unsorted for a few frames as long as there is always a tendency
   for being sorted.
*/
unsigned char mushrooms[]={14,25,60,ERASED,ERASED,ERASED,ERASED,ERASED,ERASED,ERASED,ENDOFLINE};
unsigned char bugs[]={0,1,2,3,4,5,6,7,ENDOFLINE,ENDOFLINE};

/* This tracks the position in the array to draw */
unsigned char* drawpos=&mushrooms[0];  /* TODO: don't init this */

unsigned char field=0;

/* This tracks the current screen position of the CRT
	if scrpos == *drawpos, we have to draw pixels for an object
*/
unsigned char scrpos=0;


/* Bit patterns for mushroom and bug */

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
  
  
/* This is volatile to avoid a GCC optimization.  Unvolatilize it, I dare you. */
volatile unsigned char sprite;

/* Track player and bullet position */
unsigned char playerx;
unsigned char bulletx;
unsigned char bullety;


ISR (TIM1_COMPA_vect ) {
	unsigned char i;		  
	unsigned char scrposo;	  /* screen position before this scanline*/
	signed char shifter;	  /* used to shift out a pixel */
	unsigned int* drawposold; /* drawing position before this scanline */
	
	/* simplified vsync:  only do the vertical serrations
	   The 'equalization pulses' will be taken care of as regular sync
	   After failing a couple of times of the full even/odd 1/2 frame CRAP, I switched to a true analog CRT and ran hsync only.  After find the problem, I added the only real 'defining' characterisic
	   of vsync: the vertical serrations.  After that it sync up fine on both CRT and LCD.
	   Then later, I find the Atari 2600 Stella chip manual, which tells us that using only vertical serrations for vsync works on 'all kinds of tvs'.
	*/

	if (line >=6 && line <12 ) {
		OCR1A = HALF_LINE_CLOCKS;  /* vsync lines are 1/2 length*/
		
		wait_until(HSYNCSTART-   (  ( U_SECOND_CLOCKS * 440L  ) /100 )   );
		NTSC_PORT |= NTSC_SYNC;  //sync high   
		wait_until(HSYNCSTART);			 
		NTSC_PORT &= ~NTSC_SYNC;  //sync low    vsync is normally low durring verical serrations .  This is timed so the low transition is the same as when it does for hsync
		goto end;
		//line++;
		//return;  //scync was easy, maybe we can go back to the main program
	}

	if (line==12) {
		OCR1A = LINE_CLOCKS ; //back to regular sized lines
		scrpos=0; //reset screen pos counter
	}

	wait_until(HSYNCSTART);

	NTSC_PORT &= ~NTSC_SYNC;    /* sync low */

	wait_until(HSYNCSTART + (  ( U_SECOND_CLOCKS * 425L  ) /100 )   );	/* 4.25 microseconds */

	NTSC_PORT |= NTSC_SYNC;		/* sync high */ 
	
	wait_until(HSYNCSTART + (  ( U_SECOND_CLOCKS * (425+800L)  ) /100 )   );   /* 8 microseconds */
	
	
	/* On the line of a bullet, we actually don't draw anything else */
	
	if (line == bullety) {
		
		for (i=-5;i!= bulletx;i++) {  /* Delay to bullet position. -5 determined by experimentation to match player position */
			NTSC_PORT=NTSC_BLACK;
		}
		
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_WHITE;
		NTSC_PORT=NTSC_BLACK;
		goto end;
		
		/* jumping to end to line++ and return is less code than doing it here */
		//line++;
		//return;
	}
	
	if (line == 262 + 3 ) { /* we are on the 263nd line. 262 because 1st line is 0.  Then +3 because vsync lines are double speed*/
			line=0;   /*next up is vsync*/
			field^=1; /*switch fields*/
			
			/* On odd frames we draw mushrooms, on even frame we draw bugs */
			if (field){
				drawpos=&mushrooms[0];
			}
			else{	
				drawpos=&bugs[0];
			}
			
			return; /* note we don't line++, because we just reset it to 0 */
	}
	

	
	if ((line>64)&&(line <224))
	{
		/*Active video lines */	
		
		scrposo=scrpos; /*in case we need to reset*/
		drawposold = drawpos; /*in case we need to reset*/
				 
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
				/* We still go thru the trouble of shifting out all black for empty spaces
				   so that we can have consistent timing */
				shifter = 0b10000000;
				
			}
			
			/* Shifting RIGHT will make every pixel pass thru the bit that outputs the color
			   I used a SIGNED type, so that is is sign extened, so bit 7 remains high.
			   Bit 7 is the sync line 
			   
			   The other bits in the port are either inputs with external pullups, or unused.
			   
			*/
			
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
			
			NTSC_PORT=NTSC_BLACK; /* to keep last pixel from being stuck on */
			
			
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
	
	end:
	line++;

}


/* joystick inputs */

#define JOYSTICK PINA
#define FIRE  0b00100000     
#define LEFT  0b00001000     
#define RIGHT 0b00010000     

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
/*	volatile unsigned char* px = &playerx;  //Use if necessary */

	
	//scrpos=0;
	
	ntsc_init();
	
	while(1){

		for (i=58000;i;i++) {
		   __asm__ __volatile__ ( "");         /* This makes the delay loop actually happen */
		}
		
		tick = (tick +1) & 0xF;  //tick counter 0 to 15
		
	
		/* advance the player */
			
		
		if (! (JOYSTICK&LEFT))
			playerx--;
			//  *((volatile char*)(&playerx)) -= 1;  //force update to interrupt handler
		
		if (! (JOYSTICK&RIGHT))
			playerx++;
			//	*((volatile char*)(&playerx)) += 1;  //force update to interrupt handler
		
		/* restrict player motion */
		playerx&=31;
	
			
		

		bulletpos = INVALID;
			
		if (bullety< 64  ) {
			/* reset bullet position */		
			
			bulletx=playerx;  /* So bullet lines up with where the player was.  Determined by experimentation */
			bullety=2; //off the screen
			
			if (!(JOYSTICK&FIRE))	/* If the fire button is pressed, shoot off a bullet */
				bullety=240;  
			
		} else {
			/* move bullet up */
					
			bullety-=2;  // very important we never interrupt ODD scanlines (every 7th line updates the sprite counters, and drawing the bullet suppresses a row of pixels)
			bulletpos = (bullety - 64) / 8 * SCRCOLS + (bulletx/4); /*turn bullet x/y into approximate screen position number */
			
			if (bulletx>20)     /* Correction factor because integer division sucks */
				bulletpos++;
	
			/*
			//For calibratation purposes
			These mushrooms are the leftmost, rightmost, and bullet positions. 
			mushrooms[0] = bulletpos % SCRCOLS;  //show the column we shoot in
			mushrooms[1] = SCRCOLS;  //show the column we shoot in
			mushrooms[2] = 2*SCRCOLS -1;
			*/	
	
		}
		
		
		
		
		/* Update bullet via the volatile pointers */
		
		*bx=bulletx;
		*by=bullety;
		
		
		/* gcc optimizes by keeping bx and by in registers.  But we need
		   the interrupt routine to see it.
		   Normally, you would just make bulletx/y as volatile, but that adds
		   extra instructions where I don't need it.  I just need to 'poke' these
		   vars at least once per main loop
		*/
		
		
		//*px=playerx;
		/* NOTE: for some reason we don't need to update playerx
		   through a volatile.  I guess gcc spills this var to 
		   memory at least once per main loop
		   
		   If the player position becomes stuck, you have to 
		   re-volatilize-it 
		 */
			

	
		resort:  /* Yes, there is a goto down below, deal with it */
	
		/* go through all bugs */
		for (bugp=bugs; *bugp != ENDOFLINE; bugp++) {
			
		
			if (tick==0) {
				/* we are on an update tick: move all the bugs */
				
					
				if ((*bugp/SCRCOLS)&1) {     /*bugs move left on odd rows*/
					if ((*bugp % SCRCOLS) ==0)
						*bugp += (SCRCOLS+1); /* +1 is because we un-advance in next statement. */
						/* we could easily not do this and instead do an else, but that adds
						  an extra jump statement to get around the else clause */
					 
					(*bugp)--;  
				}
				else  { /*bugs move left on even rows*/
					
					if ((*bugp % SCRCOLS) ==(SCRCOLS-1))
						*bugp += (SCRCOLS-1);   /* -1 is because we advance in next statement. */
						/* we could easily not do this and instead do an else, but that adds
						  an extra jump statement to get around the else clause */
					
					(*bugp)++;
					
				}		
			} else {
				/* if we are not on an update tick, we can safely move around the bug ordering
				   to repair the sort */
				
				if (   (*bugp)  >= (*(bugp+1))  ) {
					
					t=*bugp;
					
					if ( (*bugp)  == (*(bugp+1)))
						t--; /*on conflict move one bug back*/
					
					*bugp=*(bugp+1);
					*(bugp+1) = t;
					
					goto resort;
					
					/* We use the goto to force a re-sorting. If we don't it takes too
					   many main loop iterations to fix the order, and the display
					   looks bad. We can repeat these gotos as many times as we need
					   because we are NOT in an update tick. */
					
				}
				
				
			}
		
		
		
		
		
		
			/* scan thru all mushrooms */
			
			for (mushp=mushrooms;*mushp!=ENDOFLINE;mushp++) {
				
				if (tick==0) {
					/* if on updatetick jump down 1 row when bug hits mushroom */
					if (*mushp == *bugp)
						(*bugp)+=SCRCOLS;
					
				}
				
				
				if (bulletpos == *mushp) {	
					bullety=0;
					*mushp = ERASED; /* erase a mushroom if touched by bullet */	
				}
								
				
				/* if we have a free mushroom slot, insert pending mushroom*/
				if (*mushp==ERASED){
					*mushp=insmush;
					insmush = ERASED;
					
				}
				
				/* swap if we find out of order */
				/* note, we don't need a goto, as mushrooms are scanned much more
				   often than bugs because its the inner loop */
				
				if ( *(mushp) > *(mushp+1) ) {
					t=*mushp;
					*mushp=*(mushp+1);
					*(mushp+1) = t;
				}
				
				
			} /* end mushroom loop */
			
		
			/* Check if bug is shot */
			
			if (bulletpos == *bugp) {
				bullety=0;
				insmush = bulletpos;  //set mushroom insert
				*bugp = 0; //when shot a new bug appears
			}

		}  /* end bug loop */
				
	}// end main looop

} //end main
 