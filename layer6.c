/*
 * Copyright (c) 1992, 1993 Arno Augustin, Frank Hoering, University of
 * Erlangen-Nuremberg, Germany.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	Erlangen-Nuremberg, Germany.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* all 'page' references refer to 'FTZ 157 D2 E' */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "layer6.h"
#include "layer2.h"
#include "xfont.h"
#include "control.h"
#include "font.h"
#include "attrib.h"

/* sys_errlist is deprecated. TODO: Use strerror instead. */
extern __const char *__const sys_errlist[];

/* exported variables */
// TODO: hook 
int tia = 0;    /* 1: white-on-black mode, no attributes */
int reveal = 0; /* 1: show hidden content */
int dirty;      /* screen change that requires a redraw */

/* local terminal status */
static struct {
   int cursorx, cursory;           /* cursor position (1 based)  */
   int cursor_on;
   int serialmode;                 /* 1=serial, 0=parallel       */
   int wrap;                       /* wraparound ?               */
   int par_attr;                   /* store all attributes       */
   int par_fg, par_bg;
   int G0123L[5];                  /* which set is addressed by G0, ... */
   int leftright[2];               /* actual lower/upper character set  */
   int lastchar;
   int sshift;
   int save_left;                  /* unload L set and invoke previous set  */
   int prim, supp;                 /* where is the prim set (service break) */
   int service_break;              /* (is the protect attr active ?)        */
   int drcs_bits, drcs_step;
   int drcs_w, drcs_h;
   int col_modmap;                 /* modify 1:colormap, 0:DCLUT */
   int clut;                       /* 0 based ! */
   int scroll_upper, scroll_lower;
   int scroll_impl, scroll_area;
   int hold_mosaic;
} t, backup;
struct screen_cell screen[24][40];   /* information for every character */
int rows, fontheight;                /* 20 rows(12 high) or 24 rows(10 high) */

/* local functions */
void LOG(const char *format, ...);
void xbtxerror(int perr, const char *format, ...);
static void default_sets(void);
static void do_US(void);
static int primary_control_C0(int c1);
static int do_CSI(void);
static void supplementary_control_C1(int c1, int fullrow);
static void do_ESC(void);
static void do_DRCS(void);
static void log_DRC(int c, char data[4][2*FONT_HEIGHT], int bits);
static void do_DRCS_data(int c);
static void do_DEFCOLOR(void);
static void do_DEFFORMAT(void);
static void do_RESET(void);
static void set_attr(int a, int set, int col, int mode);
static void output(int c);
static void redrawc(int x, int y);
static void updatec(int real, int x, int y);
static void update_DRCS_display(int start, int stop, int tep);
static void clearscreen(void);
static void scroll(int up);
//void do_TSW();

/*
 * initialize layer6 variables
 */
void init_layer6()
{
   int y;
   
   fontheight = 10;
   rows = 24;
   t.cursorx = t.cursory = 1;
   t.wrap = 1;
   t.service_break = 0;
   t.scroll_area = 0;
   t.scroll_impl = 1;
   t.cursor_on = 0;
   t.par_attr = 0;
   t.par_fg = WHITE;
   t.par_bg = TRANSPARENT;
   for(y=0; y<24; y++)  define_fullrow_bg(y, BLACK);
   clearscreen();
   default_sets();
}

   
/*
 * Read and process one layer6 code sequence.
 * Returns 1 when DCT (terminate data collect 0x1a) is received, else 0.
 */

int process_BTX_data()
{
   int set, c1, c2, dct=0;
   
   c1 = layer2_getc();

   if(     c1>=0x00 && c1<=0x1f)  dct = primary_control_C0(c1);
   else if(c1>=0x80 && c1<=0x9f)  supplementary_control_C1(c1, 0);
   else {
      if(t.sshift) set = t.G0123L[ t.sshift ];
      else         set = t.G0123L[ t.leftright[ (c1&0x80) >> 7 ] ];

      if( set == SUPP  &&  (c1 & 0x70) == 0x40 ) {   /* diacritical ??? */
	 LOG("diacritical mark %d\n", c1 & 0x0f);
	 c2 = layer2_getc();
	 if(c2&0x60) c1 = (c1<<8) | c2;
	 t.sshift = 0;
      }

      LOG("OUTPUT 0x%02x '%c'\n", c1&0xff, isprint(c1&0xff) ? c1&0xff : '.');
      output(c1);
      t.lastchar = c1;
   }
   
   return dct;
}


/*
 * initialize default character sets (page 113)
 */
static void default_sets()
{
   t.G0123L[G0] = PRIM;
   t.G0123L[G1] = SUP2;
   t.G0123L[G2] = SUPP;
   t.G0123L[G3] = SUP3;
   t.G0123L[ L] = L;       /* always L !!! (cannot be changed) */
   t.leftright[0] = G0;
   t.leftright[1] = G2;
   t.sshift = 0;
   t.save_left = G0;
   t.prim = G0;
   t.supp = G2;
}

/*
 * invert the character at the given location
 * in order to show/hide the cursor
 */
static void invert_cursor(int x, int y)
{
   xcursor(x, y, fontheight);
   dirty = 1;
}

/*
 * move the cursor, perform automatic wraparound (page 97)
 *                  and implicite scrolling (page 101)
 */
static void move_cursor(int cmd, int y, int x)
{
   int up=0, down=0;

   /* erase old cursor */
   if(t.cursor_on) invert_cursor(t.cursorx-1, t.cursory-1);

   /* move & wrap */
   switch(cmd) {
      case APF:
         if(++t.cursorx > 40) {
	    if(t.wrap) { t.cursorx-=40; down=1; }
            else       { t.cursorx=40; }
         }
	 break;
	 
      case APB:
         if(--t.cursorx < 1) {
	    if(t.wrap) { t.cursorx+=40; up=1; }
            else       { t.cursorx=1; }
         }
	 break;
	 
      case APU:  up=1;         break;
      case APD:  down=1;       break;
      case APR:  t.cursorx=1;  break;
	 
      case APA:
	 t.hold_mosaic = 0;
	 if(t.wrap) {   /* wrap !! (SKY-NET *200070000000004a#) */
	    if(x <  1)  { x += 40;  y--; }
	    if(x > 40)  { x -= 40;  y++; }
	    if(y <  1)    y += rows;
	    if(y > rows)  y -= rows;
	 }
	 else {
	    if(x <  1)  x =  1;
	    if(x > 40)  x = 40;
	    if(y <  1)  y =  1;
	    if(y > rows)  y = rows;
	 }
      
	 t.cursorx=x;
	 t.cursory=y;
	 break;
   }

   if(up) {
      t.hold_mosaic = 0;
      if(t.scroll_area && t.scroll_impl &&
	 t.cursory == t.scroll_upper)  scroll(0);
      else
         if(--t.cursory < 1) {
	    if(t.wrap)  t.cursory += rows;
	    else        t.cursory  = 1;
         }
   }
   
   if(down) {
      t.hold_mosaic = 0;
      if(t.scroll_area && t.scroll_impl &&
	 t.cursory == t.scroll_lower)  scroll(1);
      else
         if(++t.cursory > rows) {
	    if(t.wrap)  t.cursory -= rows;
	    else        t.cursory  = rows;
         }
   }

   /* draw new cursor */
   if(t.cursor_on) invert_cursor(t.cursorx-1, t.cursory-1);
}

      
/*
 * process a code from the primary control set C0 (0x00 - 0x1f).
 * Returns 1 when DCT is received, else 0.
 */

