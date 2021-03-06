#include "VGAX.h"

//HSYNC pin used by TIMER2
#define HSYNCPIN 3

//These two pin cannot be modified without modify the HSYNC assembler code
#define COLORPIN0 6
#define COLORPIN1 7

//VSYNC pin used by TIMER1.
#define VSYNCPIN 9

//Number of VGA lines to be skipped (black lines)
/*These lines includes the vertical sync pulse and back porch.
Minimum value must be 35 (calculate from Nick Gammon)
You can modify this value to center the framebuffer vertically, or not*/
#define SKIPLINES 20

static byte afreq, afreq0;
unsigned vtimer;
static byte aline, rlinecnt;
static byte vskip;
byte vgaxfb[VGAX_HEIGHT*VGAX_BWIDTH];

//VSYNC interrupt
ISR(TIMER1_OVF_vect) {
  aline=-1;
  vskip=SKIPLINES;
  vtimer++;
  rlinecnt=0;
}

//HSYNC interrupt
ISR(TIMER2_OVF_vect) {
  /*
  NOTE: I prefer to generate the line here, inside the interrupt.
  Gammon's code generate the line pixels inside main().
  My versin generate the signal using only interrupts, so inside main() function
  you can do anything you want. Your code will be interrupted when VGA signal
  needs to be generated
  */
  
  // wait 15 cycles to align left side of the screen
  asm volatile(
      ".rept 15             \n\t"
    "      nop              \n\t"
      ".endr                \n\t"
  );

  //check vertical porch
  if (vskip) {
      vskip--;
      return;
  }

  if (rlinecnt<VGAX_HEIGHT) {   
    //interrupt jitter fix (needed to keep signal stable)
    //code from https://github.com/cnlohr/avrcraft/tree/master/terminal

    #define DEJITTER_OFFSET 1
    #define DEJITTER_SYNC -3
    asm volatile(
      "     lds r16, %[timer0]    \n\t" //
      "     subi r16, %[tsync]    \n\t" //
      "     andi r16, 7           \n\t" //
      "     call TL               \n\t" //
      "TL:                        \n\t" //
      "     pop r31               \n\t" //
      "     pop r30               \n\t" //
      "     adiw r30, (LW-TL-5)   \n\t" //
      "     add r30, r16          \n\t" //
      "     ijmp                  \n\t" //
      "LW:                        \n\t" //
      "     nop                   \n\t" //
      "     nop                   \n\t" //
      "     nop                   \n\t" //
      "     nop                   \n\t" //
      "LBEND:                     \n\t" //
    :
    : [timer0] "i" (&TCNT0),
      [toffset] "i" ((uint8_t)DEJITTER_OFFSET),
      [tsync] "i" ((uint8_t)DEJITTER_SYNC)
    : "r30", "r31", "r16", "r17");

    /*
    Output all pixels.

    NOTE: My trick here is to unpack 4 pixels and shift them before writing to
    PORTD.

    Pixels are packed as 0b11223344 because the first pixel write have no time
    to perform a shift (ld, out) and must be prealigned to the two upper bits 
    of PORTD, where the two wires of the VGA DSUB are connected. The second, 
    the third and the forth pixels are shifted left using mul opcode instead 
    of a left shift opcode. Shift opcodes are slow and can shift only 1 bit at
    a time, using 1 clock cycle. mul is faster.

    Instead of using a loop i use the .rept assembler directive to generate an 
    unrolled loop of 30 iterations.
    */
    asm volatile (
      "    ldi r20, 4       \n\t" //const for <<2bit
      ".rept 24             \n\t" //output 4 pixels for each iteration
      "    ld r16, Z+       \n\t" //
      "    out %[port], r16 \n\t" //write pixel 1
      "    mul r16, r20     \n\t" //<<2
      "    out %[port], r0  \n\t" //write pixel 2
      "    nop              \n\t" //expand last pixel
      "    mul r0, r20      \n\t" //<<4
      "    out %[port], r0  \n\t" //write pixel 3
      "    nop              \n\t" //expand last pixel
      "    mul r0, r20      \n\t" //<<6
      "    out %[port], r0  \n\t" //write pixel 4
      "    nop              \n\t" //expand last pixel
      ".endr                \n\t" //
      "    nop              \n\t" //expand last pixel
      "    ldi r16, 0       \n\t" //
      "    out %[port], r16 \n\t" //write black for next pixels
    :
    : [port] "I" (_SFR_IO_ADDR(PORTD)),
      "z" "I" ((byte*)vgaxfb + rlinecnt*VGAX_BWIDTH)
    : "r16", "r17", "r20", "r21", "memory");

    //increment framebuffer line counter after 6 VGA lines
    if (++aline==5) { 
      aline=-1;
      rlinecnt++;
    }
  } 
}
void VGAX::begin() {
  //Timers setup code, modified version of the Nick Gammon's VGA sketch
  cli();
  
  //disable TIMER0 interrupt
  TIMSK0=0;
  TCCR0A=0;
  TCCR0B=1; //enable 16MHz counter (used to fix the HSYNC interrupt jitter)
  OCR0A=0;
  OCR0B=0;
  TCNT0=0;
  //TIMER1 - vertical sync pulses
  pinMode(VSYNCPIN, OUTPUT);
  TCCR1A=bit(WGM11) | bit(COM1A1);
  TCCR1B=bit(WGM12) | bit(WGM13) | bit(CS12) | bit(CS10); //1024 prescaler
  ICR1=259; //16666 / 64 uS=260 (less one)
  OCR1A=0; //64 / 64 uS=1 (less one)
  TIFR1=bit(TOV1); //clear overflow flag
  TIMSK1=bit(TOIE1); //interrupt on overflow on TIMER1
  //TIMER2 - horizontal sync pulses
  pinMode(HSYNCPIN, OUTPUT);
  TCCR2A=bit(WGM20) | bit(WGM21) | bit(COM2B1); //pin3=COM2B1
  TCCR2B=bit(WGM22) | bit(CS21); //8 prescaler
  OCR2A=63; //32 / 0.5 uS=64 (less one)
  OCR2B=7; //4 / 0.5 uS=8 (less one)
  TIFR2=bit(TOV2); //clear overflow flag
  TIMSK2=bit(TOIE2); //interrupt on overflow on TIMER2
  //pins for outputting the colour information
  pinMode(COLORPIN0, OUTPUT);
  pinMode(COLORPIN1, OUTPUT);  
  sei();
}
void VGAX::end() {
  //disable TIMER0
  TCCR0A=0;
  TCCR0B=0;
  //disable TIMER1
  TCCR1A=0;
  TCCR1B=0;
  //disable TIMER2
  TCCR2A=0;
  TCCR2B=0;
}

void VGAX::clear(byte color) {
  register byte c=color;
  c&=3;
  register byte c0=(c*4) | c;
  c0|=c0*16;
  unsigned cnt=VGAX_BSIZE;
  byte *o=(byte*)vgaxfb;
  while (cnt--)
    *o++=c0;
}

void VGAX::copy(byte *src) {
  byte *o=(byte*)vgaxfb;
  unsigned cnt=VGAX_BSIZE;
  while (cnt--)
    *o++=pgm_read_byte(src++);
}

void VGAX::fillrect(byte x, byte y, byte width, byte height, byte color) {
  byte rh=height;
  while (rh--) {
    byte rw=width, rx=x;
    while (rw--) {
      if (rx<VGAX_WIDTH && y<VGAX_HEIGHT)
        putpixel(rx, y, color);
      rx++;
    }
    y++;
  }
}

void VGAX::delay(int msec) {
  while (msec--) {
    unsigned cnt=16000/32; //TODO: use a more precise way to calculate cnt
    while (cnt--)
      asm volatile("nop\nnop\nnop\nnop\n");
  }
}
