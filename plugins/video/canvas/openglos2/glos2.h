/*
    Crystal Space 2D OpenGL driver for OS/2
    Copyright (C) 1998 by Jorrit Tyberghein
    Written by Andrew Zabolotny <bit@eltech.ru>

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

#ifndef __GLOS2_H__
#define __GLOS2_H__

#include "csutil/scf.h"
#include "cssys/os2/icsos2.h"
#include "cs2d/openglcommon/glcommon2d.h"

// avoid including os2.h
class glWindow;
typedef ULong HWND;
typedef ULong ULONG;
typedef ULong HPAL;

/**
 * This is the OS/2 OpenGL 2D driver. I did not had any chance to test
 * it on any videocard that supports hardware acceleration for OS/2 OpenGL
 * but I hope it will work.
 * <p>
 * The OS/2 OpenGL window is not resizeable as Crystal Space does not
 * support dynamically resizeable windows. The support for color index
 * modes (256 color) is not (yet?) functional (it's the fault of 3D driver).
 */
class csGraphics2DOS2GL : public csGraphics2DGLCommon
{
  /// Pixel format (a combination of GLCF_XXX)
  UInt PixelFormat;
  /// The width for which LineAddress has been computed last time
  UInt LineAddressFrameW;
  /// The OpenGL window object
  glWindow *glW;
  /// The handle of window where OpenGL context is located
  HWND WinHandle;
  /// The palette
  ULONG GLPalette[256];
  /// TRUE if palette has to be updated
  bool UpdatePalette;
  /// Use native mouse cursor, if possible?
  bool HardwareCursor;

  /// Window position in percents
  int WindowX, WindowY;
  /// Pointer to the OS/2 system driver
  iOS2SystemDriver* OS2System;

public:
  csGraphics2DOS2GL (iBase *iParent);
  virtual ~csGraphics2DOS2GL ();

  virtual bool Initialize (iSystem *pSystem);
  virtual bool Open (const char *Title);
  virtual void Close ();

  virtual void Print (csRect *area = NULL);
  virtual int GetPage ();
  virtual bool DoubleBuffer (bool Enable);
  virtual bool DoubleBuffer ();

  virtual void SetRGB (int i, int r, int g, int b);

  virtual bool BeginDraw ();
  virtual void FinishDraw ();

  virtual bool SetMousePosition (int x, int y);
  virtual bool SetMouseCursor (csMouseCursorID iShape);

private:
  static void KeyboardHandlerStub (void *Self, unsigned char ScanCode, int Down,
    unsigned char RepeatCount, int ShiftFlags);
  static void MouseHandlerStub (void *Self, int Button, int Down, int x, int y,
    int ShiftFlags);
  static void FocusHandlerStub (void *Self, bool Enable);
  static void TerminateHandlerStub (void *Self);
};

#endif // __GLOS2_H__
