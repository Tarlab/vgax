/*
VGAX Library for Arduino UNO (ATMega328 MCU)
Source code url: https://github.com/smaffer/vgax

120x60px VGA framebuffer with 4 colors

COPYRIGHT (C) 2014 Sandro Maffiodo
smaffer@gmail.com
http://www.assezeta.com/sandromaffiodo

based on the "VGA color video generation" by Nick Gammon:
  http://www.gammon.com.au/forum/?id=11608.
inspired from the work of Peten Paja:
  http://petenpaja.blogspot.fi/2013/11/toorums-quest-ii-retro-video-game.html
AVR interrupt dejitter from Charles CNLOHR:
  https://github.com/cnlohr/avrcraft/tree/master/terminal

see https://github.com/smaffer/vgax for the library description and for the
hardware wiring.

HERE you can find some inline documentation about the VGAX library class 
*/
#ifndef __VGAX_library__
#define __VGAX_library__

#include <Arduino.h>
#include <avr/pgmspace.h>

// 60/30/7 = 120*60
// 80/24/5 = 96*80
#define VGAX_HEIGHT 80 //number of lines
#define VGAX_BWIDTH 24 //number of bytes in a row
#define VGAX_WIDTH (VGAX_BWIDTH*4) //number of pixels in a row
#define VGAX_BSIZE (VGAX_BWIDTH*VGAX_HEIGHT) //size of framebuffer in bytes

//framebuffer. if you want you can write directly to this array. its safe
extern byte vgaxfb[VGAX_HEIGHT*VGAX_BWIDTH];

//clock replacement. this is increment in the VSYNC interrupt, so run at 60Hz
extern unsigned vtimer;

//VGAX class. This is a static class. Multiple instances will not work
class VGAX {
public:
  /*
   * begin()
   * end()
   *    NOTES: begin() method reconfigure TIMER0 TIMER1 and TIMER2.
   *    If you need to shutdown this library, you need to call end() and
   *    reconfigure all the three timers by yourself. The lib will not
   *    restore the previous timers configuration
   */
  static void begin();
  static void end();
  /*
   * putpixel(x, y, color)
   *    x: horizontal pixel coordinate. Must be less than VGAX_WIDTH
   *    y: vertical pixel coordinate. Must be less than VGAX_HEIGHT
   *    color: 2bit color. you must use only these values: 0 1 2 3
   */
  static inline void putpixel(byte x, byte y, byte color) {
    byte *p=vgaxfb + y*VGAX_BWIDTH + (x>>2);
    byte bitpos=6-(x & 3)*2;
    *p=(*p & ~(3 <<bitpos)) | color <<bitpos;
  }
  /*
   * clear(color)
   *    color: 2bit color to clear the framebuffer
   */
  static void clear(byte color);
  /*
   * copy(src)
   *    src: source data. src size must be equal to framebuffer
   */
  static void copy(byte *src);
  static void fillrect(byte x, byte y, byte width, byte height, byte color);
  /*
   * delay(msec)
   *    msec: number of milliseconds to wait
   */
  static void delay(int msec);
  /*
   * millis()
   *    return the number of milliseconds ellapsed
   */
  static inline unsigned millis() {
    return vtimer*16;
  }
  /*
   * micros()
   *    return the number of microseconds ellapsed  
   */
  static inline unsigned long micros() {
    return vtimer*16000;
  }
};
#endif
