/*
    Copyright (C) 1998 by Jorrit Tyberghein and Dan Ogles.

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

#ifndef __GL_TEXTURECACHE_H__
#define __GL_TEXTURECACHE_H__

#include "csutil/scf.h"
#include <GL/gl.h>

struct iLightMap;
struct iTextureHandle;
struct iPolygonTexture;

///
struct csGLCacheData
{
  /// the size of this texture/lightmap
  long Size;
  /// iTextureHandle if texture, iLightMap if lightmap.
  void *Source;
  /// GL texture handle
  GLuint Handle;
  /// linked list
  csGLCacheData *next, *prev;
};

///
enum csCacheType
{
  CS_TEXTURE, CS_LIGHTMAP
};

///
class OpenGLCache
{
private:
  ///
  csCacheType type;
  /// the head and tail of the cache data
  csGLCacheData *head, *tail;

protected:
  /// the maximum size of the cache
  long cache_size;
  /// number of items
  int num;
  /// the total size of the cache
  long total_size;

public:
  /// takes the maximum size of the cache
  OpenGLCache (int max_size, csCacheType type, int bpp);
  ///
  virtual ~OpenGLCache ();

  ///
  void cache_texture (iTextureHandle *texture);
  ///
  void cache_lightmap (iPolygonTexture *polytex);
  ///
  void Clear ();

protected:
  ///
  int bpp;

  ///
  virtual void Load (csGLCacheData *d) = 0;
  ///
  virtual void Unload (csGLCacheData *d) = 0;
};

///
class OpenGLTextureCache : public OpenGLCache
{
  bool rstate_bilinearmap;

public:
  ///
  OpenGLTextureCache (int size, int bitdepth);
  ///
  virtual ~OpenGLTextureCache ();

  ///
  void SetBilinearMapping (bool m) { rstate_bilinearmap = m; }
  ///
  bool GetBilinearMapping () { return rstate_bilinearmap; }

protected:
  ///
  virtual void Load (csGLCacheData *d);
  ///
  virtual void Unload (csGLCacheData *d);
};

///
class OpenGLLightmapCache : public OpenGLCache
{
public:
  ///
  OpenGLLightmapCache (int size, int bitdepth);
  ///
  virtual ~OpenGLLightmapCache ();

protected:
  ///
  virtual void Load (csGLCacheData *d);
  ///
  virtual void Unload (csGLCacheData *d);
};

#endif // __GL_TEXTURECACHE_H__
