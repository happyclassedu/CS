/*
    Crystal Space Windowing System: Windowing System Application class
    Copyright (C) 1998,1999,2000 by Andrew Zabolotny <bit@eltech.ru>

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

#include <limits.h>
#include <stdarg.h>
#include "cssysdef.h"
#include "qint.h"
#include "csutil/inifile.h"
#include "csutil/csstrvec.h"
#include "csutil/scanstr.h"
#include "cssys/cseventq.h"
#include "csws/cslistbx.h"
#include "csws/csmouse.h"
#include "csws/csmenu.h"
#include "csws/cswindow.h"
#include "csws/csdialog.h"
#include "csws/csapp.h"
#include "csws/cswsutil.h"
#include "isystem.h"
#include "ivfs.h"
#include "itxtmgr.h"
#include "ievent.h"

// Archive path of CSWS.CFG
#define CSWS_CFG "csws.cfg"

CS_TOKEN_DEF_START
  CS_TOKEN_DEF (MOUSECURSOR)
  CS_TOKEN_DEF (TITLEBUTTON)
  CS_TOKEN_DEF (DIALOGBUTTON)
  CS_TOKEN_DEF (TEXTURE)
  CS_TOKEN_DEF (TRANSPARENT)
  CS_TOKEN_DEF (FILE)
  CS_TOKEN_DEF (MIPMAP)
  CS_TOKEN_DEF (DITHER)
CS_TOKEN_DEF_END

//--//--//--//--//--//--//--//--//--//--//--//--//--//--//--//- csAppPlugIn //--

IMPLEMENT_IBASE (csApp::csAppPlugIn)
  IMPLEMENTS_INTERFACE (iPlugIn)
IMPLEMENT_IBASE_END

csApp::csAppPlugIn::csAppPlugIn (csApp *iParent)
{
  CONSTRUCT_IBASE (NULL);
  app = iParent;
}

bool csApp::csAppPlugIn::Initialize (iSystem *System)
{
  app->VFS = QUERY_PLUGIN_ID (System, CS_FUNCID_VFS, iVFS);
  if (!app->VFS) return false;

  // Get system event outlet for faster access
  app->EventOutlet = System->GetSystemEventOutlet ();

  // We want ALL the events! :)
  return System->CallOnEvents (this, unsigned (-1));
}

bool csApp::csAppPlugIn::HandleEvent (iEvent &Event)
{
  // If this is a pre-process or post-process event,
  // do the respective work ...
  if (Event.Type == csevBroadcast
   && Event.Command.Code == cscmdPreProcess)
    app->StartFrame ();

  // Send the event to all child components
  bool rc = app->PreHandleEvent (Event)
         || app->HandleEvent (Event)
         || app->PostHandleEvent (Event);

  if (Event.Type == csevBroadcast
   && Event.Command.Code == cscmdPostProcess)
    app->FinishFrame ();

  return rc;
}

//--//--//--//--//--//--//--//--//--//--//--//--//--//--//--//--//-- csApp -//--

csApp::csApp (iSystem *SysDriver) : csComponent (NULL), scfiPlugIn (this)
{
  app = this;			// so that all inserted windows will inherit it
  MouseOwner = NULL;		// no mouse owner
  KeyboardOwner = NULL;		// no keyboard owner
  FocusOwner = NULL;		// no focus owner
  titlebardefs =
  dialogdefs = NULL;
  RedrawFlag = true;
  WindowListChanged = false;
  LoopLevel = 0;
  BackgroundStyle = csabsSolid;
  insert = true;
  SetFontSize (12);
  GfxPpl = NULL;
  VFS = NULL;
  InFrame = false;
  (System = SysDriver)->IncRef ();

  OldMouseCursorID = csmcNone;
  MouseCursorID = csmcArrow;

  SetPalette (CSPAL_APP);
  state |= CSS_VISIBLE | CSS_SELECTABLE | CSS_FOCUSED;

  Mouse = new csMouse (this);
  theme = NULL;
}

csApp::~csApp ()
{
  System->UnloadPlugIn (&scfiPlugIn);

  // Delete all children prior to shutting down the system
  DeleteAll ();

  delete titlebardefs;
  delete dialogdefs;

  delete Mouse;
  delete GfxPpl;
  if (VFS) VFS->DecRef ();
  // Delete all textures prior to deleting the texture manager
  Textures.DeleteAll ();

  System->DecRef ();
}

bool csApp::InitialSetup ()
{
  if (!System->RegisterPlugIn ("crystalspace.windowing.application", "CSWS", &scfiPlugIn))
    return false;

  // Change to the directory on VFS where we keep our data
  const char *DataDir = System->ConfigGetStr ("CSWS", "Library", NULL);
  if (DataDir)
    VFS->ChDir (DataDir);

  // Create the graphics pipeline
  GfxPpl = new csGraphicsPipeline (System);

  ScreenWidth = GfxPpl->G2D->GetWidth ();
  ScreenHeight = GfxPpl->G2D->GetHeight ();
  bound.Set (0, 0, ScreenWidth, ScreenHeight);
  dirty.Set (bound);
  WindowListWidth = ScreenWidth / 3;
  WindowListHeight = ScreenWidth / 6;

  LoadConfig ();
  // Compute and set the work palette (instead of console palette)
  PrepareTextures ();

  return true;
}

void csApp::printf (int mode, char* str, ...)
{
  char buf[1024];
  va_list arg;
  va_start (arg, str);
  vsprintf (buf, str, arg);
  va_end (arg);
  System->Printf (mode, "%s", buf);
}

void csApp::SetBackgroundStyle (csAppBackgroundStyle iBackgroundStyle)
{
  BackgroundStyle = iBackgroundStyle;
  Invalidate ();
}

void csApp::ShutDown ()
{
  EventOutlet->Broadcast (cscmdQuit);
}

bool csApp::LoadTexture (const char *iTexName, const char *iTexParams,
  int iFlags)
{
  CS_TOKEN_TABLE_START (texcommands)
    CS_TOKEN_TABLE (FILE)
    CS_TOKEN_TABLE (TRANSPARENT)
    CS_TOKEN_TABLE (MIPMAP)
    CS_TOKEN_TABLE (DITHER)
  CS_TOKEN_TABLE_END

  const char *filename = iTexName;
  char *buffer = strnew (iTexParams);
  char *bufptr = buffer;
  bool transp = false;
  float tr = 0, tg = 0, tb = 0;

  int cmd;
  char *name, *params;
  while ((cmd = csGetObject (&bufptr, texcommands, &name, &params)) > 0)
    switch (cmd)
    {
      case CS_TOKEN_FILE:
        filename = params;
        break;
      case CS_TOKEN_TRANSPARENT:
        transp = true;
        ScanStr (params, "%f,%f,%f", &tr, &tg, &tb);
        break;
      case CS_TOKEN_MIPMAP:
        if (strcasecmp (params, "yes") == 0)
          iFlags &= ~CS_TEXTURE_NOMIPMAPS;
        else if (strcasecmp (params, "no") == 0)
          iFlags |= CS_TEXTURE_NOMIPMAPS;
        else
          printf (MSG_WARNING, "Warning! Invalid MIPMAP() value, 'yes' or 'no' expected\n");
        break;
      case CS_TOKEN_DITHER:
        if (strcasecmp (params, "yes") == 0)
          iFlags |= CS_TEXTURE_DITHER;
        else if (strcasecmp (params, "no") == 0)
          iFlags &= ~CS_TEXTURE_DITHER;
        else
          printf (MSG_WARNING, "Warning! Invalid MIPMAP() value, 'yes' or 'no' expected\n");
        break;
    }

  if (cmd == CS_PARSERR_TOKENNOTFOUND)
  {
    printf (MSG_WARNING, "Unknown texture parameter keyword detected: '%s'\n", bufptr);
    delete [] buffer;
    return false;
  }

  // Now load the texture
  size_t size;
  char *fbuffer = VFS->ReadFile (filename, size);
  delete [] buffer;
  if (!fbuffer || !size)
  {
    printf (MSG_WARNING, "Cannot read image file \"%s\" from VFS\n", filename);
    return false;
  }

  iTextureManager *txtmgr = GfxPpl->G3D->GetTextureManager ();
  iImage *image = csImageLoader::Load ((UByte *)fbuffer, size,
    txtmgr->GetTextureFormat ());
  delete [] fbuffer;

  csWSTexture *tex = new csWSTexture (iTexName, image, iFlags);
  image->DecRef ();
  if (transp)
    tex->SetKeyColor (QInt (tr * 255.), QInt (tg * 255.), QInt (tb * 255.));
  Textures.Push (tex);

  return true;
}

void csApp::LoadConfig ()
{
  CS_TOKEN_TABLE_START (commands)
    CS_TOKEN_TABLE (MOUSECURSOR)
    CS_TOKEN_TABLE (TITLEBUTTON)
    CS_TOKEN_TABLE (DIALOGBUTTON)
    CS_TOKEN_TABLE (TEXTURE)
  CS_TOKEN_TABLE_END

  if (!titlebardefs)
    titlebardefs = new csStrVector (16, 16);
  if (!dialogdefs)
    dialogdefs = new csStrVector (16, 16);

  size_t cswscfglen;
  char *cswscfg = VFS->ReadFile (CSWS_CFG, cswscfglen);
  if (!cswscfg)
  {
    printf (MSG_FATAL_ERROR, "ERROR: CSWS config file `%s' not found\n", CSWS_CFG);
    fatal_exit (0, true);			// cfg file not found
    return;
  }

  int cmd;
  char *cfg = cswscfg, *name, *params;
  while ((cmd = csGetObject (&cfg, commands, &name, &params)) > 0)
    switch (cmd)
    {
      case CS_TOKEN_MOUSECURSOR:
      {
        Mouse->NewPointer (name, params);
        break;
      }
      case CS_TOKEN_TITLEBUTTON:
      {
        char *tmp = new char [strlen (name) + 1 + strlen (params) + 1];
        sprintf (tmp, "%s %s", name, params);
        titlebardefs->Push (tmp);
        break;
      }
      case CS_TOKEN_DIALOGBUTTON:
      {
        char *tmp = new char [strlen (name) + 1 + strlen (params) + 1];
        sprintf (tmp, "%s %s", name, params);
        dialogdefs->Push (tmp);
        break;
      }
      case CS_TOKEN_TEXTURE:
        LoadTexture (name, params, CS_TEXTURE_2D);
        break;
      default:
        printf (MSG_FATAL_ERROR, "Unknown token in "CSWS_CFG"! (%s)\n", cfg);
        fatal_exit (0, false);			// Unknown token
    }
  delete[] cswscfg;
}

void csApp::PrepareTextures ()
{
  iTextureManager *txtmgr = GfxPpl->G3D->GetTextureManager ();

  // Clear all textures from texture manager
  txtmgr->ResetPalette ();

  // Create a uniform palette: r(3)g(3)b(2)
  int r,g,b;
  for (r = 0; r < 8; r++)
    for (g = 0; g < 8; g++)
      for (b = 0; b < 4; b++)
        txtmgr->ReserveColor (r * 32, g * 32, b * 64);

  // Register all CSWS textures to the texture manager
  for (int i = 0; i < Textures.Length (); i++)
    Textures.Get (i)->Register (txtmgr);

  // Prepare all the textures.
  txtmgr->PrepareTextures ();
  // Set the palette (256-color mode)
  txtmgr->SetPalette ();
  // Look in palette for colors we need for windowing system
  SetupPalette ();
  // Finally, set up mouse pointer images
  Mouse->Setup ();
  // Invalidate entire screen
  Invalidate (true);
}

void csApp::SetupPalette ()
{
  csPixelFormat *pfmt = GfxPpl->G2D->GetPixelFormat ();
  PhysColorShift = ((pfmt->RedShift >= 24) || (pfmt->GreenShift >= 24)
    || (pfmt->BlueShift >= 24)) ? 8 : 0;

  iTextureManager* txtmgr = GfxPpl->G3D->GetTextureManager ();
  Pal [cs_Color_Black]   = txtmgr->FindRGB (  0,   0,   0);
  Pal [cs_Color_White]   = txtmgr->FindRGB (255, 255, 255);
  Pal [cs_Color_Gray_D]  = txtmgr->FindRGB (128, 128, 128);
  Pal [cs_Color_Gray_M]  = txtmgr->FindRGB (160, 160, 160);
  Pal [cs_Color_Gray_L]  = txtmgr->FindRGB (204, 204, 204);
  Pal [cs_Color_Blue_D]  = txtmgr->FindRGB (  0,  20,  80);
  Pal [cs_Color_Blue_M]  = txtmgr->FindRGB (  0,  44, 176);
  Pal [cs_Color_Blue_L]  = txtmgr->FindRGB (  0,  64, 255);
  Pal [cs_Color_Green_D] = txtmgr->FindRGB ( 20,  80,  20);
  Pal [cs_Color_Green_M] = txtmgr->FindRGB ( 44, 176,  44);
  Pal [cs_Color_Green_L] = txtmgr->FindRGB ( 64, 255,  64);
  Pal [cs_Color_Red_D]   = txtmgr->FindRGB ( 80,   0,   0);
  Pal [cs_Color_Red_M]   = txtmgr->FindRGB (176,   0,   0);
  Pal [cs_Color_Red_L]   = txtmgr->FindRGB (255,   0,   0);
  Pal [cs_Color_Cyan_D]  = txtmgr->FindRGB (  0,  60,  80);
  Pal [cs_Color_Cyan_M]  = txtmgr->FindRGB (  0, 132, 176);
  Pal [cs_Color_Cyan_L]  = txtmgr->FindRGB (  0, 192, 255);
  Pal [cs_Color_Brown_D] = txtmgr->FindRGB ( 80,  60,  20);
  Pal [cs_Color_Brown_M] = txtmgr->FindRGB (176, 132,  44);
  Pal [cs_Color_Brown_L] = txtmgr->FindRGB (255, 192,  64);
}

int csApp::FindColor (int r, int g, int b)
{
  int color;
  iTextureManager *txtmgr = GfxPpl->G3D->GetTextureManager ();
  color = txtmgr->FindRGB (r, g, b);
  return (color >> PhysColorShift) | 0x80000000;
}

void csApp::Idle ()
{
  System->Sleep (1);
}

void csApp::StartFrame ()
{
  if (InFrame)
    FinishFrame ();
  InFrame = true;

  cs_time elapsed_time;
  System->GetElapsedTime (elapsed_time, CurrentTime);

  GfxPpl->StartFrame (Mouse);
}

void csApp::FinishFrame ()
{
  if (!InFrame)
    StartFrame ();

  // Consider ourselves idle if we don't have anything to redraw in this frame
  bool do_idle = !RedrawFlag;

  // Redraw all changed windows
  while (RedrawFlag)
  {
    RedrawFlag = false;
    csEvent ev (0, csevBroadcast, cscmdRedraw);
    HandleEvent (ev);
  }

  // Now update screen
  if (OldMouseCursorID != MouseCursorID)
  {
    do_idle = false;
    Mouse->SetCursor (MouseCursorID);
    OldMouseCursorID = MouseCursorID;
  }

  // Flush application graphics operations
  GfxPpl->FinishFrame (Mouse);

  if (do_idle)
    Idle ();

  InFrame = false;
}

bool csApp::PreHandleEvent (iEvent &Event)
{
  if (MouseOwner && IS_MOUSE_EVENT (Event))
  {
    // Bring mouse coordinates to child coordinate system
    int dX = 0, dY = 0;
    MouseOwner->LocalToGlobal (dX, dY);
    Event.Mouse.x -= dX;
    Event.Mouse.y -= dY;
    bool ret = MouseOwner->PreHandleEvent (Event);
    Event.Mouse.x += dX;
    Event.Mouse.y += dY;
    return ret;
  }
  else if (KeyboardOwner && IS_KEYBOARD_EVENT (Event))
    return KeyboardOwner->PreHandleEvent (Event);
  else if (FocusOwner)
    return FocusOwner->PreHandleEvent (Event);
  else
    return csComponent::PreHandleEvent (Event);
}

bool csApp::PostHandleEvent (iEvent &Event)
{
  if (MouseOwner && IS_MOUSE_EVENT (Event))
  {
    // Bring mouse coordinates to child coordinate system
    int dX = 0, dY = 0;
    MouseOwner->LocalToGlobal (dX, dY);
    Event.Mouse.x -= dX;
    Event.Mouse.y -= dY;
    bool ret = MouseOwner->PostHandleEvent (Event);
    Event.Mouse.x += dX;
    Event.Mouse.y += dY;
    return ret;
  }
  else if (KeyboardOwner && IS_KEYBOARD_EVENT (Event))
    return KeyboardOwner->PostHandleEvent (Event);
  else if (FocusOwner)
    return FocusOwner->PostHandleEvent (Event);
  else
    return csComponent::PostHandleEvent (Event);
}

bool csApp::HandleEvent (iEvent &Event)
{
  // Mouse should always receive events
  // to reflect current mouse position
  Mouse->HandleEvent (Event);

  if ((Event.Type == csevKeyDown)
   && (Event.Key.Code == CSKEY_INS))
    insert = !insert;

  // If mouse moves, reset mouse cursor to arrow
  if (Event.Type == csevMouseMove)
    SetMouse (csmcArrow);

  // If a component captured the mouse,
  // forward all mouse (and keyboard) events to it
  if (MouseOwner && (IS_MOUSE_EVENT (Event) || IS_KEYBOARD_EVENT (Event)))
  {
    if (IS_MOUSE_EVENT (Event))
      MouseOwner->GlobalToLocal (Event.Mouse.x, Event.Mouse.y);
    return MouseOwner->HandleEvent (Event);
  } /* endif */

  // If a component captured the keyboard, forward all keyboard events to it
  if (KeyboardOwner && IS_KEYBOARD_EVENT (Event))
  {
    return KeyboardOwner->HandleEvent (Event);
  } /* endif */

  // If a component captured events, forward all focused events to it
  if (FocusOwner && (IS_MOUSE_EVENT (Event) || IS_KEYBOARD_EVENT (Event)))
  {
    if (IS_MOUSE_EVENT (Event))
      FocusOwner->GlobalToLocal (Event.Mouse.x, Event.Mouse.y);
    return FocusOwner->HandleEvent (Event);
  } /* endif */

  // Send event to children as appropiate
  if (csComponent::HandleEvent (Event))
    return true;

  // Handle 'window list' event
  if ((Event.Type == csevMouseDown)
   && (((System->GetMouseButton (1) && System->GetMouseButton (2)))
    || (Event.Mouse.Button == 3)))
  {
    WindowList ();
    return true;
  } /* endif */

  return false;
}