static int primary_control_C0(int c1)  /* page 118, annex 6 */
{
   int c2, x, y;
   
   switch(c1) {

      case APB:
         LOG("APB active position back\n");
         move_cursor(APB, -1, -1);
         break;
         
      case APF:
         LOG("APF active position forward\n");
         move_cursor(APF, -1, -1);
         break;

      case APD:
         LOG("APD active position down\n");
         move_cursor(APD, -1, -1);
         break;

      case APU:
         LOG("APU active position up\n");
         move_cursor(APU, -1, -1);
         break;

      case CS:
         LOG("CS  clear screen\n");
	 t.leftright[0] = t.save_left;
	 clearscreen();
         break;

      case APR:
         LOG("APR active position return\n");
         move_cursor(APR, -1, -1);
         break;

      case LS1:
      case LS0:
         c2 = (c1==LS1) ? 1 : 0;
         LOG("LS%d locking shift G%d left\n", c2, c2);
         t.leftright[0] = c2;  /* G0 or G1 !! */
	 t.save_left = c2;
         break;

      case CON:
         LOG("CON cursor on\n");
	 if(!t.cursor_on) {
	    t.cursor_on = 1;
	    invert_cursor(t.cursorx-1, t.cursory-1);
	 }
         break;

      case RPT:
         LOG("RPT repeat last char\n");
         c2 = layer2_getc() & 0x3f;
         LOG("    %d times\n", c2);
         while(c2--)  output(t.lastchar);
         break;

      case COF:
         LOG("COF cursor off\n");
	 if(t.cursor_on) {
	    t.cursor_on = 0;
	    invert_cursor(t.cursorx-1, t.cursory-1);
	 }
         break;

      case CAN:
         LOG("CAN cancel\n");
	 y = t.cursory-1;
	 screen[y][t.cursorx-1].chr = ' ';
	 screen[y][t.cursorx-1].set = PRIM;
	 for(x=t.cursorx; x<40; x++) {  /* clear to the right */
	    screen[y][x].chr    = ' ';
	    screen[y][x].set    = PRIM;
	    screen[y][x].mark   = 0;
	    screen[y][x].attr   = screen[y][t.cursorx-1].attr;
	    screen[y][x].fg     = screen[y][t.cursorx-1].fg;
	    screen[y][x].bg     = screen[y][t.cursorx-1].bg;
	 }
	 for(x=t.cursorx-1; x<40; x++)  redrawc(x+1, y+1);
         break;

      case SS2:
         LOG("SS2 single shift G2 left\n");
         t.sshift = G2;
         break;

      case ESC:
         LOG("ESC escape sequence\n");
         do_ESC();
         break;

      case SS3:
         LOG("SS3 single shift G3 left\n");
         t.sshift = G3;
         break;

      case APH:
         LOG("APH active position home\n");
	 move_cursor(APA, 1, 1);
         t.par_attr = 0;
	 t.par_fg = WHITE;
	 t.par_bg = TRANSPARENT;
         break;

      case US:
         LOG("US  unit separator (or APA)\n");
         do_US();
         break;

      default:
         LOG("??? unprocessed control character 0x%02x - ignored\n", c1);
	 if(c1 == DCT)  return 1;
   }

   return 0;
}


/*
 * process a code from the supplementary control set C1 (0x80 - 0x9f).
 * 'fullrow' indicates attribute application to the complete row.
 * Most codes advance active cursor position by one !
 */

static void supplementary_control_C1(int c1, int fullrow)  /* page 121, annex 6 */
{
   int adv = 0, mode = fullrow ? 2 : t.serialmode;

   switch(c1) {
                     /* serial   parallel */
      case 0x80:     /*  ABK       BKF    */
      case 0x81:     /*  ANR       RDF    */
      case 0x82:     /*  ANG       GRF    */
      case 0x83:     /*  ANY       YLF    */
      case 0x84:     /*  ANB       BLF    */
      case 0x85:     /*  ANM       MGF    */
      case 0x86:     /*  ANC       CNF    */
      case 0x87:     /*  ANW       WHF    */
         LOG("set foreground to color #%d %s\n", t.clut*8+c1-0x80,
	     (mode==1) ? "(+ unload L set)" : "");
	 set_attr(ATTR_FOREGROUND, 1, t.clut*8+c1-0x80, mode);
	 if(mode==1) {
	    t.leftright[0] = t.save_left;
	 }
         break;

      case FSH:
         LOG("FSH flashing begin\n");
	 /* set_attr(ATTR_FLASH, 1, 0, mode); */
         break;

      case STD:
         LOG("STD flashing steady\n");
	 /* set_attr(ATTR_FLASH, 0, 0, mode); */
         break;

      case EBX:
         LOG("EBX end of window\n");
	 /* set_attr(ATTR_WINDOW, 0, 0, mode); */
         break;

      case SBX:
         LOG("SBX start of window\n");
	 /* set_attr(ATTR_WINDOW, 1, 0, mode); */
         break;

      case NSZ:
         LOG("NSZ normal size\n");
	 set_attr(ATTR_NODOUBLE, 1, 0, mode);
         break;

      case DBH:
         LOG("DBH double height\n");
	 set_attr(ATTR_YDOUBLE, 1, 0, mode);
         break;

      case DBW:
         LOG("DBW double width\n");
	 set_attr(ATTR_XDOUBLE, 1, 0, mode);
         break;

      case DBS:
         LOG("DBS double size\n");
	 set_attr(ATTR_XYDOUBLE, 1, 0, mode);
         break;

                     /* serial   parallel */
      case 0x90:     /*  MBK       BKB    */
      case 0x91:     /*  MSR       RDB    */
      case 0x92:     /*  MSG       GRB    */
      case 0x93:     /*  MSY       YLB    */
      case 0x94:     /*  MSB       BLB    */
      case 0x95:     /*  MSM       MGB    */
      case 0x96:     /*  MSC       CNB    */
      case 0x97:     /*  MSW       WHB    */
	 /* at fullrow control the parallel set is used ! */
         LOG("set %s to color #%d\n", (mode==1) ?
	     "mosaic foreground (+ invoke L set)" : "background",
	     t.clut*8+c1-0x90);
	 if(mode==1) {
	    set_attr(ATTR_FOREGROUND, 1, t.clut*8+c1-0x90, 1);
	    t.save_left = t.leftright[0];
	    t.leftright[0] = L;
	 }
	 else set_attr(ATTR_BACKGROUND, 1, t.clut*8+c1-0x90, mode);
         break;

      case CDY:
         LOG("CDY conceal display\n");
	 set_attr(ATTR_CONCEALED, 1, 0, mode);
         break;

      case SPL:
         LOG("SPL stop lining\n");
	 set_attr(ATTR_UNDERLINE, 0, 0, mode);
         break;

      case STL:
         LOG("STL start lining\n");
	 set_attr(ATTR_UNDERLINE, 1, 0, mode);
         break;

      case CSI:
         LOG("CSI control sequence introducer\n");
         adv = do_CSI();
         break;

      case 0x9c:
         if(mode==1) {
            LOG("BBD black background\n");
	    set_attr(ATTR_BACKGROUND, 1, t.clut*8+BLACK, 1);
         } else {
            LOG("NPO normal polarity\n");
	    set_attr(ATTR_INVERTED, 0, 0, mode);
         }
         break;

      case 0x9d:
         if(mode==1) {
            LOG("NBD new background\n");
	    set_attr(ATTR_BACKGROUND, 1,
		     screen[t.cursory-1][t.cursorx-1].fg, 1);
         } else {
            LOG("IPO inverted polarity\n");
	    set_attr(ATTR_INVERTED, 1, 0, mode);
         }
         break;

      case 0x9e:
         if(mode==1) {
            LOG("HMS hold mosaic\n");
	    t.hold_mosaic = 1;
         } else {
            LOG("TRB transparent background\n");
	    set_attr(ATTR_BACKGROUND, 1, TRANSPARENT, mode);
         }
         break;

      case 0x9f:
         if(mode==1) {
            LOG("RMS release mosaic\n");
	    t.hold_mosaic = 0;
         } else {
            LOG("STC stop conceal\n");
	    set_attr(ATTR_CONCEALED, 0, 0, mode);
         }
         break;
   }

   /* serial attribute controls advance cursor 1 char forwards !  (page 90) */
   if(mode==1 && (c1!=CSI || adv) ) {
      if(t.hold_mosaic)  output(t.lastchar);  /* HMS/RMS (page 96) */
      else               move_cursor(APF, -1, -1);
   }
}


