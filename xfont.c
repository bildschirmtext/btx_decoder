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
#include <X11/Xlib.h>
#include "font.h"
#include "attrib.h"

struct btxchar {
   struct btxchar *link;    /* NULL: use this char,  else: use linked char */
   char *raw;               /* pointer to raw font data */
   char bits;               /* depth in case of DRC */
   Pixmap map[4];           /* 0: normal   (index = yd*2+xd) */
};                          /* 1: xdouble  */
                            /* 2: ydouble  */
                            /* 3: xydouble */

extern Display  *dpy;
extern Window   btxwin;
extern GC       gc;
extern Colormap cmap;
extern int      scr, btxwidth;
extern int pixel_size, have_color, visible, fontheight;
extern char raw_font[];

/* local variables */
static char tmpdata[2*FONT_HEIGHT*2*2*2*2], tmpraw[2*FONT_HEIGHT];
static struct btxchar btx_font[6*96];
static int fullrow_bg[24];        /* fullrow background color for eacch row */
static int dclut[4];              /* map the 4-color DRC's */
static XColor colormap[32+4+24];  /* store pixel value for each colorcell */
static int cur_fg = -1, cur_bg = -1;

/* local functions */
static Pixmap createpixmapfromfont();
static xdraw_normal_char(), xdraw_multicolor_char(), make_stipple();
static rawfont2bitmap();


/*
 * initialize character sets. No Pixmaps are created.
 */