void csApp::FlushEvents ()
{
  System->NextFrame ();
}

void csApp::WindowList ()
{
  csWindowList *wl = new csWindowList (this);
  int x, y;
  GetMouse ()->GetPosition (x, y);
  int w = WindowListWidth;
  int h = WindowListHeight;
  csRect rect (x - w/2, y - h/2, x + w/2, y + h/2);
  if (rect.xmin < 0)
    rect.Move (-rect.xmin, 0);
  if (rect.ymin < 0)
    rect.Move (0, -rect.ymin);
  if (rect.xmax > bound.xmax)
    rect.Move (-(rect.xmax - bound.xmax), 0);
  if (rect.ymax > bound.ymax)
    rect.Move (0, -(rect.ymax - bound.ymax));
  ((csComponent *)wl)->SetRect (rect);
  wl->Select ();
}

void csApp::Draw ()
{
  switch (BackgroundStyle)
  {
    case csabsNothing:
      break;
    case csabsSolid:
      Clear (CSPAL_APP_WORKSPACE);
      break;
  } /* endswitch */
  csComponent::Draw ();
}

bool csApp::GetKeyState (int iKey)
{
  return System->GetKeyState (iKey);
}

void csApp::Insert (csComponent *comp)
{
  csComponent::Insert (comp);
  WindowListChanged = true;
}

void csApp::Delete (csComponent *comp)
{
  csComponent::Delete (comp);
  WindowListChanged = true;
}

int csApp::Execute (csComponent *comp)
{
  csComponent *oldFocusOwner = CaptureFocus (comp);

  comp->Select ();
  comp->SetState (CSS_MODAL, true);
  LoopLevel++;
  System->Loop ();
  LoopLevel--;
  comp->SetState (CSS_MODAL, false);

  CaptureFocus (oldFocusOwner);
  return DismissCode;
}

void csApp::Dismiss (int iCode)
{
  if (LoopLevel)
  {
    DismissCode = iCode;
    EventOutlet->Broadcast (cscmdQuitLoop);
  } /* endif */
}

void csApp::SetTheme (csTheme *nTheme)
{
  delete theme;
  if (!nTheme)
    nTheme = new csTheme (this);
  theme = nTheme;
}

csTheme *csApp::GetTheme ()
{
  if (!theme)
    theme = new csTheme (this);
  return theme;
}