static void do_US()  /* page 85/86 */
{
   static unsigned char TFI_string[] = { SOH, US, 0x20, 0x7f, 0x40, ETB };
   int c2, c3, alphamosaic = 0;

   /* implicite return from service break !!! (any US sequence !?!) */
   if(t.service_break) {
      t = backup;
      move_cursor(APA, t.cursory, t.cursorx);
   }
   
   c2 = layer2_getc();

   switch(c2) {

      case 0x20:  /* annex 7.3 */
	 LOG("    TFI Terminal Facility Identifier\n");
	 c3 = layer2_getc();
	 if(c3==0x40) {
	    LOG("       TFI request\n");
	    layer2_write(TFI_string, 6);
	 }
	 else {
	    LOG("       TFI echo 0x%02x\n", c3);
	    do {
	       c3 = layer2_getc();
	       LOG("       TFI echo 0x%02x\n", c3);
	    }
	    while(c3 & 0x20);  /* extension bit */
	 }
	 break;
	 
      case 0x23:
         LOG("    define DRCS\n");
         do_DRCS();
         break;

      case 0x26:
         LOG("    define color\n");
         do_DEFCOLOR();
         break;

      case 0x2d:  /* page 155 */
         LOG("    define Format\n");
         do_DEFFORMAT();
         break;

      case 0x2f:  /* page 157 */
         LOG("    Reset sequence\n");
         do_RESET();
	 alphamosaic = 1;
         break;

      case 0x3e:  /* annex 7.4 */
	 LOG("    Telesoftware\n");
#if 0 // TODO: add this
	 do_TSW();
#endif
	 break;
	 
      default:    /* APA active position addressing */
	 if(c2<0x40)  LOG("    unknown US sequence\n");
	 else {
	    alphamosaic = 1;
	    LOG("    new row    %2d\n", c2 & 0x3f);
	    c3 = layer2_getc();
	    LOG("    new column %2d\n", c3 & 0x3f);
	    move_cursor(APA, c2 & 0x3f, c3 & 0x3f);
	    t.par_attr = 0;
	    t.par_fg = WHITE;
	    t.par_bg = TRANSPARENT;
	 }
         break;
   }

   /* VPDE = US x data. Each VPDE has to be followed by the next VPDE
    * immediately (page 85). The alphamosaic VPDE is introduced by APA or
    * by one of the reset functions ! Any remaining data (errors) after all
    * other VPDE's has to be skipped (*17420101711a#).
    */
   if(!alphamosaic) {
      while( (c2 = layer2_getc()) != US )  LOG("skipping to next US\n");
      LOG("\n");
      layer2_ungetc();
   }
}
         

static void do_ESC()
{
   int y, c2, c3, c4;
   
   c2 = layer2_getc();

   switch(c2) {

      case 0x22:
         LOG("    invoke C1\n");
         c3 = layer2_getc();
         LOG("       (%s)\n", c3==0x40 ? "serial" : "parallel");
	 if(c3==0x40)  t.serialmode = 1;
	 else {
	    t.serialmode = 0;
	    t.leftright[0] = t.save_left;
	 }
         break;

      case 0x23:
         LOG("    set attributes\n");
         c3 = layer2_getc();
         switch(c3) {
            case 0x20:
               LOG("       full screen background\n");
               c4 = layer2_getc();
               LOG("          color = %d\n",
		   c4==0x5e ? TRANSPARENT : t.clut*8+c4-0x50);
	       for(y=0; y<24; y++)
	          define_fullrow_bg(y, c4==0x5e ?
				    TRANSPARENT : t.clut*8+c4-0x50);
               redraw_screen_rect(0, 0, 39, rows-1);
               break;
            case 0x21:
               LOG("       full row\n");
               c4 = layer2_getc();
	       LOG("          ");
	       supplementary_control_C1(c4+0x40, 1);
               break;
         }
         break;

      case 0x28:
      case 0x29:
      case 0x2a:
      case 0x2b:
         LOG("    load G%d with\n", c2 - 0x28);
         c3 = layer2_getc();
         switch(c3) {
            case 0x40:
               LOG("       'primary graphic'\n");
               t.G0123L[c2 - 0x28] = PRIM;
               t.prim = c2 - 0x28;
               break;
            case 0x62:
               LOG("       'supplementary graphic'\n");
               t.G0123L[c2 - 0x28] = SUPP;
               t.supp = c2 - 0x28;
               break;
            case 0x63:
               LOG("       '2nd supplementary mosaic'\n");
               t.G0123L[c2 - 0x28] = SUP2;
               break;
            case 0x64:
               LOG("       '3rd supplementary mosaic'\n");
               t.G0123L[c2 - 0x28] = SUP3;
               break;
            case 0x20:
	       LOG("       DRCS\n");
               c4 = layer2_getc();
               if(c4 != 0x40)  LOG("HAEH  (ESC 0x%02x 0x20 0x%02x)\n", c2, c4);
	       else            LOG("\n");
               t.G0123L[c2 - 0x28] = DRCS;
               break;
         }
         break;               
      
      case 0x6e:
         LOG("    LS2 locking shift G2 left\n");
         t.leftright[0] = G2;
	 t.save_left = G2;
         break;

      case 0x6f:
         LOG("    LS3 locking shift G3 left\n");
         t.leftright[0] = G3;
	 t.save_left = G3;
         break;

      case 0x7c:
         LOG("    LS3R locking shift G3 right\n");
         t.leftright[1] = G3;
         break;

      case 0x7d:
         LOG("    LS2R locking shift G2 right\n");
         t.leftright[1] = G2;
         break;

      case 0x7e:
         LOG("    LS1R locking shift G1 right\n");
         t.leftright[1] = G1;
         break;
   }
}


/*
 * Process one control sequence. Return whether or not the cursor position
 * has to be advanced by one char (in case of serialmode).
 *
 * Anscheinend sollen nur die FLASH-controls den Cursor eins weiterstellen -
 * steht zwar nirgends, sieht aber am besten aus !!!
 */

