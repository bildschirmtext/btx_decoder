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


/* primary control function set (page 119  and  annex 6) */

#define NUL 0x00         /* NOP time filler          (level 2) */
#define SOH 0x01         /* start of heading         (level 2) */
#define STX 0x02         /* start text               (level 2) */
#define ETX 0x03         /* end text                 (level 2) */
#define EOT 0x04         /* end of transmission      (level 2) */
#define ENQ 0x05         /* enquiry                  (level 2) */
#define ACK 0x06         /* acknowledge              (level 2) */
#define ITB 0x07         /* end intermediate block   (level 2) */
#define APB 0x08         /* active position back     (level 6) */
#define APF 0x09         /* active position forward  (level 6) */
#define APD 0x0a         /* active position down     (level 6) */
#define APU 0x0b         /* active position up       (level 6) */
#define CS  0x0c         /* clear screen             (level 6) */
#define APR 0x0d         /* active position return   (level 6) */
#define LS1 0x0e         /* locking shift 1          (level 6) */
#define LS0 0x0f         /* locking shift 0          (level 6) */
#define DLE 0x10         /* data link escape         (level 2) */
#define CON 0x11         /* cursor on                (level 6) */
#define RPT 0x12         /* repeat last character    (level 6) */
#define INI 0x13         /* initiator '*'            (level 7) */
#define COF 0x14         /* cursor off               (level 6) */
#define NAK 0x15         /* negative acknowledge     (level 2) */
#define SYN 0x16         /*                          (level 2) */
#define ETB 0x17         /* end textblock            (level 2) */
#define CAN 0x18         /* cancel, clear to eol     (level 6) */
#define SS2 0x19         /* single shift for G2 SET  (level 6) */
#define DCT 0x1a         /* terminate data collect   (level 7) */
#define ESC 0x1b         /* escape                   (level 6) */
#define TER 0x1c         /* terminator '#'           (level 7) */
#define SS3 0x1d         /* single shift for G3 SET  (level 6) */
#define APH 0x1e         /* active position home     (level 6) */
#define APA 0x1f         /* active position          (level 6) */
#define US  0x1f         /* unit separator (= APA)   (level 6) */


/* supplementary control function set repertory 1 (serial) C1S (page 121) */

#define ABK 0x80         /* Alphanumeric black   */
#define ANR 0x81         /* Alphanumeric red     */
#define ANG 0x82         /* Alphanumeric green   */
#define ANY 0x83         /* Alphanumeric yellow  */
#define ANB 0x84         /* Alphanumeric blue    */
#define ANM 0x85         /* Alphanumeric magenta */
#define ANC 0x86         /* Alphanumeric cyan    */
#define ANW 0x87         /* Alphanumeric white   */
#define FSH 0x88         /* Flashing begin  */
#define STD 0x89         /* Flashing steady */
#define EBX 0x8a         /* End of window   */
#define SBX 0x8b         /* Start of window */
#define NSZ 0x8c         /* Normal size     */
#define DBH 0x8d         /* Double height   */
#define DBW 0x8e         /* Double width    */
#define DBS 0x8f         /* Double size     */
#define MBK 0x90         /* Mosaic black   */
#define MSR 0x91         /* Mosaic red     */
#define MSG 0x92         /* Mosaic green   */
#define MSY 0x93         /* Mosaic yellow  */
#define MSB 0x94         /* Mosaic blue    */
#define MSM 0x95         /* Mosaic magenta */
#define MSC 0x96         /* Mosaic cyan    */
#define MSW 0x97         /* Mosaic white   */
#define CDY 0x98         /* Conceal display  */
#define SPL 0x99         /* Stop lining      */
#define STL 0x9a         /* Start lining     */
#define CSI 0x9b         /* Control sequence introducer */
#define BBD 0x9c         /* Black background */
#define NBD 0x9d         /* New background   */
#define HMS 0x9e         /* Hold mosaic      */
#define RMS 0x9f         /* Release mosaic   */


/* supplementary control function set repertory 2 (parallel) C1P (page 122) */

