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

#include <stdio.h>
#include <stdlib.h>
#include "font.h"
#include "attrib.h"
#include "xfont.h"

struct btxchar {
   struct btxchar *link;    /* NULL: use this char,  else: use linked char */
   char *raw;               /* pointer to raw font data */
   char bits;               /* depth in case of DRC */
};                          /* 0: normal   (index = yd*2+xd) */
                            /* 1: xdouble  */
                            /* 2: ydouble  */
                            /* 3: xydouble */

typedef struct {
   int red;
   int green;
   int blue;
} color;

unsigned char *memimage = NULL;

extern int      scr, btxwidth;
extern int pixel_size, have_color, visible, fontheight;
extern char raw_font[];

/* local variables */
static struct btxchar btx_font[6*96];
static int fullrow_bg[24];        /* fullrow background color for eacch row */
static int dclut[4];              /* map the 4-color DRC's */
static color colormap[32+4+24];   /* store pixel value for each colorcell */

/* local functions */
static void xdraw_normal_char(struct btxchar *ch, int x, int y, int xd, int yd, int ul, struct btxchar *dia, int fg, int bg);
static void xdraw_multicolor_char(struct btxchar *ch, int x, int y, int xd, int yd);

/*
 * initialize character sets. No Pixmaps are created.
 */

void init_fonts()
{
   static char raw_del[] = { 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
			     0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
			     0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f };
   int n;

   /* primary graphic, 2nd suppl. mosaic, suppl. graphic, 3rd suppl. mosaic */
   for(n=0; n<4*96; n++) {
      btx_font[n].raw  = raw_font+n*2*FONT_HEIGHT;
      btx_font[n].bits = 1;
      btx_font[n].link = NULL;
   }
   
   /* link chars into '1st supplementary mosaic' (L) set */
   for(n=0; n<32; n++) {
      btx_font[SUP1*96+n].link    = btx_font+SUP2*96+n;
      btx_font[SUP1*96+n+32].link = btx_font+PRIM*96+n+32;
      btx_font[SUP1*96+n+64].link = btx_font+SUP2*96+n+64;
   }

   /* initialize the DEL character of the L set */
   btx_font[L*96+0x7f-0x20].link = NULL;
   btx_font[L*96+0x7f-0x20].raw = raw_del;
   btx_font[L*96+0x7f-0x20].bits = 1;

   /* initialize DRCS font to all NULL's */
   btx_font[DRCS*96+0].link = btx_font;    /* link 'SPACE' */
   for(n=1; n<96; n++) {
      btx_font[DRCS*96+n].bits = 0;
      btx_font[DRCS*96+n].raw  = NULL;
      btx_font[DRCS*96+n].link = NULL;
   }
}


/*
 * draw the specified character at cursor position x, y (zero based).
 * The character is stretched according to xdouble, ydouble (1=stretch,
 * 0=don't). If the Pixmap for this state doesn't exist, it will be
 * created and cached.
 */

void xputc(int c, int set, int x, int y, int xdouble, int ydouble, int underline, int diacrit, int fg, int bg)
{
   extern int rows;
   struct btxchar *ch, *dia;
   
   if(!visible || x<0 || y<0 || x>39 || y>rows-1) return;

   ch = btx_font + set*96 + c - 0x20;
   
   /* folow the link pointer */
   if(ch->link)  ch = ch->link;

   /* a yet undefined DRC should be drawn - draw a SPACE */
   if(!ch->raw)  ch = btx_font;

   if(ch->bits==1) {
      dia = diacrit ? btx_font + SUPP*96 + diacrit - 0x20 : NULL;
      xdraw_normal_char(ch, x, y, xdouble, ydouble, underline, dia, fg, bg);
   }
   else {  /* 2/4 bits */
      xdraw_multicolor_char(ch, x, y, xdouble, ydouble);
   }
}


#define set_mem(x, y, col) { \
   memimage[(y)*480*3 + (x)*3 + 0] = colormap[col].red>>8;   \
   memimage[(y)*480*3 + (x)*3 + 1] = colormap[col].green>>8; \
   memimage[(y)*480*3 + (x)*3 + 2] = colormap[col].blue>>8; }

static void xdraw_normal_char(ch, x, y, xd, yd, ul, dia, fg, bg)
struct btxchar *ch, *dia;
int x, y, xd, yd, ul, fg, bg;
{
   int i, j, z, s, yy, yyy, xxx;

   if(fg==TRANSPARENT)  fg = 32+4+y;
   if(bg==TRANSPARENT)  bg = 32+4+y;

   z=y*fontheight;
   for(yy=0; yy<fontheight; yy++) {
      for(yyy=0; yyy<(yd+1); yyy++) {
         s=x*FONT_WIDTH;
         for(j=0; j<2; j++)
            for(i=5; i>=0; i--)
               for(xxx=0; xxx<(xd+1); xxx++, s++)
                  set_mem(s, z, ( (ch->raw[yy*2+j]&(1<<i)) ||
                                  (dia && (dia->raw[yy*2+j]&(1<<i))) )
                                ? fg : bg);
         z++;
      }
   }
   
   if(ul)
      for(yyy=0; yyy<(yd+1); yyy++)
         for(xxx=0; xxx<FONT_WIDTH*(xd+1); xxx++)
            set_mem(x*FONT_WIDTH+xxx,
                    y*fontheight+(fontheight-1)*(yd+1)+yyy, fg);
}


