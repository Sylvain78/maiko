/* $Id: xinit.c,v 1.5 2001/12/26 22:17:06 sybalsky Exp $ (C) Copyright Venue, All Rights Reserved */

/************************************************************************/
/*									*/
/*	(C) Copyright 1989, 1990, 1990, 1991, 1992, 1993, 1994, 1995 Venue.	*/
/*	    All Rights Reserved.		*/
/*	Manufactured in the United States of America.			*/
/*									*/
/************************************************************************/

#include "version.h"

#include <X11/X.h>        // for NoEventMask, MSBFirst, StructureNotifyMask
#include <X11/Xlib.h>     // for XSelectInput, XImage, XFlush, DefaultScreen
#include <assert.h>       // for assert
#include <signal.h>       // for sig_atomic_t
#include <stdbool.h>      // for false, bool, true
#include <stdio.h>        // for NULL
#include "adr68k.h"       // for NativeAligned4FromLAddr
#include "dbprint.h"      // for TPRINT
#include "devif.h"        // for (anonymous), MRegion, DspInterface, OUTER_S...
#include "dspifdefs.h"    // for GenericReturnT
#include "lispemul.h"     // for BITSPER_DLWORD, DLword, LispPTR
#include "xbbtdefs.h"     // for clipping_Xbitblt
#include "xdefs.h"        // for XLOCK, XUNLOCK, DEF_BDRWIDE, DEF_WIN_HEIGHT
#include "xinitdefs.h"    // for Open_Display, X_init, Xevent_after_raid
#include "xlspwindefs.h"  // for Create_LispWindow

/* DISPLAY_MAX same magic number is in ldsout.c */
#define DISPLAY_MAX (65536 * 16 * 2)

extern char Display_Name[128];
extern DLword *DisplayRegion68k;
extern int noscroll;

extern bool Lisp_Xinitialized;
bool Lisp_Xinitialized = false;
extern int xsync;
int xsync = False;

extern int LispWindowRequestedX;
extern int LispWindowRequestedY;
extern unsigned LispWindowRequestedWidth;
extern unsigned LispWindowRequestedHeight;
int LispWindowRequestedX = 0;
int LispWindowRequestedY = 0;
unsigned LispWindowRequestedWidth = DEF_WIN_WIDTH;
unsigned LispWindowRequestedHeight = DEF_WIN_HEIGHT;

extern int LispDisplayRequestedX, LispDisplayRequestedY;
extern unsigned LispDisplayRequestedWidth, LispDisplayRequestedHeight;
int LispDisplayRequestedX, LispDisplayRequestedY;
unsigned LispDisplayRequestedWidth, LispDisplayRequestedHeight;

extern Colormap Colors;
Colormap Colors;

extern volatile sig_atomic_t XLocked;
volatile sig_atomic_t XLocked = 0; /* non-zero while doing X ops, to avoid signals */
extern volatile sig_atomic_t XNeedSignal;
volatile sig_atomic_t XNeedSignal = 0; /* T if an X interrupt happened while XLOCK asserted */

/* ubound: return (unsigned) value if it is between lower and upper otherwise lower or upper */
static inline unsigned ubound(unsigned lower, unsigned value, unsigned upper)
{
  if (value <= lower)
    return (lower);
  else if (value >= upper)
    return (upper);
  else
    return (value);
}

/************************************************************************/
/*									*/
/*			i n i t _ X e v e n t				*/
/*									*/
/*	Turn on the X window we've been using for display.		*/
/*									*/
/************************************************************************/
void init_Xevent(DspInterface dsp)
{
  int GravMask, BarMask;

  GravMask =
      ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | StructureNotifyMask;
  BarMask = GravMask;

  XSelectInput(dsp->display_id, dsp->LispWindow, dsp->EnableEventMask);
  XSelectInput(dsp->display_id, dsp->DisplayWindow, dsp->EnableEventMask);
  if (noscroll == 0) {
    XSelectInput(dsp->display_id, dsp->HorScrollBar, BarMask);
    XSelectInput(dsp->display_id, dsp->VerScrollBar, BarMask);
    XSelectInput(dsp->display_id, dsp->HorScrollButton, NoEventMask);
    XSelectInput(dsp->display_id, dsp->VerScrollButton, NoEventMask);
    XSelectInput(dsp->display_id, dsp->NEGrav, GravMask);
    XSelectInput(dsp->display_id, dsp->SEGrav, GravMask);
    XSelectInput(dsp->display_id, dsp->SWGrav, GravMask);
    XSelectInput(dsp->display_id, dsp->NWGrav, GravMask);
  }
} /*end init_Xevent */

/************************************************************************/
/*									*/
/*			l i s p _ X e x i t				*/
/*									*/
/*	Turn off the X window we've been using for display.		*/
/*									*/
/************************************************************************/
void lisp_Xexit(DspInterface dsp)
{
  assert(Lisp_Xinitialized);

  XLOCK;
  XDestroySubwindows(dsp->display_id, dsp->LispWindow);
  XDestroyWindow(dsp->display_id, dsp->LispWindow);
  XCloseDisplay(dsp->display_id);
  XUNLOCK(dsp);

  Lisp_Xinitialized = false;
} /* end lisp_Xexit */