static int do_CSI()
{
   int c2, c3, upper, lower;
   
   c2 = layer2_getc();
   if(c2 == 0x42) {
      LOG("    STC stop conceal\n");
      set_attr(ATTR_CONCEALED, 0, 0, t.serialmode);
      return 0;
   }
   
   LOG("\n");
   c3 = layer2_getc();

   /* protection only available as fullrow controls ?? (page 135) */
   if(c2 == 0x31 && c3 == 0x50) {
      LOG("       PMS protected mode start\n");
      set_attr(ATTR_PROTECTED, 1, 0, 2);
      return 0;
   }
   if(c2 == 0x31 && c3 == 0x51) {
      LOG("       PMC protected mode cancel\n");
      set_attr(ATTR_PROTECTED, 0, 0, 2);
      return 0;
   }
   if(c2 == 0x32 && c3 == 0x53) {
      LOG("       MMS marked mode start\n");
      /* set_attr(ATTR_MARKED, 1, 0, t.serialmode); */
      return 0;
   }
   if(c2 == 0x32 && c3 == 0x54) {
      LOG("       MMT marked mode stop\n");
      /* set_attr(ATTR_MARKED, 0, 0, t.serialmode); */
      return 0;
   }
   
   switch(c3) {

      case 0x40:
         LOG("       invoke CLUT%d\n", c2 - 0x2f);
	 t.clut = c2 - 0x30;
         return 0;

      case 0x41:
         switch(c2) {
            case 0x30:
               LOG("       IVF inverted flash\n");
               return 1;
            case 0x31:
               LOG("       RIF reduced intesity flash\n");
               return 1;
            case 0x32:
            case 0x33:
            case 0x34:
               LOG("       FF%c fast flash %c\n", c2-1, c2-1);
               return 1;
            case 0x35:
               LOG("       ICF increment flash\n");
               return 1;
            case 0x36:
               LOG("       DCF decrement flash\n");
               return 1;
         }
         break;

      case 0x60:
         switch(c2) {
            case 0x30:
               LOG("       SCU explicit scroll up\n");
	       if(t.scroll_area) scroll(1);
               return 0;
            case 0x31:
               LOG("       SCD explicit scroll down\n");
	       if(t.scroll_area) scroll(0);
               return 0;
            case 0x32:
               LOG("       AIS activate implicite scrolling\n");
	       t.scroll_impl = 1;
               return 0;
            case 0x33:
               LOG("       DIS deactivate implicite scrolling\n");
	       t.scroll_impl = 0;
               return 0;
         }
         break;

      default:  /* definition of scrolling area (page 137) */
	 upper = c2 & 0x0f;
	 if(c3>=0x30 && c3<=0x39)  upper = upper*10 + (c3&0x0f);
	 LOG("       upper row: %2d\n", upper);
	 if(c3>=0x30 && c3<=0x39)  { c3 = layer2_getc(); LOG("\n"); }

/*	 if(c3!=0x3b)  fprintf(stderr, "XCEPT: scrolling area - protocol !\n");
*/
	 lower = layer2_getc() & 0x0f;
	 LOG("\n");
	 c3 = layer2_getc();
	 if(c3>=0x30 && c3<=0x39)  lower = lower*10 + (c3&0x0f);
	 LOG("       lower row: %2d", lower);
	 if(c3>=0x30 && c3<=0x39)  { LOG("\n"); c3=layer2_getc(); LOG("    "); }
	 
	 if(c3==0x55) {
	    LOG("   CSA create scrolling area\n");
	    if(upper>=2 && lower<rows && lower>=upper) {
	       t.scroll_upper = upper;
	       t.scroll_lower = lower;
	       t.scroll_area = 1;
	    }
	 }
	 if(c3==0x56) {
	    LOG("   CSD delete scrolling area\n");
	    t.scroll_area = 0;
	 }
         return 0;
   }

   return 0;  /* not reached, avoid warnings */
}


static void do_DRCS()   /* (page 139ff) */
{
   int c3, c4, c5, c6, c7, c8;

   c3 = layer2_getc();
   if(c3 == 0x20) {
      LOG("       DRCS header unit\n");
      c4 = layer2_getc();
      if(c4==0x20 || c4==0x28) {
	 LOG("          %s existing DRCS\n", (c4==0x20) ? "keep" : "delete");
	 if(c4==0x28)  free_DRCS();
	 c5 = layer2_getc();
      } else c5 = c4;
      if(c5 == 0x20) {
	 LOG("\n");
	 c6 = layer2_getc();
      } else c6 = c5;
      if(c6 == 0x40) {
	 LOG("\n");
	 c7 = layer2_getc();
      } else c7 = c6;

      switch(c7 & 0xf) {
         case 6:
	    LOG("          12x12 pixel\n");
	    t.drcs_w = 12;
	    t.drcs_h = 12;
	    break;
	 case 7:
	    LOG("          12x10 pixel\n");
	    t.drcs_w = 12;
	    t.drcs_h = 10;
	    break;
	 case 10:
	    LOG("          6x12 pixel\n");
	    t.drcs_w = 6;
	    t.drcs_h = 12;
	    break;
	 case 11:
	    LOG("          6x10 pixel\n");
	    t.drcs_w = 6;
	    t.drcs_h = 10;
	    break;
	 case 12:
	    LOG("          6x5 pixel\n");
	    t.drcs_w = 6;
	    t.drcs_h = 5;
	    break;
	 case 15:
	    LOG("          6x6 pixel\n");
	    t.drcs_w = 6;
	    t.drcs_h = 6;
	    break;
      }

      c8 = layer2_getc();
      LOG("          %d bit/pixel\n", c8 & 0xf);
      t.drcs_bits = c8 & 0xf;
      t.drcs_step = (t.drcs_h>=10 && t.drcs_w*t.drcs_bits==24)  ?  2 : 1;
   }
   else {
      LOG("       DRCS pattern transfer unit (char: 0x%02x)\n", c3);
      do_DRCS_data(c3);
   }
}


/*
 * load and define dynamic characters, first character is 'c'.
 * more than one bitplane of a character may be loaded simultaneously !
 */

