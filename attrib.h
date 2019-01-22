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
 *
 */

#define ATTR_UNDERLINE  0x00001
#define ATTR_FLASH      0x00002
#define ATTR_INVERTED   0x00008
#define ATTR_WINDOW     0x00010
#define ATTR_PROTECTED  0x00020
#define ATTR_MARKED     0x00040
#define ATTR_CONCEALED  0x00080
#define ATTR_YDOUBLE    0x00100
#define ATTR_XDOUBLE    0x00200

#define ATTR_FOREGROUND 0x00400  /* only for set_attr() !! */
#define ATTR_BACKGROUND 0x00800  /*  "  */
#define ATTR_NODOUBLE   0x01000  /*  "  */
#define ATTR_XYDOUBLE   0x02000  /*  "  */
#define ATTR_SIZE       0x04000
#define ATTR_ANYSIZE    (ATTR_NODOUBLE|ATTR_XDOUBLE|ATTR_YDOUBLE|ATTR_XYDOUBLE)


#define attrib(y,x,a)    ((screen[(y)-1][(x)-1].attr & (a)) > 0)
#define realattr(y,x,a)  ((screen[(y)-1][(x)-1].real & (a)) > 0)
#define attr_fg(y,x)     screen[(y)-1][(x)-1].fg
#define attr_bg(y,x)     screen[(y)-1][(x)-1].bg

#define BLACK        0
#define RED          1
#define GREEN        2
#define YELLOW       3
#define BLUE         4
#define MAGENTA      5
#define CYAN         6
#define WHITE        7
#define TRANSPARENT  8


/* information stored at every character location */
struct screen_cell {
   unsigned int chr;
   unsigned char set;
   unsigned int attr;      /* assigned attributes */
   unsigned int mark;      /* markers for each attribute */
   unsigned char fg;
   unsigned char bg;
   unsigned int real;      /* really displayed attributes (size !) */
};
