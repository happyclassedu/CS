/*
    Copyright (C) 1998 by Jorrit Tyberghein

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdarg.h>

#include "sysdef.h"
#include "cscom/com.h"
#include "cs2d/ggi/ggi2d.h"
#include "cssys/unix/iunix.h"
#include "csgeom/csrect.h"
#include "csinput/csinput.h"
#include "isystem.h"

#include <ggi/ggi.h>

BEGIN_INTERFACE_TABLE(csGraphics2DGGI)
  IMPLEMENTS_COMPOSITE_INTERFACE_EX( IGraphics2D, XGraphics2D )
  IMPLEMENTS_COMPOSITE_INTERFACE_EX( IGraphicsInfo, XGraphicsInfo )
END_INTERFACE_TABLE()

IMPLEMENT_UNKNOWN_NODELETE(csGraphics2DGGI)

// csGraphics2DGGI functions
csGraphics2DGGI::csGraphics2DGGI(ISystem* piSystem) : csGraphics2D (piSystem)
{
  System = piSystem;

  if (FAILED (System->QueryInterface (IID_IUnixSystemDriver, (void**)&UnixSystem)))
  {
    CsPrintf (MSG_FATAL_ERROR, "FATAL: The system driver does not support "
                               "the IUnixSystemDriver interface\n");
    exit (-1);
  }

}

void csGraphics2DGGI::Initialize ()
{
  csGraphics2D::Initialize ();

  Font = 0;
  Memory = NULL;

  // GGI Starts here

  CsPrintf (MSG_INITIALIZATION, "Crystal Space LibGGI version.\n");
  CsPrintf (MSG_INITIALIZATION,  "Using %dx%dx%d resolution.\n\n", Width, Height, Depth);

  Memory = NULL;

  if (ggiInit() != 0) {
    CsPrintf (MSG_FATAL_ERROR, "Couldn't initialize LibGGI\n");
    exit (1);
  }

  vis = ggiOpen(NULL);

  if (vis == NULL) {
    CsPrintf (MSG_FATAL_ERROR, "Couldn't open GGI visual\n");
    exit (2);
  }
 
  ggiParseMode("", &vis_mode);

  vis_mode.visible.x = Width;
  vis_mode.visible.y = Height;
  vis_mode.graphtype = Depth;

  ggiCheckMode(vis, &vis_mode);

  if (ggiSetMode(vis, &vis_mode) < 0) {
    CsPrintf (MSG_FATAL_ERROR, "Couldn't set mode\n");
    exit (3);
  }
  // Put GGI into async mode
  ggiSetFlags (vis, GGIFLAG_ASYNC);

  display_width  = vis_mode.virt.x;
  display_height = vis_mode.virt.y;

  // calculate pixel format
  
  const ggi_pixelformat *ggi_pf = ggiGetPixelFormat(vis);

  if (ggi_pf == NULL) {
    CsPrintf (MSG_FATAL_ERROR, "Couldn't get pixel format\n");
    exit (4);
  }
  
  pfmt.RedMask    = 0;
  pfmt.GreenMask  = 0;
  pfmt.BlueMask   = 0;
  pfmt.PalEntries = 0;
  pfmt.PixelBytes = (GT_SIZE(vis_mode.graphtype)+7) / 8;

  switch (GT_SCHEME(vis_mode.graphtype)) {

    case GT_PALETTE:
      pfmt.PalEntries = 1 << GT_DEPTH(vis_mode.graphtype);
      break;

    case GT_TRUECOLOR:
      pfmt.RedMask    = ggi_pf->red_mask;
      pfmt.GreenMask  = ggi_pf->green_mask;
      pfmt.BlueMask   = ggi_pf->blue_mask;
      break;

    default:
      CsPrintf (MSG_FATAL_ERROR, "Unsupported pixel format\n");
      exit (5);
  }

  switch (pfmt.PixelBytes)
  {
    case 1:
      break;

    case 2:
      DrawPixel = DrawPixel16;
      WriteChar = WriteChar16;
      GetPixelAt = GetPixelAt16;
      DrawSprite = DrawSprite16;
      break;

    case 3:
      //DrawPixel = DrawPixel24;
      //WriteChar = WriteChar24;
      //GetPixelAt = GetPixelAt24;
      //DrawSprite = DrawSprite24;
      break;

    case 4:
      DrawPixel = DrawPixel32;
      WriteChar = WriteChar32;
      GetPixelAt = GetPixelAt32;
      DrawSprite = DrawSprite32;
      break;
  }

  complete_pixel_format();
}

csGraphics2DGGI::~csGraphics2DGGI(void)
{
  // Destroy your graphic interface
  Close();

  if (Memory) {
    delete [] Memory;
    Memory = NULL;
  }

  if (vis) {
    ggiClose(vis);
    vis = NULL;
  }

  ggiExit();
}

// Used to printf through system driver
void csGraphics2DGGI::CsPrintf (int msgtype, char *format, ...)
{
  va_list arg;
  char buf[256];

  va_start (arg, format);
  vsprintf (buf, format, arg);
  va_end (arg);

  System->Print (msgtype, buf);
}

bool csGraphics2DGGI::Open(char *Title)
{
  // Open your graphic interface
  if (!csGraphics2D::Open (Title)) return false;

  // Set loop callback
  UnixSystem->SetLoopCallback (ProcessEvents, this);

  Memory = CHK( new unsigned char [Width*Height*pfmt.PixelBytes] );

  Clear(0);
  return true;
}

void csGraphics2DGGI::Close(void)
{
  // Close your graphic interface

  csGraphics2D::Close ();
}

// ---------------------------------------------------------------------- //

#define GGI_EVENTS  (ggi_event_mask) (emKey | emPointer)

int csGraphics2DGGI::translate_key (ggi_event *ev)
{
  // Normal Latin-1 key ?
  if (GII_KTYP(ev->key.sym) == GII_KT_LATIN1) {
    switch (ev->key.sym)
    {
      case GIIUC_Tab:        return CSKEY_TAB;
      case GIIUC_Escape:     return CSKEY_ESC;
      case GIIUC_Linefeed:
      case GIIUC_Return:     return CSKEY_ENTER;
      case GIIUC_Delete:
      case GIIUC_BackSpace:  return CSKEY_BACKSPACE;
    }
    return GII_KVAL(ev->key.sym);
  }

  // Function key ?
  if (GII_KTYP(ev->key.sym) == GII_KT_FN)
  {
    int func = GII_KVAL(ev->key.sym);
    if ((1 <= func) && (func <= 12)) {
      return CSKEY_F1 + func - 1;
    }
  }

  // Shift key ?
  switch (ev->key.sym)
  {
    case GIIK_Shift: return CSKEY_SHIFT;
    case GIIK_Meta:
    case GIIK_Alt:
    case GIIK_AltGr: return CSKEY_ALT;
    case GIIK_Ctrl:  return CSKEY_CTRL;
  }

  // Special key ?
  //
  // The "label" field contains the key without being affected by
  // any modifiers (shift, alt or whatever).
          
  switch(ev->key.label)
  {
    case GIIK_Enter:   return CSKEY_ENTER;
    case GIIK_Insert:  return CSKEY_INS;
    case GIIK_P5:      return CSKEY_CENTER;

    case GIIK_Up:       case GIIK_P8: return CSKEY_UP;
    case GIIK_Down:     case GIIK_P2: return CSKEY_DOWN;
    case GIIK_Right:    case GIIK_P6: return CSKEY_RIGHT;
    case GIIK_Left:     case GIIK_P4: return CSKEY_LEFT;
    case GIIK_Home:     case GIIK_P7: return CSKEY_HOME;
    case GIIK_End:      case GIIK_P1: return CSKEY_END;
    case GIIK_PageUp:   case GIIK_P9: return CSKEY_PGUP;
    case GIIK_PageDown: case GIIK_P3: return CSKEY_PGDN;
  }

  return -1;
}

void csGraphics2DGGI::ProcessEvents (void* Param)
{
  csGraphics2DGGI *Self = (csGraphics2DGGI *)Param;

  ggi_event ev;
  struct timeval tv;

  tv.tv_sec  = 0;
  tv.tv_usec = 0;

  while (ggiEventPoll(Self->vis, GGI_EVENTS, &tv) != 0)
  { 
    ggiEventRead(Self->vis, &ev, GGI_EVENTS);

    switch (ev.any.type)
    {
      case evKeyPress:
      case evKeyRepeat:
      case evKeyRelease:
      {
        int key  = Self->translate_key(&ev);
        int down = (ev.any.type != evKeyRelease);

        if (key >= 0)
          Self->UnixSystem->KeyboardEvent (key, down);
        break;
      }

      // NOTE: mouse not yet implemented...
    }
  }
}

void csGraphics2DGGI::Print (csRect *area)
{
  (void) area;

  ggiPutBox(vis, 0, 0, Width, Height, Memory);
  ggiFlush(vis);
}

void csGraphics2DGGI::SetRGB(int i, int r, int g, int b)
{
  ggi_color col;

  col.r = r << 8;
  col.g = g << 8;
  col.b = b << 8;

  ggiSetPalette(vis, i, 1, &col);
  ggiFlush(vis);

  csGraphics2D::SetRGB (i, r, g, b);
}

bool csGraphics2DGGI::PerformExtension(char* args)
{
  (void) args;

  return true;
}