init_fonts()
{
   static char raw_del[] = { 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
			     0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
			     0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f };
   int n, i;

   /* primary graphic, 2nd suppl. mosaic, suppl. graphic, 3rd suppl. mosaic */
   for(n=0; n<4*96; n++) {
      btx_font[n].raw  = raw_font+n*2*FONT_HEIGHT;
      btx_font[n].bits = 1;
      btx_font[n].link = NULL;
      for(i=0; i<4; i++)  btx_font[n].map[i] = NULL;
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
   for(i=0; i<4; i++)  btx_font[L*96+0x7f-0x20].map[i] = NULL;
   
   /* initialize DRCS font to all NULL's */
   btx_font[DRCS*96+0].link = btx_font;    /* link 'SPACE' */
   for(n=1; n<96; n++) {
      btx_font[DRCS*96+n].bits = 0;
      btx_font[DRCS*96+n].raw  = NULL;
      btx_font[DRCS*96+n].link = NULL;
      for(i=0; i<4; i++)  btx_font[DRCS*96+n].map[i] = NULL;
   }
}


/*
 * draw the specified character at cursor position x, y (zero based).
 * The character is stretched according to xdouble, ydouble (1=stretch,
 * 0=don't). If the Pixmap for this state doesn't exist, it will be
 * created and cached.
 */

xputc(c, set, x, y, xdouble, ydouble, underline, diacrit, fg, bg)
int c, set, x, y, xdouble, ydouble, underline, diacrit, fg, bg;
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

static xdraw_normal_char(ch, x, y, xd, yd, ul, dia, fg, bg)
struct btxchar *ch, *dia;
int x, y, xd, yd, ul, fg, bg;
{
   extern unsigned char *memimage;  /* draw to window or to memory ?? */
   int i, j, z, s, yy, yyy, xxx, size;
   XGCValues gcv;
   
   if(fg==TRANSPARENT)  fg = 32+4+y;
   if(bg==TRANSPARENT)  bg = 32+4+y;

   if(memimage) {
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
   else {  /* draw to btx window */
      size = yd*2 + xd;
      /* Pixmap not available in this size, create it */
      if(!ch->map[size]) {
	 rawfont2bitmap(ch->raw, tmpdata, pixel_size*(xd+1),
			pixel_size*(yd+1) );
	 ch->map[size] =
            XCreateBitmapFromData(dpy, btxwin, tmpdata,
				  FONT_WIDTH*pixel_size*(xd+1),
				  FONT_HEIGHT*pixel_size*(yd+1));
      }

      /* set colors */
      if(have_color) {
	 if(cur_fg!=fg)
	    { XSetForeground(dpy, gc, colormap[fg].pixel); cur_fg=fg; }
	 if(cur_bg!=bg)
	    { XSetBackground(dpy, gc, colormap[bg].pixel); cur_bg=bg; }
      }

      /* draw character */
      XCopyPlane(dpy, ch->map[size], btxwin, gc, 0, 0,
		 FONT_WIDTH*pixel_size*(xd+1),
		 fontheight*pixel_size*(yd+1),
		 x*FONT_WIDTH*pixel_size, y*fontheight*pixel_size, 1L);

      if(dia) {
	 /* Pixmap of diacritical mark not available in this size, create it */
	 if(!dia->map[size]) {
	    rawfont2bitmap(dia->raw, tmpdata, pixel_size*(xd+1),
			   pixel_size*(yd+1) );
	    dia->map[size] =
	       XCreateBitmapFromData(dpy, btxwin, tmpdata,
				     FONT_WIDTH*pixel_size*(xd+1),
				     FONT_HEIGHT*pixel_size*(yd+1));
	 }

	 /* adjust GC and draw the diacritical mark (fill stippled) */
	 gcv.stipple = dia->map[size];
	 gcv.ts_x_origin = x*FONT_WIDTH*pixel_size;
	 gcv.ts_y_origin = y*fontheight*pixel_size;
	 gcv.fill_style = FillStippled;
	 XChangeGC(dpy, gc, GCStipple | GCTileStipXOrigin | GCTileStipYOrigin |
		   GCFillStyle, &gcv);
	 XFillRectangle(dpy, btxwin, gc, x*FONT_WIDTH*pixel_size,
			y*fontheight*pixel_size,
			FONT_WIDTH*pixel_size*(xd+1),
			fontheight*pixel_size*(yd+1) );
	 XSetFillStyle(dpy, gc, FillSolid);
      }

      if(ul)
         XFillRectangle(dpy, btxwin, gc, x*FONT_WIDTH*pixel_size,
	    y*fontheight*pixel_size+(fontheight-1)*pixel_size*(yd+1),
            FONT_WIDTH*pixel_size*(xd+1), pixel_size*(yd+1) );
   }
}


static xdraw_multicolor_char(ch, x, y, xd, yd)
struct btxchar *ch;
int x, y, xd, yd;
{
   extern unsigned char *memimage;  /* draw to window or to memory ?? */
   extern int tia;
   int c, i, j, z, s, p, yy, xxx, yyy, colormask, size;
   XGCValues gcv;
   
   if(memimage) {
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
   else {  /* draw to btx window */
      colormask = 0;
      size = yd*2 + xd;
      if(!ch->map[size]) {
	 ch->map[size] =
            createpixmapfromfont(ch->raw, pixel_size*(xd+1),
				 pixel_size*(yd+1), ch->bits );
      }

      if(tia || !have_color) {
	 if(have_color) {
	    if(cur_fg!=WHITE)
	    { XSetForeground(dpy, gc, colormap[WHITE].pixel); cur_fg=WHITE; }
	    if(cur_bg!=BLACK)
	    { XSetBackground(dpy, gc, colormap[BLACK].pixel); cur_bg=BLACK; }
	 }
	 XCopyPlane(dpy, ch->map[size], btxwin, gc, 0, 0,
		    FONT_WIDTH*pixel_size*(xd+1),
		    fontheight*pixel_size*(yd+1),
		    x*FONT_WIDTH*pixel_size, y*fontheight*pixel_size, 1L);
      }
      else {
	 XCopyArea(dpy, ch->map[size], btxwin, gc, 0, 0,
		   FONT_WIDTH*pixel_size*(xd+1),
		   fontheight*pixel_size*(yd+1),
		   x*FONT_WIDTH*pixel_size, y*fontheight*pixel_size);

	 /* Super-PFUSCH !!   (for 2 bit DRC's):             */
	 /* repaint transparent DCLUT colors with fullscr_bg */
	 if(ch->bits==2) {
	    for(i=0; i<4; i++)  if(dclut[i]==TRANSPARENT) colormask |= 1<<i;
	    if(colormask) {
	       make_stipple(ch->raw, tmpraw, colormask);
	       rawfont2bitmap(tmpraw, tmpdata, pixel_size*(xd+1),
			      pixel_size*(yd+1) );
	       gcv.stipple =
	          XCreateBitmapFromData(dpy, btxwin, tmpdata,
					FONT_WIDTH*pixel_size*(xd+1),
					FONT_HEIGHT*pixel_size*(yd+1));
	    
	       gcv.foreground = colormap[32+4+y].pixel;
	       cur_fg = 32+4+y;
	       gcv.ts_x_origin = x*FONT_WIDTH*pixel_size;
	       gcv.ts_y_origin = y*fontheight*pixel_size;
	       gcv.fill_style = FillStippled;
	       XChangeGC(dpy, gc, GCForeground|GCStipple|GCTileStipXOrigin |
			 GCTileStipYOrigin|GCFillStyle, &gcv);
	       XFillRectangle(dpy, btxwin, gc, x*FONT_WIDTH*pixel_size,
			      y*fontheight*pixel_size,
			      FONT_WIDTH*pixel_size*(xd+1),
			      fontheight*pixel_size*(yd+1) );
	       XSetFillStyle(dpy, gc, FillSolid);
	       XFreePixmap(dpy, gcv.stipple);
	    } /* colormask */
	 } /* bits==2 */
      } /* TIA */
   } /* memimage */
}


xclearscreen()
{
   extern int tia, rows;
   int y, bg;
   
   if(visible) {
      if(have_color) {
	 for(y=0; y<rows; y++) {
	    bg = tia ? BLACK : 32+4+y;
	    if(cur_fg!=bg)
	       { XSetForeground(dpy, gc, colormap[bg].pixel); cur_fg=bg;}

	    XFillRectangle(dpy, btxwin, gc, 0, y*fontheight*pixel_size,
			   btxwidth, fontheight*pixel_size);
	 }
      }
      else XClearWindow(dpy, btxwin);
   }
}


xscroll(upper, lower, up)
int upper, lower, up;
{
   int col, y;
   
   if(visible) {
      /* XSetGraphicsExposures(dpy, gc, True); */

      if(upper<lower)
         XCopyArea(dpy, btxwin, btxwin, gc, 0,
		   (upper+up)*fontheight*pixel_size,
		   btxwidth, (lower-upper)*fontheight*pixel_size,
		   0, (upper+!up)*fontheight*pixel_size);

      /* XSetGraphicsExposures(dpy, gc, False); */

      /* clear the scrolled in row */
      y = up ? lower : upper;
      if(have_color) {
	 col = 32+4+y;
	 if(cur_fg!=col)
	    { XSetForeground(dpy, gc, colormap[col].pixel); cur_fg=col;}
      }
      else XSetForeground(dpy, gc, WhitePixel(dpy, scr));
      
      XFillRectangle(dpy, btxwin, gc, 0, y*fontheight*pixel_size, btxwidth,
		     fontheight*pixel_size);

      if(!have_color) XSetForeground(dpy, gc, BlackPixel(dpy, scr));
   }
}


xcursor(x, y)
int x, y;
{
   extern GC cursorgc;
   
   if(visible) XFillRectangle(dpy, btxwin, cursorgc, x*FONT_WIDTH*pixel_size,
			      y*fontheight*pixel_size,
			      FONT_WIDTH*pixel_size, fontheight*pixel_size);
}


/*
 * set bits in dst for each color in src that has a bit set to 1 in mask 
 */
static make_stipple(src, dst, mask)
char *src, *dst;
int mask;
{
   int c, i, y;

   for(y=0; y<2*FONT_HEIGHT; y++) {
      *dst = 0;
      for(i=5; i>=0; i--) {
	 c = ((*src & (1<<i))>0) + ((*(src+2*FONT_HEIGHT) & (1<<i))>0)*2;
	 if(mask & (1<<c))  *dst |= (1<<i);
      }
      src++; dst++;
   }
}


/*
 * converts the BTX font (src) to X11 bitmap data (dst).
 * The xzoom and yzoom parameters specify the stretching factor in x and
 * y direction. Make sure that you have allocated enough memory at dst !
 */

static rawfont2bitmap(src, dst, xzoom, yzoom)
char *src, *dst;
int xzoom, yzoom;
{
#define MAXDATA 16
#define addbit(x) { byte = bitindex/8; \
                    if(x) data[byte] |= 1 << 7-bitindex%8; \
                    bitindex++; }

   int x, y, i, j, k, bitindex, byte;
   static char data[MAXDATA];
   char c;

   for(y=0; y<FONT_HEIGHT; y++) {
      for(i=0; i<MAXDATA; i++) data[i] = 0;
      bitindex = 0;

      /* extract and x-stretch font into data field  (12 bit !) */
      for(k=0; k<2; k++) {
	 for(x=5; x>=0; x--)
            for(j=0; j<xzoom; j++) addbit(*src & (1<<x));
	 src++;
      }

      /* reverse data field for bitmap format */
      for(i=0; i<=byte; i++) {
         c = data[i];
         data[i] = 0;
         for(j=0; j<=7; j++)  if(c & 1<<j) data[i] |= 1<<7-j;
      }

      /* copy and y-stretch data into dst */
      for(j=0; j<yzoom; j++)
         for(i=0; i<=byte; i++)  *dst++ = data[i];
   }
}


/*
 * create and initialize pixmap with font data 'src'.
 * 4 bit (16 color) DRC's are mapped to colormap entries 16-31.
 * 2 bit (4  color) DRC's are mapped to colormap entries 32-35 (DCLUT).
 */
static Pixmap createpixmapfromfont(src, xzoom, yzoom, bits)
char *src;
int xzoom, yzoom, bits;
{
   static char data[FONT_HEIGHT*2*2*FONT_WIDTH*2*2], row[FONT_WIDTH*2*2];
   char *d = data;
   int c, i, j, k, l, x, y, byteindex;
   XImage *image;
   Pixmap pix;

   pix = XCreatePixmap(dpy, btxwin, FONT_WIDTH*xzoom,
		       FONT_HEIGHT*yzoom, DefaultDepth(dpy, scr));

   image = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr),
			ZPixmap, 0, (char *) data, xzoom*FONT_WIDTH,
			yzoom*FONT_HEIGHT, 8, 0);

   for(y=0; y<FONT_HEIGHT; y++) {
      byteindex = 0;
      for(l=0; l<2; l++) {
	 for(x=5; x>=0; x--) {
	    for(c=0, k=0; k<bits; k++)
               if(*(src+k*FONT_HEIGHT*2) & (1<<x) )  c |= 1<<k;
	    for(j=0; j<xzoom; j++)
	       row[byteindex++] =
	          bits==4 ? colormap[c+16].pixel : colormap[c+32].pixel;
	 }
	 src++;
      }
      
      for(j=0; j<yzoom; j++)
         for(i=0; i<byteindex; i++)  *d++ = row[i];
   }

   XPutImage(dpy, pix, gc, image, 0, 0, 0, 0,
	     xzoom*FONT_WIDTH, yzoom*FONT_HEIGHT);
   
   /* Note that when the image is created using XCreateImage, the destroy
    * procedure that the XDestroyImage function calls frees both the image
    * structure and the data pointed to by the image structure.
    *
    * The free() function causes the space pointed to by ptr to be deallocated,
    * that is, made available for further allocation.  If ptr is a null point-
    * er, no action occurs.
    */
   image->data = NULL;
   
   XDestroyImage(image);

   return pix;
}