/************************************************************************/
/*									*/
/*		    X e v e n t _ b e f o r e _ r a i d			*/
/*									*/
/*	Called before Medley enters URAID, to turn off events in	*/
/*	the X windows we use for Medley's display.			*/
/*									*/
/************************************************************************/
void Xevent_before_raid(DspInterface dsp)
{
  TPRINT(("TRACE: Xevent_before_raid()\n"));

  XSelectInput(dsp->display_id, dsp->LispWindow, NoEventMask);
  XSelectInput(dsp->display_id, dsp->DisplayWindow, NoEventMask);
  if (noscroll == 0) {
    XSelectInput(dsp->display_id, dsp->VerScrollBar, NoEventMask);
    XSelectInput(dsp->display_id, dsp->HorScrollBar, NoEventMask);
    XSelectInput(dsp->display_id, dsp->NEGrav, NoEventMask);
    XSelectInput(dsp->display_id, dsp->SEGrav, NoEventMask);
    XSelectInput(dsp->display_id, dsp->SWGrav, NoEventMask);
    XSelectInput(dsp->display_id, dsp->NWGrav, NoEventMask);
  }
  XLOCK;
  XFlush(dsp->display_id);
  XUNLOCK(dsp);
} /* end Xevent_before_raid */

/************************************************************************/
/*									*/
/*		    X e v e n t _ a f t e r _ r a i d			*/
/*									*/
/*	Called after Medley returns from URAID, to re-enable events	*/
/*	from the X server in the windows we use for the display.	*/
/*									*/
/************************************************************************/
void Xevent_after_raid(DspInterface dsp)
{
  init_Xevent(dsp);
  (dsp->bitblt_to_screen)(dsp, 0, dsp->Visible.x, dsp->Visible.y, dsp->Visible.width,
                          dsp->Visible.height);
  XLOCK;
  XFlush(dsp->display_id);
  XUNLOCK(dsp);

} /* end Xevent_after_raid */

/************************************************************************/
/*									*/
/*			O p e n _ D i s p l a y				*/
/*									*/
/*	Open the connection to the X client/window manager display,	*/
/*	gather information from it that we'll need (pixel depth, etc),	*/
/*	and initialize the lisp display-bank size to fit the screen.	*/
/*									*/
/*									*/
/************************************************************************/
void Open_Display(DspInterface dsp)
{
  assert(Lisp_Xinitialized == false);

  /****************************************************/
  /* If debugging, set the X connection so that	*/
  /* we run synchronized--so a debugger can		*/
  /* 'stop in _XError' and know EXACTLY where		*/
  /* an error got caused.				*/
  /****************************************************/
  XSynchronize(dsp->display_id, xsync);

  Colors =
      DefaultColormapOfScreen(ScreenOfDisplay(dsp->display_id, DefaultScreen(dsp->display_id)));

  /* When we make the initial screen we haven't yet read in the */
  /* displayregion bitmap. Fix this now. */
  if (dsp->ScreenBitmap.data == NULL) dsp->ScreenBitmap.data = (char *)DisplayRegion68k;

  Create_LispWindow(dsp); /* Make the main window */
  Lisp_Xinitialized = true;
  init_Xevent(dsp); /* Turn on the event reporting */
} /* end OpenDisplay */

/*********************************************************************/
/*                                                                   */
/*			  X _ i n i t                                */
/* dsp: a display structure to be filled with all the necessary      */
/*      information.                                                 */
/*                                                                   */
/* lispbitmap: The bitmapdata that is to be dumped to the screen.    */
/* width_hint and height_hint: The requested size of the screen. The */
/*      width and height hints will be obeyed as close as possible   */
/*      with respect to the physical limitations. This makes it      */
/*      possible to get a screen that is larger (or smaller) than    */
/*      the physical screen. 0 will give you some default value      */
/* depth_hint: The requested depth in bits. depth 1 can always be    */
/*      granted. Any value of 1 or less will yield depth 1.          */
/*                                                                   */
/* Return value: the lispbitmap or 1 if all is ok. else FALSE is     */
/* returned                                                          */
/*                                                                   */
/* This is the init function for X displays. To create a new display */
/* you call this function. If you want to know what you got you will */
/* have to read the individual fields of the dsp structure.          */
/*                                                                   */
/*********************************************************************/