#define BKF 0x80         /* Black   foreground */
#define RDF 0x81         /* Red     foreground */
#define GRF 0x82         /* Green   foreground */
#define YLF 0x83         /* Yellow  foreground */
#define BLF 0x84         /* Blue    foreground */
#define MGF 0x85         /* Magenta foreground */
#define CNF 0x86         /* Cyan    foreground */
#define WHF 0x87         /* White   foreground */
/*      FSH 0x88            Flashing begin  */
/*      STD 0x89            Flashing steady */
/*      EBX 0x8a            End of window   */
/*      SBX 0x8b            Start of window */
/*      NSZ 0x8c            Normal size     */
/*      DBH 0x8d            Double height   */
/*      DBW 0x8e            Double width    */
/*      DBS 0x8f            Double size     */
#define BKB 0x90         /* Black   background */
#define RDB 0x91         /* Red     background */
#define GRB 0x92         /* Green   background */
#define YLB 0x93         /* Yellow  background */
#define BLB 0x94         /* Blue    background */
#define MGB 0x95         /* Magenta background */
#define CNB 0x96         /* Cyan    background */
#define WHB 0x97         /* White   background */
/*      CDY 0x98            Conceal display  */
/*      SPL 0x99            Stop lining      */
/*      STL 0x9a            Start lining     */
/*      CSI 0x9b            Control sequence introducer */
#define NPO 0x9c         /* Normal polarity        */
#define IPO 0x9d         /* Inverted polarity      */
#define TRB 0x9e         /* Transparent background */
#define STC 0x9f         /* Stop conceal           */



/*
 * control code sequences (annex 6)
 *

US 0x20  TFI ...         (annex 7.3)
US 0x23  define DRCS     (page 86)
US 0x26  define color
US 0x2d  define format
US 0x2f  reset
US <y> <x>   position cursor at x, y

ESC 0x22 0x40  invoke serial   C1 set       (page 116)
ESC 0x22 0x41  invoke parallel C1 set
ESC 0x23 0x21 <Fe>  full row    attributes  (page 134)
ESC 0x23 0x20 <Fe>  full screen attributes  (page 136)
              <Fe> = BKB(C1P 0x50), RDB(C1P 0x51), ....

ESC 0x28 <Fi>    G0
ESC 0x29 <Fi>    G1
ESC 0x2a <Fi>    G2
ESC 0x2b <Fi>    G3
     <Fi> = 0x40        primary graphic set
            0x62        suppl.  graphic set
            0x63        2nd suppl. mosaic set
            0x64        3rd suppl. mosaic set
            0x20 0x40   DRCS

ESC 0x7e  LS1R   locking shift 1 right  (page 114)
ESC 0x6e  LS2    locking shift 2
ESC 0x7d  LS2R   locking shift 2 right
ESC 0x6f  LS3    locking shift 3
ESC 0x7c  LS3R   locking shift 3 right


CSI 0x30 0x41  IVF    inverted flash          (page 131)
CSI 0x31 0x41  RIF    reduced intesity flash
CSI 0x32 0x41  FF1    fast flash 1
CSI 0x33 0x41  FF2    fast flash 2
CSI 0x34 0x41  FF3    fast flash 3
CSI 0x35 0x41  ICF    increment flash
CSI 0x36 0x41  DCF    decrement flash

CSI 0x42  STC    stop conceal (same as C1P STC)  (page 133)

CSI 0x32 0x53  MMS    marked mode start
CSI 0x32 0x54  MMT    makred mode stop
 
CSI 0x31 0x50  PMS    protected mode start
CSI 0x31 0x51  PMC    protected mode cancel

CSI 0x30 0x40  CT1    color table 1  (page 137)
CSI 0x31 0x40  CT2    color table 2
CSI 0x32 0x40  CT3    color table 3
CSI 0x33 0x40  CT4    color table 4

CSI <URT> <URU> 0x3b <LRT> <LRU> 0x55   CSA   create scrolling area
CSI <URT> <URU> 0x3b <LRT> <LRU> 0x56   DSA   delete scrolling area
CSI 0x30 0x60  SCU    scroll up
CSI 0x31 0x60  SCD    scroll down
CSI 0x32 0x60  AIS    activate   implicit scrolling
CSI 0x33 0x60  DIS    deactivate implicit scrolling

 */