/*
 * defines the raw bitmap data for a DRC 'c'. If 'c' was in use before,
 * its old raw data and Pixmaps are freed. No Pixmap for any state is
 * initialized, since xputc() does this on demand.
 */

define_raw_DRC(c, data, bits)
int c, bits;
char *data;
{
   struct btxchar *ch = btx_font + DRCS*96 + c - 0x20;
   char *malloc();
   int n;
   
   if(ch->raw)  free(ch->raw);
   for(n=0; n<4; n++)
      if(ch->map[n]) {
	 XFreePixmap(dpy, ch->map[n]);
	 ch->map[n] = NULL;
      }

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
free_DRCS()
{
   int i, n;
   
   btx_font[DRCS*96+0].link = btx_font;    /* link 'SPACE' */
   for(n=DRCS*96+1; n<(DRCS+1)*96; n++) {
      if(btx_font[n].raw)  {
	 free(btx_font[n].raw);
	 btx_font[n].raw = NULL;
      }
      for(i=0; i<4; i++)
         if(btx_font[n].map[i]) {
	    XFreePixmap(dpy, btx_font[n].map[i]);
	    btx_font[n].map[i] = NULL;
	 }
      btx_font[n].link = NULL;
      btx_font[n].bits = 0;
   }
}


/*
 * free ALL defined font pixmaps (for change of window size).
 */
free_font_pixmaps()
{
   int i, n;
   
   for(n=0; n<6*96; n++)
      if(!btx_font[n].link)
         for(i=0; i<4; i++)
            if(btx_font[n].map[i]) {
	       XFreePixmap(dpy, btx_font[n].map[i]);
	       btx_font[n].map[i] = NULL;
	    }
}

   
/*
 * try to allocate the colors:  15 shareable read only color cells +
 *                              16 read/write color cells +
 *                               4 read/write  " for DCLUT mapping +
 *    (for better performance)  24 read/write  " for fullrow_bg colors
 */
alloc_colors()
{
   int n;
   unsigned long planemasks[1], pixelvals[16+4+24];

   /* allocate colormap entries 0-15 read only  (8 == TRANSPARENT !) */
   /* colormap has to be already initialized by init_colormap()      */
   for(n=0; n<16; n++) {
      if(n==8)  { colormap[n] = colormap[0]; continue; }
      if(have_color)
         if( !XAllocColor(dpy, cmap, &colormap[n]) )  have_color = 0;
   }

   /* allocate colormap entries 16-31 and DCLUT entries 0-3
      and fullrow_bg's 0-23 read/write */
   if(have_color && XAllocColorCells(dpy, cmap, False,
				     planemasks, 0, pixelvals, 16+4+24) ) {
      for(n=0; n<16+4+24; n++) colormap[16+n].pixel = pixelvals[n];
   }
   else have_color = 0;

   store_colors();
}


/*
 * initialize colormap and STORE colors !
 */
default_colors()   /* page 153 */
{
   init_colormap();
   store_colors();
}


/*
 * load X colors with values in colormap (only the read/write cells !)
 */
store_colors()
{
   int n;

   if(visible && have_color) {
      for(n=0; n<8; n++) {
	 XStoreColor(dpy, cmap, &colormap[n+16]);  /* clut 2 */
	 XStoreColor(dpy, cmap, &colormap[n+24]);  /* clut 3 */
      }

      for(n=0; n<4; n++)  XStoreColor(dpy, cmap, colormap+32+n); /* dclut */
      for(n=0; n<24; n++) XStoreColor(dpy, cmap, colormap+32+4+n);
   }
}


/*
 * initialize colormap entries for clut 0-3, DCLUT + fullrow background
 */
init_colormap()
{
   int n;

   /* clut 0 + clut 1 */
   for(n=0; n<16; n++) {  /* page 112) */
      if(n==8)  { colormap[n] = colormap[0]; continue; }
      colormap[n].red   = ((n&1)>0) ? ( ((n&8)==0) ? 0xffff : 0x7fff ) : 0;
      colormap[n].green = ((n&2)>0) ? ( ((n&8)==0) ? 0xffff : 0x7fff ) : 0;
      colormap[n].blue  = ((n&4)>0) ? ( ((n&8)==0) ? 0xffff : 0x7fff ) : 0;
      colormap[n].flags = DoRed | DoGreen | DoBlue;
   }

   /* clut 2 + clut 3 */
   for(n=0; n<8; n++) {
      colormap[n+16].red   = colormap[n+24].red   = n&1 ? 0xffff : 0;
      colormap[n+16].green = colormap[n+24].green = n&2 ? 0xffff : 0;
      colormap[n+16].blue  = colormap[n+24].blue  = n&4 ? 0xffff : 0;
      colormap[n+16].flags = colormap[n+24].flags = DoRed | DoGreen | DoBlue;
   }

   /* DCLUT */
   for(n=0; n<4; n++) {
      colormap[32+n].red   = colormap[n].red;
      colormap[32+n].green = colormap[n].green;
      colormap[32+n].blue  = colormap[n].blue;
      colormap[32+n].flags = DoRed | DoGreen | DoBlue;
      dclut[n] = n;
   }

   /* fullrow background (BLACK) */
   for(n=0; n<24; n++) {
      colormap[32+4+n].red   = colormap[0].red;
      colormap[32+4+n].green = colormap[0].green;
      colormap[32+4+n].blue  = colormap[0].blue;
      colormap[32+4+n].flags = DoRed | DoGreen | DoBlue;
      fullrow_bg[n] = 0;
   }
}


/*
 * define new RGB values for color 'index' in colormap
 */
define_color(index, r, g, b)
unsigned int index, r, g, b;
{
   int i;
   
   colormap[index].red   = r << 12;
   colormap[index].green = g << 12;
   colormap[index].blue  = b << 12;
   colormap[index].flags = DoRed | DoGreen | DoBlue;
   if(visible && have_color) XStoreColor(dpy, cmap, colormap+index);

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
define_DCLUT(entry, index)
int entry, index;
{
   dclut[entry] = index;
   colormap[32+entry].red   = colormap[index].red;
   colormap[32+entry].green = colormap[index].green;
   colormap[32+entry].blue  = colormap[index].blue;
   colormap[32+entry].flags = colormap[index].flags;
   if(visible && have_color) XStoreColor(dpy, cmap, colormap+32+entry);
}


/*
 * set RGB values for fullrow_bg of 'row' to those of color 'index'.
 */
define_fullrow_bg(row, index)
int row, index;
{
   fullrow_bg[row] = index;
   colormap[32+4+row].red   = colormap[index].red;
   colormap[32+4+row].green = colormap[index].green;
   colormap[32+4+row].blue  = colormap[index].blue;
   colormap[32+4+row].flags = colormap[index].flags;
   if(visible && have_color) XStoreColor(dpy, cmap, colormap+32+4+row);
}   