DspInterface X_init(DspInterface dsp, LispPTR lispbitmap, unsigned width_hint, unsigned height_hint,
                    unsigned depth_hint)
{
  Screen *Xscreen;

  dsp->identifier = Display_Name; /* This is a hack. The display name */
                                  /* has to dealt with in a more */
                                  /* graceful way. */

  /* Try to open the X display. If this isn't possible, we just */
  /* return FALSE. */
  if ((dsp->display_id = XOpenDisplay(dsp->identifier)) == NULL) return (NULL);
  /* Load the dsp structure */

  Xscreen = ScreenOfDisplay(dsp->display_id, DefaultScreen(dsp->display_id));

  /* Set the scrollbar and border widths */
  if (noscroll == 0) {
    dsp->ScrollBarWidth = SCROLL_WIDTH;
    dsp->InternalBorderWidth = DEF_BDRWIDE;
  } else {
    dsp->ScrollBarWidth = 0;
    dsp->InternalBorderWidth = 0;
  }

  dsp->Visible.x = LispDisplayRequestedX;
  dsp->Visible.y = LispDisplayRequestedY;

  /* Set the width and height of the display.  */
  if (height_hint == 0) {
    dsp->Display.height =
      (unsigned)HeightOfScreen(Xscreen) - OUTER_SB_WIDTH(dsp); /* In the default case,
                                                          adjust for scroll gadgets*/
  } else
    dsp->Display.height = ubound(WIN_MIN_HEIGHT, height_hint, WIN_MAX_HEIGHT);

  if (width_hint == 0) {
    dsp->Display.width =
      (unsigned)WidthOfScreen(Xscreen) - OUTER_SB_WIDTH(dsp); /* In the default case,
                                                         adjust for scroll gadgets*/
  } else
    dsp->Display.width = ubound(WIN_MIN_WIDTH, width_hint, WIN_MAX_WIDTH);

  /************************************************************/
  /* 		Set the size of ScreenBitMap                  */
  /* The display's width is rounded to a 32-bit multiple,     */
  /* so that little-Endian machines can display right.        */
  /************************************************************/
  dsp->Display.width = ((dsp->Display.width + 31) >> 5) << 5;

  /*
   * Device methods
   */
  dsp->device.enter = (void (*)(void *))Open_Display;
  dsp->device.exit = (void (*)(void *))lisp_Xexit;
  dsp->device.before_raid = (void (*)(void *))Xevent_before_raid;
  dsp->device.after_raid = (void (*)(void *))Xevent_after_raid;

  dsp->bitblt_to_screen = clipping_Xbitblt;

  dsp->BitGravity = NorthWestGravity;

  dsp->cleardisplay = (unsigned long (*)(DspInterface))GenericReturnT;
  dsp->set_color_map_entry = GenericReturnT;

  /* Set the geometry of the Visible (Lisp) window. */
  dsp->Visible.width =
      ubound(OUTER_SB_WIDTH(dsp), LispWindowRequestedWidth,
            min(dsp->Display.width, (unsigned)WidthOfScreen(Xscreen) - OUTER_SB_WIDTH(dsp)));
  dsp->Visible.height =
      ubound(OUTER_SB_WIDTH(dsp), LispWindowRequestedHeight,
            min(dsp->Display.height, (unsigned)HeightOfScreen(Xscreen) - OUTER_SB_WIDTH(dsp)));

  /* Initialize the screen image structure. */
  dsp->ScreenBitmap.width = (int)dsp->Display.width;
  dsp->ScreenBitmap.height = (int)dsp->Display.height;
  dsp->ScreenBitmap.xoffset = 0;
  dsp->bitsperpixel =
    (unsigned short)DefaultDepthOfScreen(ScreenOfDisplay(dsp->display_id, DefaultScreen(dsp->display_id)));
#if (defined(BYTESWAP))
  dsp->ScreenBitmap.byte_order = LSBFirst;
#else  /* BYTESWAP */
  dsp->ScreenBitmap.byte_order = MSBFirst;
#endif /* BYTESWAP */

  dsp->ScreenBitmap.data = (char *)(lispbitmap ? NativeAligned4FromLAddr((LispPTR)lispbitmap) : 0);

  switch (depth_hint) {
    case 8: /* Color Screen */
      dsp->ScreenBitmap.format = ZPixmap;
      dsp->ScreenBitmap.bitmap_unit = BitmapUnit(dsp->display_id);
      dsp->ScreenBitmap.bitmap_bit_order = MSBFirst;
      dsp->ScreenBitmap.bitmap_pad = 32;
      dsp->ScreenBitmap.depth = 8;
      dsp->ScreenBitmap.bits_per_pixel = 8;
      dsp->ScreenBitmap.bytes_per_line = (int)dsp->Display.width;
      dsp->ScreenBitmap.red_mask = 7;
      dsp->ScreenBitmap.green_mask = 7;
      dsp->ScreenBitmap.blue_mask = 3;
      break;
    default: /* B/W Screen */
      dsp->ScreenBitmap.format = XYBitmap;
      dsp->ScreenBitmap.bitmap_unit = BitmapUnit(dsp->display_id);
      dsp->ScreenBitmap.bitmap_bit_order = MSBFirst;
      dsp->ScreenBitmap.bitmap_pad = 32;
      dsp->ScreenBitmap.depth = 1;
      dsp->ScreenBitmap.bits_per_pixel = 1;
      dsp->ScreenBitmap.bytes_per_line =
          ((dsp->Display.width + (BITSPER_DLWORD - 1)) / BITSPER_DLWORD) * (BITSPER_DLWORD / 8);
      break;
  }
  XInitImage(&dsp->ScreenBitmap);
  return (dsp);
}
