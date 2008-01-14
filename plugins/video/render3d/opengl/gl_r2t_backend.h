/*
    Copyright (C) 2005 by Jorrit Tyberghein
              (C) 2005 by Frank Richter

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

#ifndef __CS_GL_R2T_BACKEND_H__
#define __CS_GL_R2T_BACKEND_H__

#include "csgeom/csrect.h"
#include "csutil/ref.h"
#include "ivideo/graph3d.h"
#include "ivideo/texture.h"

struct iTextureHandle;

CS_PLUGIN_NAMESPACE_BEGIN(gl3d)
{

class csGLGraphics3D;

/// Superclass for all render2texture backends
class csGLRender2TextureBackend
{
protected:
  csGLGraphics3D* G3D;
  
  struct RTAttachment
  {
    csRef<iTextureHandle> texture;
    bool persistent;
    int subtexture;
    
    void Clear()
    {
      texture.Invalidate ();
    }
    bool IsValid()
    {
      return texture.IsValid();
    }
    void Set (iTextureHandle* handle, bool persistent,
      int subtexture)
    {
      this->texture = handle;
      this->persistent = persistent;
      this->subtexture = subtexture;
    }
  };
public:
  csGLRender2TextureBackend (csGLGraphics3D* G3D);
  virtual ~csGLRender2TextureBackend();

  virtual bool SetRenderTarget (iTextureHandle* handle, bool persistent,
  	int subtexture, csRenderTargetAttachment attachment) = 0;
  virtual void UnsetRenderTargets() = 0;
  virtual bool CanSetRenderTarget (const char* format,
    csRenderTargetAttachment attachment) = 0;
  virtual iTextureHandle* GetRenderTarget (csRenderTargetAttachment attachment,
    int* subtexture) const = 0;
  
  virtual void BeginDraw (int drawflags) = 0;
  virtual void SetupProjection () = 0;
  virtual void FinishDraw () = 0;
  virtual void SetClipRect (const csRect& clipRect) = 0;
  virtual void SetupClipPortalDrawing () = 0;
  virtual bool HasStencil() = 0;
};

}
CS_PLUGIN_NAMESPACE_END(gl3d)

#endif // __CS_GL_R2T_BACKEND_H__