static void do_DRCS_data(int c)
{
   static char data[4][2*FONT_HEIGHT];
   int c4, i, n, planes=0, planemask=0, byte=0;
   int maxbytes, start = c;

   maxbytes = 2*t.drcs_h;
   if(t.drcs_h<10)  maxbytes *= 2;
   
   do {
      c4 = layer2_getc();

      switch(c4) {
         case 0x20:  /* S-bytes */
         case 0x2f:
	    LOG("          fill rest of char with %d\n", c4 & 1);
	    for(; byte<maxbytes; byte++)
	       for(n=0; n<4; n++)
	          if(planemask & (1<<n)) data[n][byte] = (c4==0x20) ? 0 : 0xff;
	    break;
	 case 0x21:
	 case 0x22:
	 case 0x23:
	 case 0x24:
	 case 0x25:
	 case 0x26:
	 case 0x27:
	 case 0x28:
	 case 0x29:
	 case 0x2a:
	    LOG("          repeat last row %d times\n", c4 & 0xf);
	    if(byte&1) byte++;   /* pad to full row (assume 0) */
            for(i=0; i<(c4 & 0xf); i++) {
	       for(n=0; n<4; n++)
	          if(planemask & (1<<n)) {
		     data[n][byte]   = byte ? data[n][byte-2] : 0;
		     data[n][byte+1] = byte ? data[n][byte-2+1] : 0;
		     if(t.drcs_h<10) {
			data[n][byte+2] = byte ? data[n][byte-2] : 0;
			data[n][byte+3] = byte ? data[n][byte-2+1] : 0;
		     }
		  }
	       byte += 2;
	       if(t.drcs_h<10)  byte += 2;
	    }
            break;
	 case 0x2c:
	 case 0x2d:
	    LOG("          full row %d\n", c4 & 1);
	    if(byte&1) byte++;   /* pad to full row (assume 0) */
            for(n=0; n<4; n++)
	       if(planemask & (1<<n)) {
		  data[n][byte]   = (c4==0x2c) ? 0 : 0xff;
		  data[n][byte+1] = (c4==0x2c) ? 0 : 0xff;
		  if(t.drcs_h<10) {
		     data[n][byte+2] = (c4==0x2c) ? 0 : 0xff;
		     data[n][byte+3] = (c4==0x2c) ? 0 : 0xff;
		  }
	       }
            byte += 2;
	    if(t.drcs_h<10)  byte += 2;
            break;
	 case 0x2e:
	    LOG("          fill rest of char with last row\n");
	    if(byte&1) byte++;   /* pad to full row (assume 0) */
            while(byte<maxbytes) {
	       for(n=0; n<4; n++)
	          if(planemask & (1<<n)) {
		     data[n][byte]   = data[n][byte-2];
		     data[n][byte+1] = data[n][byte-2+1];
		  }
	       byte += 2;
	    }
            break;

         case 0x30:  /* B-bytes */
         case 0x31:
         case 0x32:
         case 0x33:
	    if(byte) {
	       LOG("          new plane ahead - filling up\n");
	       byte = maxbytes;   /* pad to full plane */
	       layer2_ungetc();
	    }
	    else {
	       LOG("          start of pattern block (plane %d)\n", c4 & 0xf);
	       for(i=0; i<2*FONT_HEIGHT; i++)  data[c4 & 0xf][i] = 0;
	       planemask |= 1 << (c4 & 0xf);
	    }
	    break;

	 default:
            if(c4<0x20 || c4>0x7f) {
	       LOG("          end of pattern data\n");
	       layer2_ungetc();
	       if(byte)  byte = maxbytes;
	    }
	    else {   /* D-bytes */
	       LOG("          pattern data\n");
	       if(t.drcs_w==6) {   /* low res */
		  for(n=0; n<4; n++)
		     if(planemask & (1<<n))  {
			data[n][byte] =
		         ((c4&32)?0x30:0) | ((c4&16)?0x0c:0) | ((c4&8)?0x03:0);
			data[n][byte+1] =
		         ((c4&4)?0x30:0) | ((c4&2)?0x0c:0) | ((c4&1)?0x03:0);
		     }
		  byte += 2;
	       }
	       else {
		  for(n=0; n<4; n++)
		     if(planemask & (1<<n))  data[n][byte] = c4 & 0x3f;
		  byte++;
	       }

	       if(!(byte&1) && t.drcs_h<10) {  /* duplicate row ? */
		  for(n=0; n<4; n++)
	             if(planemask & (1<<n)) {
			data[n][byte]   = data[n][byte-2];
			data[n][byte+1] = data[n][byte-2+1];
		     }
		  byte += 2;
	       }
	    }
	    break;
	    
      } /* switch */

      if(byte == maxbytes) {  /* plane is complete */
	 for(n=0; n<4; n++)  if(planemask & (1<<n))  planes++;
	 planemask = 0;
	 byte = 0;
	 if(planes == t.drcs_bits) {  /* DRC is complete */
	    log_DRC(c, data, t.drcs_bits);
	    define_raw_DRC(c, data, t.drcs_bits);
	    planes = 0;
	    c += t.drcs_step;
	 }
      }

   } while(c4>=0x20 && c4<=0x7f);

   update_DRCS_display(start, c, t.drcs_step);
}


/*
 * pretty print the received character to the log file
 */
static void log_DRC(int c, char data[4][2*FONT_HEIGHT], int bits)
{
   int n, i, j, k;
   
   LOG("\n                      DRC # 0x%2x\n", c);
   for(n=0; n<bits; n++) {
      LOG("                      (plane %d)\n", n);
      LOG("                      --------------------------\n");
      for(j=0; j<fontheight; j++) {
	 LOG("                      |");
	 for(k=0; k<2; k++)
	    for(i=5; i>=0; i--)
	       if(data[n][j*2+k] & (1<<i)) LOG("* ");
	       else                        LOG("  ");
	 LOG("|\n");
      }
      LOG("                      --------------------------\n\n");
   }
}


/*
 * load and define dynamic colors.
 */
static void do_DEFCOLOR()  /* (page 150ff) */
{
   int c3, c4, c5, c6, c7, index, r, g, b;

   c3 = layer2_getc();

   switch(c3) {

      case 0x20:   /*  US 0x26 0x20 <ICT> <SUR> <SCM>  */
         LOG("       color header unit\n");
         t.col_modmap = 1;    /* by default modify colormap */
         c4 = layer2_getc();
         if((c4 & 0xf0) == 0x20) {
	    if(c4!=0x20 && c4!=0x22) {
	       LOG("*** <ICT>: bad value !\n");
	       fprintf(stderr, "XCEPT: do_DEFCOLOR(): ICT1 = 0x%02x\n", c4);
	    }
            LOG("          <ICT>: load %s\n", c4==0x20 ? "colormap" : "DCLUT");
	    t.col_modmap = (c4==0x20);
            c5 = layer2_getc();
         } else c5 = c4;
         if((c5 & 0xf0) == 0x20) {
            LOG("          <ICT>: (unit %d)\n", c5&0xf);
	    if(c5!=0x20) {
	       LOG("*** <ICT>: bad value !\n");
	       fprintf(stderr, "XCEPT: do_DEFCOLOR(): ICT2 = 0x%02x\n", c5);
	    }
            c6 = layer2_getc();
         } else c6 = c5;
         if((c6 & 0xf0) == 0x30) {
            LOG("          <SUR>: %d bits\n", c6&0xf);
	    if(c6!=0x34 && c6!=0x35) {
	       LOG("*** <SUR>: bad value !\n");
	       fprintf(stderr, "XCEPT: do_DEFCOLOR(): SUR = 0x%02x\n", c6);
	    }
            c7 = layer2_getc();
         } else c7 = c6;
         if((c7 & 0xf0) == 0x40) {
            LOG("          <SCM>: 0x%02x\n", c7);
	    if(c7!=0x40 && c7!=0x41) {
	       LOG("*** <SCM>: bad value !\n");
	       fprintf(stderr, "XCEPT: do_DEFCOLOR(): SCM = 0x%02x\n", c7);
	    }
         } else {
            LOG("          default header\n");
            layer2_ungetc();
         }
         break;

      case 0x21:
         LOG("       color reset unit\n");
	 default_colors();
         break;

      default:
         LOG("       color transfer unit  (1.Stelle: %d)\n", c3&0xf);
	 index = c3&0xf;
         c4 = layer2_getc();
         if((c4 & 0xf0) == 0x30) { /* c3 zehner, c4 einer */
            LOG("                            (2.Stelle: %d)\n", c4&0xf);
	    index = (c3&0xf)*10 + (c4&0xf);
            c5 = layer2_getc();
         } else c5 = c4;

	 if(t.col_modmap) {  /* load colormap */
	    while(c5>=0x40 && c5<=0x7f) {
	       LOG("          color #%2d:  R G B\n", index);
	       c6 = layer2_getc();
	       r = (c5&0x20)>>2 | (c5&0x04)    | (c6&0x20)>>4 | (c6&0x04)>>2;
	       g = (c5&0x10)>>1 | (c5&0x02)<<1 | (c6&0x10)>>3 | (c6&0x02)>>1;
	       b = (c5&0x08)    | (c5&0x01)<<2 | (c6&0x08)>>2 | (c6&0x01);
	       LOG("                      %1x %1x %1x\n", r, g, b);
	       if(index>=16 && index<=31)  define_color(index++, r, g, b);
	       c5 = layer2_getc();
	    }
	 }
	 else {  /* load DCLUT */
	    while(c5>=0x40 && c5<=0x7f) {
	       LOG("          DCLUT[%2d] = %2d\n", index, c5&0x1f);
	       if(index>=0 && index<=3)  define_DCLUT(index++, c5&0x1f);
	       c5 = layer2_getc();
	    }
	 }

	 LOG("          end of color data\n");
         layer2_ungetc();
         break;
   }
}

   
static void do_DEFFORMAT()   /* format designation  (page 155) */
{
   int c3, c4;

   rows = 24;
   fontheight = 10;
   t.wrap  = 1;
   
   c3 = layer2_getc();
   if((c3&0xf0) == 0x40) {
      switch(c3) {
         case 0x41:
            LOG("       40 columns by 24 rows\n");
	    break;
         case 0x42:
	    LOG("       40 columns by 20 rows\n");
	    rows = 20;
	    fontheight = 12;
	    break;
         default:
	    LOG("       unrecognized format (using default)\n");
	    break;
      }
      c4 = layer2_getc();
   }
   else c4 = c3;
   
   if((c4&0xf0) == 0x70) {
      LOG("       wraparound %s\n", (c3&1) ? "inactive" : "active");
      t.wrap = (c3 == 0x70) ? 1 : 0;
   }
   else {
      LOG("       default format\n");
      layer2_ungetc();
   }
}


