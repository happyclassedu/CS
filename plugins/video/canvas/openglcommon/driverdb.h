/*
    Copyright (C) 2004 by Jorrit Tyberghein
	      (C) 2004 by Frank Richter

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

#ifndef __CS_CANVAS_OPENGLCOMMON_DRIVERDB_H__
#define __CS_CANVAS_OPENGLCOMMON_DRIVERDB_H__

class csGraphics2DGLCommon;

class csGLDriverDatabase
{
  void Report (int severity, const char* msg, ...);
public:
  csGraphics2DGLCommon* ogl2d;

  csStringHash tokens;
#define CS_TOKEN_ITEM_FILE "video/canvas/openglcommon/driverdb.tok"
#include "cstool/tokenlist.h"

  csGLDriverDatabase ();
  ~csGLDriverDatabase ();

  void Open (csGraphics2DGLCommon* ogl2d);
  void Close ();
};

#endif // __CS_CANVAS_OPENGLCOMMON_DRIVERDB_H__