static void xdraw_multicolor_char(ch, x, y, xd, yd)
struct btxchar *ch;
int x, y, xd, yd;
{
   extern int tia;
   int c, i, j, z, s, p, yy, xxx, yyy;

   z=y*fontheight;
   for(yy=0; yy<fontheight; yy++) {
      for(yyy=0; yyy<(yd+1); yyy++) {
         s=x*FONT_WIDTH;
         for(j=0; j<2; j++)
            for(i=5; i>=0; i--) {
               for(p=0, c=0; p<ch->bits; p++)
                  if(ch->raw[p*2*FONT_HEIGHT+yy*2+j]&(1<<i))  c |= 1<<p;
               if(ch->bits==2) {
                  c = dclut[c];
                  if(c==TRANSPARENT)  c = 32+4+y;
               }
               else c += 16;
               for(xxx=0; xxx<(xd+1); xxx++, s++)  set_mem(s, z, c);
            }
         z++;
      }
   }
}


void xcursor(int x, int y)
{
   /* this inverts the character at the given location */
   for (int yy = y * fontheight; yy < y * fontheight + fontheight; yy++) {
      for (int xx = x * 12; xx < x * 12 + 12; xx++) {
         memimage[(yy)*480*3 + (xx)*3 + 0] ^= 0xFF;
         memimage[(yy)*480*3 + (xx)*3 + 1] ^= 0xFF;
         memimage[(yy)*480*3 + (xx)*3 + 2] ^= 0xFF; 
      }
   }
}

/*
 * defines the raw bitmap data for a DRC 'c'. If 'c' was in use before,
 * its old raw data and Pixmaps are freed. No Pixmap for any state is
 * initialized, since xputc() does this on demand.
 */

void define_raw_DRC(int c, char *data, int bits)
{
   struct btxchar *ch = btx_font + DRCS*96 + c - 0x20;
   int n;
   
   if(ch->raw)  free(ch->raw);

   if(! (ch->raw = malloc(2*FONT_HEIGHT*bits)) ) {
      perror("XCEPT: no mem for raw DRCS");
      exit(1);
   }

   ch->bits = bits;
   for(n=0; n<2*FONT_HEIGHT*bits; n++)  ch->raw[n] = data[n];

   ch->link = NULL;
}


/*
 * free the current DRCS completely
 */
void free_DRCS()
{
   int n;
   
   btx_font[DRCS*96+0].link = btx_font;    /* link 'SPACE' */
   for(n=DRCS*96+1; n<(DRCS+1)*96; n++) {
      if(btx_font[n].raw)  {
	 free(btx_font[n].raw);
	 btx_font[n].raw = NULL;
      }
      btx_font[n].link = NULL;
      btx_font[n].bits = 0;
   }
}


/*
 * initialize colormap entries for clut 0-3, DCLUT + fullrow background
 */
void default_colors()
{
   int n;

   /* clut 0 + clut 1 */
   for(n=0; n<16; n++) {  /* page 112) */
      if(n==8)  { colormap[n] = colormap[0]; continue; }
      colormap[n].red   = ((n&1)>0) ? ( ((n&8)==0) ? 0xffff : 0x7fff ) : 0;
      colormap[n].green = ((n&2)>0) ? ( ((n&8)==0) ? 0xffff : 0x7fff ) : 0;
      colormap[n].blue  = ((n&4)>0) ? ( ((n&8)==0) ? 0xffff : 0x7fff ) : 0;
   }

   /* clut 2 + clut 3 */
   for(n=0; n<8; n++) {
      colormap[n+16].red   = colormap[n+24].red   = n&1 ? 0xffff : 0;
      colormap[n+16].green = colormap[n+24].green = n&2 ? 0xffff : 0;
      colormap[n+16].blue  = colormap[n+24].blue  = n&4 ? 0xffff : 0;
   }

   /* DCLUT */
   for(n=0; n<4; n++) {
      colormap[32+n].red   = colormap[n].red;
      colormap[32+n].green = colormap[n].green;
      colormap[32+n].blue  = colormap[n].blue;
      dclut[n] = n;
   }

   /* fullrow background (BLACK) */
   for(n=0; n<24; n++) {
      colormap[32+4+n].red   = colormap[0].red;
      colormap[32+4+n].green = colormap[0].green;
      colormap[32+4+n].blue  = colormap[0].blue;
      fullrow_bg[n] = 0;
   }
}


/*
 * define new RGB values for color 'index' in colormap
 */
void define_color(unsigned int index, unsigned int r, unsigned int g, unsigned int b)
{
   int i;
   
   colormap[index].red   = r << 12;
   colormap[index].green = g << 12;
   colormap[index].blue  = b << 12;

   /* if DCLUT contains this color, change DCLUT color too */
   for(i=0; i<4; i++)
      if(dclut[i]==index) define_DCLUT(i, index);

   /* if fullrow_bg contains this color, change fullrow color too */
   for(i=0; i<24; i++)
      if(fullrow_bg[i]==index) define_fullrow_bg(i, index);
}


/*
 * set RGB values for DCLUT color 'entry' to those of color 'index'.
 */
void define_DCLUT(entry, index)
int entry, index;
{
   dclut[entry] = index;
   colormap[32+entry].red   = colormap[index].red;
   colormap[32+entry].green = colormap[index].green;
   colormap[32+entry].blue  = colormap[index].blue;
}


/*
 * set RGB values for fullrow_bg of 'row' to those of color 'index'.
 */
void define_fullrow_bg(row, index)
int row, index;
{
   fullrow_bg[row] = index;
   colormap[32+4+row].red   = colormap[index].red;
   colormap[32+4+row].green = colormap[index].green;
   colormap[32+4+row].blue  = colormap[index].blue;
}

void init_xfont()
{
   memimage = malloc(480*240*3);
   init_fonts();
   default_colors();
}