static void do_RESET()  /* reset functions  (page 157) */
{
   int y, c3, c4;

   c3 = layer2_getc();

   switch(c3) {

      case 0x40:   /* (page 158) */
         LOG("       service break to row\n");
         c4 = layer2_getc();
         LOG("          #%d\n", c4 & 0x3f);
         backup = t;  /* structure copy */
         t.leftright[0] = t.prim;  /* PFUSCH !!! */
         t.leftright[1] = t.supp;
	 t.save_left = t.prim;
         t.wrap = 0;
	 t.cursor_on = 0;
	 move_cursor(APA, c4 & 0x3f, 1);
         t.serialmode = 1;
	 t.clut = 0;
	 t.service_break = 1;
         break;

      case 0x41:
      case 0x42:
         LOG("       defaults (%s C1)\n", c3&1 ? "serial" : "parallel");
         default_sets();
         t.serialmode = c3 & 1;
         t.wrap = 1;
	 t.cursor_on = 0;
	 rows = 24;
	 fontheight = 10;
	 for(y=0; y<24; y++)  define_fullrow_bg(y, BLACK);
         clearscreen();  /* clearscr resets CLUT, par_attr, cursor pos */
         break;

      case 0x43:
      case 0x44:
         LOG("       limited defaults (%s C1)\n", c3&1 ? "serial":"parallel");
         default_sets();
         t.serialmode = c3 & 1;
         break;

      case 0x4f:
         LOG("       reset to previous state\n");
         t = backup;
	 move_cursor(APA, t.cursory, t.cursorx);
         break;
   }
}


/*
 * set/reset parallel, serial or fullrow attributes (+ set/delete markers)
 * (mode: 0=parallel, 1=serial, 2=fullrow)
 *
 * MARKER:  (page 91 / annex 1, page 29)
 *
 *   parallel: (set in output())
 *      set wherever an attr is changed
 *      set at attr changes in writing a continuous string
 *      delete existing markers in overwritten part of "
 *      delete when printing and attrs do not change
 *
 *   serial:   (set here)
 *      set at this position
 */

static void set_attr(int a, int set, int col, int mode)
{
   int x, y = t.cursory-1, refresh, mattr = a;

   /* set fullrow background */
   if(mode==2 && a==ATTR_BACKGROUND) {
      define_fullrow_bg(y, col);
      redraw_screen_rect(0, y, 39, y);
      return;
   }

   if(a & ATTR_ANYSIZE)  mattr = ATTR_SIZE;

   /* serial mode controls set a marker */
   if(mode==1)  screen[y][t.cursorx-1].mark |= mattr;

   /* serial/fullrow controls apply to parts of the row */
   if(mode) {  /* serial || fullrow */

      /* receiption of serial DBS/DBH in last row of scrolling area        */
      /* forces a scroll up + a cursor up before writing (page 101) */
      if( (t.service_break || !(screen[y][0].attr & ATTR_PROTECTED))  &&
	 (a==ATTR_YDOUBLE || a==ATTR_XYDOUBLE) &&
	 t.scroll_area && t.cursory==t.scroll_lower) {
	 scroll(1);
	 move_cursor(APU, -1, -1);
	 y = t.cursory-1;
      }
      
      x = (mode==2) ? 0 : t.cursorx-1;

      do {
	 refresh = 0;
	 
	 /* fullrow controls delete the marker in the complete row */
	 if(mode==2)  screen[y][x].mark &= ~mattr;

	 if( t.service_break || !(screen[y][x].attr & ATTR_PROTECTED) ||
	    (a==ATTR_PROTECTED && set==0) ) {
	    switch(a) {
	       case ATTR_NODOUBLE:
	          if(screen[y][x].attr & (ATTR_XDOUBLE|ATTR_YDOUBLE))
		     refresh = 1;
		  screen[y][x].attr &= ~(ATTR_XDOUBLE|ATTR_YDOUBLE);
		  break;
	       case ATTR_XYDOUBLE:
		  if((screen[y][x].attr & (ATTR_XDOUBLE|ATTR_YDOUBLE)) !=
		     (ATTR_XDOUBLE|ATTR_YDOUBLE) )  refresh = 1;
		  screen[y][x].attr |= (ATTR_XDOUBLE|ATTR_YDOUBLE);
		  break;
	       case ATTR_XDOUBLE:
		  if((screen[y][x].attr & (ATTR_XDOUBLE|ATTR_YDOUBLE)) !=
		     ATTR_XDOUBLE )  refresh = 1;
		  screen[y][x].attr &= ~ATTR_YDOUBLE;
		  screen[y][x].attr |= ATTR_XDOUBLE;
		  break;
	       case ATTR_YDOUBLE:
		  if((screen[y][x].attr & (ATTR_XDOUBLE|ATTR_YDOUBLE)) !=
		     ATTR_YDOUBLE )  refresh = 1;
		  screen[y][x].attr &= ~ATTR_XDOUBLE;
		  screen[y][x].attr |= ATTR_YDOUBLE;
		  break;
	       case ATTR_FOREGROUND:
		  if(screen[y][x].fg!=col && screen[y][x].chr!=' ') refresh=1;
		  screen[y][x].fg = col;
		  break;
	       case ATTR_BACKGROUND:
		  if(screen[y][x].bg != col)  refresh = 1;
		  screen[y][x].bg = col;
		  break;
	       
	       default:
		  if( ((screen[y][x].attr & a)>0) != set )  refresh = 1;
		  if(set)  screen[y][x].attr |= a;
		  else     screen[y][x].attr &= ~a;
		  break;
	    } /* switch */

	    if(refresh)  redrawc(x+1, y+1);

	    /* (DBH/DBS chars must not cross borders of a protected area) */
	    if(a==ATTR_PROTECTED && y && realattr(y, x+1, ATTR_YDOUBLE) &&
	       attrib(y, x+1, a) != attrib(y+1, x+1, a) )  redrawc(x+1, y);

	 } /* if !protected */

	 x++;
      }         /* while x<40 && (serialmode --> no marker set) */
      while(x<40  && (mode!=1 || !(screen[y][x].mark & mattr)) );

   }
   else {  /* parallel */
      switch(a) {
         case ATTR_NODOUBLE:
            t.par_attr &= ~(ATTR_XDOUBLE|ATTR_YDOUBLE);
	    break;
	 case ATTR_XYDOUBLE:
	    t.par_attr |= (ATTR_XDOUBLE|ATTR_YDOUBLE);
	    break;
	 case ATTR_XDOUBLE:
	    t.par_attr &= ~ATTR_YDOUBLE;
            t.par_attr |= ATTR_XDOUBLE;
	    break;
	 case ATTR_YDOUBLE:
	    t.par_attr &= ~ATTR_XDOUBLE;
            t.par_attr |= ATTR_YDOUBLE;
	    break;
         case ATTR_FOREGROUND:
	    t.par_fg = col;
	    break;
	 case ATTR_BACKGROUND:
	    t.par_bg = col;
	    break;
	 default:
	    if(set)  t.par_attr |= a;
	    else     t.par_attr &= ~a;
	    break;
      }
   }
}


/*
 * output character c from current set at current cursor position.
 */

static void output(int c)
{
   int xd, yd, set, mattr, x = t.cursorx, y = t.cursory;
   
   /* select active character set */
   if(t.sshift) set = t.G0123L[ t.sshift ];
   else         set = t.G0123L[ t.leftright[ (c&0x80) >> 7 ] ];

   if(t.serialmode) {
      xd = attrib(y, x, ATTR_XDOUBLE);
      yd = attrib(y, x, ATTR_YDOUBLE);

      /* update screen memory (if not protected) */
      if( t.service_break || !attrib(y, x, ATTR_PROTECTED) ) {
	 screen[y-1][x-1].chr = c & ~0x80;
	 screen[y-1][x-1].set = set;
	 redrawc(x, y);
      }
   }
   else {  /* parallel */
      xd = (t.par_attr & ATTR_XDOUBLE) > 0;
      yd = (t.par_attr & ATTR_YDOUBLE) > 0;
      if(y<2)  yd = 0;
      
      /* if not protected, (scroll +) update memory */
      if( t.service_break || !attrib(y, x, ATTR_PROTECTED) ) {

	 /* parallel DBS/DBH chars in first row of scrolling area         */
	 /* force a scroll down + a cursor down before writing (page 101) */
	 if(yd && t.scroll_area && y==t.scroll_upper) {
	    scroll(0);
	    move_cursor(APD, -1, -1);
	    y = t.cursory;
	 }
	 
	 /* update screen memory
	  * if no DBS/DBH in the first row or in the row below a protected area
	  * if no DBS/DBH in the row below a scrolling area (page 99)
	  */
	 if( !(yd && !t.service_break && attrib(y-1, x, ATTR_PROTECTED))  &&
	    !(yd && t.scroll_area && y==t.scroll_lower+1)  ) {
	    
	    /* parallel DBH/DBS chars extent upwards, write in the row above */
	    if(yd) y--;

	    screen[y-1][x-1].chr  = c & ~0x80;
	    screen[y-1][x-1].set  = set;
	    screen[y-1][x-1].fg   = t.par_fg;
	    screen[y-1][x-1].bg   = t.par_bg;
	    screen[y-1][x-1].attr = t.par_attr;

	    /* par. DBS/DBW attrs apply also to the next char ! (page 106) */
	    if(xd && x<40)  screen[y-1][x].attr = t.par_attr;
	 
	    mattr = t.par_attr & ~ATTR_ANYSIZE;
	    if(t.par_attr & ATTR_ANYSIZE)  mattr |= ATTR_SIZE;
      
	    if(x>1) {  /* set/reset markers */
	       screen[y-1][x-1].mark = screen[y-1][x-1-1].attr ^ mattr;
	       screen[y-1][x-1].mark &= ~(ATTR_FOREGROUND|ATTR_BACKGROUND);
	       if(screen[y-1][x-1-1].fg!=t.par_fg)
	          screen[y-1][x-1].mark |= ATTR_FOREGROUND;
	       if(screen[y-1][x-1-1].bg!=t.par_bg)
	          screen[y-1][x-1].mark |= ATTR_BACKGROUND;
	    }
	    if(x<40) {  /* set/reset markers */
	       screen[y-1][x].mark = mattr ^ screen[y-1][x].attr;
	       screen[y-1][x].mark &= ~(ATTR_FOREGROUND|ATTR_BACKGROUND);
	       if(screen[y-1][x].fg!=t.par_fg)
	          screen[y-1][x].mark |= ATTR_FOREGROUND;
	       if(screen[y-1][x].bg!=t.par_bg)
	          screen[y-1][x].mark |= ATTR_BACKGROUND;
	    }
	 
	    redrawc(x, y);
	 } /* scrolling/protected area */
      } /* if not protected */
   }  /* if serial / parallel */
   
   if(xd && t.cursorx<40)  move_cursor(APF, -1, -1);
   move_cursor(APF, -1, -1);
   t.sshift = 0;
}


/*
 * redraws character at screen position x, y (1 based !),
 * if not concealed or obscured
 */

static void redrawc(int x, int y)
{
   int c, set, xd, yd, up_in=0, dn_in=0;
   unsigned int real, xreal=0, yreal=0, xyreal=0;

   dirty = 1;
   
   if(tia) {
      xputc(screen[y-1][x-1].chr&0x7f, screen[y-1][x-1].set, x-1, y-1,
	    0, 0, 0, (screen[y-1][x-1].chr>>8) & 0x7f, WHITE, BLACK, fontheight, rows);
      if(t.cursor_on && x==t.cursorx && y==t.cursory)  invert_cursor(x-1, y-1);
   }
   else {
      /* check whether attributes are valid */
      xd = attrib(y, x, ATTR_XDOUBLE);
      yd = attrib(y, x, ATTR_YDOUBLE);
      if(x>=40)    xd = 0;
      if(y>=rows)  yd = 0;
   
      /* check whether origin is obscured */
      if( (y>1 && realattr(y-1, x, ATTR_YDOUBLE)) ||
          (x>1 && realattr(y, x-1, ATTR_XDOUBLE)) ||
	  (y>1 && x>1 && realattr(y-1, x-1, ATTR_XDOUBLE) &&
	      realattr(y-1, x-1, ATTR_YDOUBLE)) ) {
	 screen[y-1][x-1].real = 0;
      }
      else {
	 /* check whether DBW char is partially obscured */
	 if(xd && y>1 && x<40 && realattr(y-1, x+1, ATTR_YDOUBLE))  xd = 0;

	 /* check whether char crosses borders of scrolling area (page 99) */
	 if(yd && t.scroll_area) {
	    if(y  >=t.scroll_upper && y  <=t.scroll_lower)  up_in=1;
	    if(y+1>=t.scroll_upper && y+1<=t.scroll_lower)  dn_in=1;
	    if(up_in != dn_in)  yd = 0;
	 }
	 
	 /* check whether char crosses borders of protected area (page 110) */
	 if(yd &&  attrib(y, x, ATTR_PROTECTED) !=
	           attrib(y+1, x, ATTR_PROTECTED) )  yd=0;
      
	 /* concealed characters are drawn as SPACES (page 109) */
	 if(!reveal && attrib(y, x, ATTR_CONCEALED)) { c=' ';  set=PRIM; }
	 else {
	    c   = screen[y-1][x-1].chr & 0x7f;
	    set = screen[y-1][x-1].set;
	 }

	 /* now really display the character in the remaining size */
	 xputc(c, set, x-1, y-1, xd, yd,
	       attrib(y, x, ATTR_UNDERLINE),
	       (screen[y-1][x-1].chr>>8) & 0x7f,
	       attrib(y, x, ATTR_INVERTED) ? attr_bg(y, x) : attr_fg(y, x),
	       attrib(y, x, ATTR_INVERTED) ? attr_fg(y, x) : attr_bg(y, x),
               fontheight, rows);
	 if(t.cursor_on && x==t.cursorx && y==t.cursory)  invert_cursor(x-1, y-1);

	 /* update 'real' attributes */
	 real = screen[y-1][x-1].real;
	 screen[y-1][x-1].real = screen[y-1][x-1].attr & ~ATTR_ANYSIZE;
	 if(xd) {
	    screen[y-1][x-1].real |= ATTR_XDOUBLE;
	    xreal = screen[y-1][x].real;
	    screen[y-1][x].real = 0;
	 }
	 if(yd) {
	    screen[y-1][x-1].real |= ATTR_YDOUBLE;
	    yreal = screen[y][x-1].real;
	    screen[y][x-1].real = 0;
	 }
	 if(xd && yd) {
	    xyreal = screen[y][x].real;
	    screen[y][x].real = 0;
	 }

	 /* redraw (indirect recursion !!) neighbours */
	 updatec(real, x, y);
	 if(xd)  updatec(xreal, x+1, y);
	 if(yd)  updatec(yreal, x, y+1);
	 if(xd && yd)  updatec(xyreal, x+1, y+1);

	 /* redraw an enlarged char to the left (now partially obscured !) */
	 if(yd && x>1 && realattr(y+1, x-1, ATTR_XDOUBLE))  redrawc(x-1, y+1);
      } /* origin obscured */
   } /* TIA */
}


/*
 * this character has been obscured. check whether it was an enlarged char
 * and therefor its neighbours have to be redrawn.
 */
static void updatec(int real, int x, int y)
{
   if(real & ATTR_XDOUBLE)  redrawc(x+1, y);
   if(real & ATTR_YDOUBLE)  redrawc(x, y+1);
   if( (real & ATTR_XDOUBLE) && (real & ATTR_YDOUBLE) )  redrawc(x+1, y+1);
}


/*
 * redraw rectangle of screen (for exposure handling)
 */
void redraw_screen_rect(int x1, int y1, int x2, int y2)
{
   int x, y;

   for(y=y1; y<=y2; y++)
      for(x=x1; x<=x2; x++)  redrawc(x+1, y+1);
}

	 
/*
 * DRC's in the range start-stop with step have been (re)defined.
 * This routine checks whether this character is currently visible and so
 * needs to be redisplayed.
 */

static void update_DRCS_display(int start, int stop, int step)
{
   int x, y, c;

   for(y=0; y<24; y++)
      for(x=0; x<40; x++)
	 if(screen[y][x].set==DRCS)
	    for(c=start; c<stop; c+=step)
	       if(screen[y][x].chr==c)  redrawc(x+1, y+1);
}


/*
 * initialize screen memory and clear display
 */

static void clearscreen()
{
   int x, y;
   
   t.clut = 0;
   t.par_attr = 0;
   t.par_fg = WHITE;
   t.par_bg = TRANSPARENT;
   t.scroll_area = 0;
   t.scroll_impl = 1;
   t.cursorx = t.cursory = 1;
   
   for(y=0; y<24; y++)
      for(x=0; x<40; x++) {
	 screen[y][x].chr = ' ';
	 screen[y][x].set = PRIM;
	 screen[y][x].attr = 0;
	 screen[y][x].real = 0;
	 screen[y][x].mark = 0;
	 screen[y][x].fg = WHITE;
	 screen[y][x].bg = TRANSPARENT;
      }

   redraw_screen_rect(0, 0, 39, rows-1);
   if(t.cursor_on) invert_cursor(0, 0);
}


static void scroll(int up)
{
   int y, x, savereal[40];

   if(up) {
      /* save real attributes of first row */
      for(x=0; x<40; x++)  savereal[x] = screen[t.scroll_upper-1][x].real;
      
      /* scroll screen memory up */
      for(y=t.scroll_upper-1; y<t.scroll_lower-1; y++) {
	 for(x=0; x<40; x++)  screen[y][x] = screen[y+1][x];
      }
   }
   else {
      /* scroll screen memory down */
      for(y=t.scroll_lower-1; y>t.scroll_upper-1; y--) {
	 for(x=0; x<40; x++)  screen[y][x] = screen[y-1][x];
      }
   }

   /* clear row */
   for(x=0; x<40; x++) {
      screen[y][x].chr = ' ';
      screen[y][x].set = PRIM;
      screen[y][x].attr = 0;
      screen[y][x].real = 0;
      screen[y][x].mark = 0;
      screen[y][x].fg = WHITE;
      screen[y][x].bg = TRANSPARENT;
   }

   /* remove old cursor */
   if(t.cursor_on && t.cursory>=t.scroll_upper && t.cursory<=t.scroll_lower)
      invert_cursor(t.cursorx-1, t.cursory-1);

   /* fast scroll the area */
   for(y=t.scroll_upper; y<=t.scroll_lower; y++)
      for(x=1; x<=40; x++)  redrawc(x, y);

   /* redraw cursor */
   if(t.cursor_on && t.cursory>=t.scroll_upper && t.cursory<=t.scroll_lower)
      invert_cursor(t.cursorx-1, t.cursory-1);
}


/*
 * layer 6 debug log routine. (varargs !?!)
 */

void LOG(const char *format, ...)
{
   FILE *textlog = stderr;

   va_list args;
   va_start(args, format);

   if(textlog) {
      vfprintf(textlog, format, args);
      fflush(textlog);
   }
}

void xbtxerror(int perr, const char *format, ...)
{
   va_list args;
   va_start(args, format);

   fprintf(stderr, "\nXCEPT: ");
   vfprintf(stderr, format, args);
   if(perr) fprintf(stderr, ": %s", sys_errlist[errno]);
   fprintf(stderr, "\n\n");
   exit(1);
}
