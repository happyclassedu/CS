/*
    Copyright (C) 2003 by Jorrit Tyberghein
	      (C) 2003 by Frank Richter
	      (C) 1999 by Gary Haussmann
			  Samuel Humphreys

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

#include "cssysdef.h"

#if defined(CS_OPENGL_PATH)
#include CS_HEADER_GLOBAL(CS_OPENGL_PATH,gl.h)
#else
#include <GL/gl.h>
#endif

#include "csutil/csuctransform.h"
#include "csgfx/memimage.h"
#include "iutil/databuff.h"
#include "ivideo/fontserv.h"

#include "glcommon2d.h"
#include "glfontcache.h"

//---------------------------------------------------------------------------

csGLFontCache::csGLFontCache (csGraphics2DGLCommon* G2D) : 
  cacheDataAlloc (512), verts2d (256, 256), texcoords (256, 256)
{
  csGLFontCache::G2D = G2D;
  statecache = G2D->statecache;

  int maxtex = 257;
  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxtex);

  texSize = G2D->config->GetInt ("Video.OpenGL.FontCache.TextureSize", 256);
  texSize = MAX (texSize, 64);
  texSize = MIN (texSize, maxtex);
  maxTxts = G2D->config->GetInt ("Video.OpenGL.FontCache.MaxTextureNum", 16);
  maxTxts = MAX (maxTxts, 1);
  maxTxts = MIN (maxTxts, 32);
  maxFloats = G2D->config->GetInt ("Video.OpenGL.FontCache.VertexCache", 128);
  maxFloats = ((maxFloats + 3) / 4) * 4;
  maxFloats = MAX (maxFloats, 4);
  usedTexs = 0;

  compressPages = false;
  /*
    Optionally enable compression of the font texture pages.
   */
  if (G2D->config->GetBool ("Video.OpenGL.FontCache.CompressTextures", false))
  {
    G2D->ext.InitGL_ARB_texture_compression ();
    G2D->ext.InitGL_EXT_texture_compression_s3tc ();
    compressPages = G2D->ext.CS_GL_ARB_texture_compression &&
      G2D->ext.CS_GL_EXT_texture_compression_s3tc;
  }
  glyphAlign = compressPages ? 4 : 1;

  glGenTextures (1, &texWhite);
  statecache->SetTexture (GL_TEXTURE_2D, texWhite);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  csRGBpixel texPix (255, 255, 255, 0);

  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, 
    GL_RGBA, GL_UNSIGNED_BYTE, &texPix);

  numFloats = 0;
  jobCount = 0;

  textWriting = false;
  textWriting = false;
}

csGLFontCache::~csGLFontCache ()
{
  CleanupCache ();

  statecache->SetTexture (GL_TEXTURE_2D, 0);
  int tex;
  for (tex = 0; tex < textures.Length (); tex++)
  {
    glDeleteTextures (1, &textures[tex].handle);
  }
  glDeleteTextures (1, &texWhite);
  textures.DeleteAll ();
}

csGLFontCache::GlyphCacheData* csGLFontCache::InternalCacheGlyph (
  KnownFont* font, utf32_char glyph, uint flags)
{
  bool hasGlyph = font->font->HasGlyph (glyph);
  if (!hasGlyph)
  {
    GLGlyphCacheData* cacheData = cacheDataAlloc.Alloc ();
    memset (cacheData, 0, sizeof (GLGlyphCacheData));
    cacheData->font = font;
    cacheData->glyph = glyph;
    cacheData->hasGlyph = false;
    return cacheData;
  }
  csRect texRect;
  csSubRect2* sr = 0;

  csBitmapMetrics bmetrics;
  csRef<iDataBuffer> alphaData;
  if ((flags & CS_WRITE_NOANTIALIAS) == 0)
  {
    alphaData = font->font->GetGlyphAlphaBitmap (glyph, bmetrics);
  }
  csRef<iDataBuffer> bitmapData;
  if (!alphaData)
    bitmapData = font->font->GetGlyphBitmap (glyph, bmetrics);

  const int allocWidth = 
    ((bmetrics.width + glyphAlign - 1) / glyphAlign) * glyphAlign;
  const int allocHeight = 
    ((bmetrics.height + glyphAlign - 1) / glyphAlign) * glyphAlign;
  int tex = 0;
  while (tex < textures.Length ())
  {
    sr = textures[tex].glyphRects->Alloc (allocWidth, allocHeight, 
      texRect);
    if (sr != 0)
    {
      break;
    }
    tex++;
  }
  if ((sr == 0) && (textures.Length () < maxTxts))
  {
    tex = textures.Length ();
    textures.SetLength (textures.Length () + 1);

    textures[tex].InitRects (texSize);

    glGenTextures (1, &textures[tex].handle);
    statecache->SetTexture (GL_TEXTURE_2D, textures[tex].handle);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    csRGBpixel* texImage = new csRGBpixel[texSize * texSize * 4];
#ifdef CS_DEBUG
    csRGBpixel* p = texImage;
    for (int y = 0; y < texSize; y++)
    {
      for (int x = 0; x < texSize; x++)
      {
	const uint8 val = 0x7f + (((x ^ y) & 1) << 7);
	(p++)->Set (val, val, val, 255 - val);
      }
    }
#endif
    // Alloc a pixel on that texture for background drawing.
    texImage->Set (255, 255, 255, 0);
    textures[tex].glyphRects->Alloc (1, 1, texRect);

    if (compressPages)
    {
      glTexImage2D (GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
	texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage);
    }
    else
    {
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, texSize, texSize, 0, 
	GL_RGBA, GL_UNSIGNED_BYTE, texImage);
    }
    delete[] texImage;

    statecache->SetTexture (GL_TEXTURE_2D, 0);

    sr = textures[tex].glyphRects->Alloc (allocWidth, allocHeight, 
      texRect);
  }
  if (sr != 0)
  {
    GLGlyphCacheData* cacheData = cacheDataAlloc.Alloc ();
    cacheData->subrect = sr;
    cacheData->texNum = tex;
    cacheData->font = font;
    cacheData->glyph = glyph;
    cacheData->flags = flags & RELEVANT_WRITE_FLAGS;
    cacheData->bmetrics = bmetrics;
    font->font->GetGlyphMetrics (glyph, cacheData->glyphMetrics);
    cacheData->tx1 = (float)texRect.xmin / (float)texSize;
    cacheData->ty1 = (float)texRect.ymin / (float)texSize;
    cacheData->tx2 = (float)(texRect.xmin + bmetrics.width) / (float)texSize;
    cacheData->ty2 = (float)(texRect.ymin + bmetrics.height) / (float)texSize;
    cacheData->hasGlyph = true;

    CopyGlyphData (font->font, glyph, tex, bmetrics, texRect, bitmapData, 
      alphaData);

    return cacheData;
  }
  return 0;
}

void csGLFontCache::InternalUncacheGlyph (GlyphCacheData* cacheData)
{
  GLGlyphCacheData* glCacheData = (GLGlyphCacheData*)cacheData;
  const int texNum = glCacheData->texNum;
  if (usedTexs & (1 << texNum))
  {
    FlushArrays ();
    usedTexs &= ~(1 << texNum);
  }
  textures[texNum].glyphRects->Reclaim (glCacheData->subrect);
  cacheDataAlloc.Free (glCacheData);
}

void csGLFontCache::CopyGlyphData (iFont* font, utf32_char glyph, int tex, 
				   const csBitmapMetrics& bmetrics, 
				   const csRect& texRect, iDataBuffer* bitmapDataBuf, 
				   iDataBuffer* alphaDataBuf)
{
  statecache->SetTexture (GL_TEXTURE_2D, textures[tex].handle);

  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

  csRGBpixel* rgbaData = new csRGBpixel[texRect.Width () * texRect.Height ()];

  const int padX = texRect.Width() - bmetrics.width;
  if (alphaDataBuf)
  {
    uint8* alphaData = alphaDataBuf->GetUint8 ();
    csRGBpixel* dest = rgbaData;
    uint8* src = alphaData;
    int x, y;
    for (y = 0; y < bmetrics.height; y++)
    {
      for (x = 0; x < bmetrics.width; x++)
      {
	const uint8 val = *src++;
	(dest++)->Set (255 - val, 255 - val, 255 - val, val);
      }
      dest += padX;
    }
  }
  else
  {
    if (bitmapDataBuf)
    {
      uint8* bitData = bitmapDataBuf->GetUint8 ();

      //uint8* alphaData = new uint8[rect.Width () * rect.Height ()];
      csRGBpixel* dest = rgbaData;
      uint8* src = bitData;
      uint8 byte = *src++;
      int x, y;
      for (y = 0; y < bmetrics.height; y++)
      {
	for (x = 0; x < bmetrics.width; x++)
	{
	  const uint8 val = (byte & 0x80) ? 0xff : 0;
	  (dest++)->Set (255 - val, 255 - val, 255 - val, val);
	  if ((x & 7) == 7)
	  {
	    byte = *src++;
	  }
	  else
	  {
	    byte <<= 1;
	  }
	}
	if ((bmetrics.width & 7) != 0) byte = *src++;
	dest += padX;
      }
    }
  }

  glTexSubImage2D (GL_TEXTURE_2D, 0, texRect.xmin, texRect.ymin, 
    texRect.Width (), texRect.Height (), 
    GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
  delete[] rgbaData;
}

void csGLFontCache::FlushArrays ()
{
  if (jobCount == 0) return;

  if (needStates)
  {
    statecache->Enable_GL_TEXTURE_2D ();
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
    statecache->SetBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    statecache->SetAlphaFunc (GL_GREATER, 0.0f);

    envColor = ~0;
    needStates = false;
  }
  glTexCoordPointer(2, GL_FLOAT, sizeof (float) * 2, texcoords.GetArray ());
  glVertexPointer(2, GL_FLOAT, sizeof (float) * 2, verts2d.GetArray ());
  for (int j = 0; j < jobCount; j++)
  {
    const TextJob& job = jobs[j];
    const bool doFG = (job.vertCount != 0);
    const bool doBG = (job.bgVertCount != 0);
    if (doFG || doBG)
    {
      statecache->SetTexture (GL_TEXTURE_2D, job.texture);
      if (job.bg >= 0)
      {
	if (envColor != job.bg)
	{
	  float bgRGB[4];
	  G2D->DecomposeColor (job.bg, bgRGB[0], bgRGB[1], bgRGB[2]);
	  glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, bgRGB);
	  envColor = job.bg;
	}
	statecache->Disable_GL_ALPHA_TEST ();
	statecache->Disable_GL_BLEND ();

	if (doBG)
	{
	  glDrawArrays(GL_QUADS, job.bgVertOffset, job.bgVertCount);
	}
      }
      else
      {
	if (doFG)
	{
	  statecache->Enable_GL_ALPHA_TEST ();
	  statecache->Enable_GL_BLEND ();
	  if (envColor != 0)
	  {
	    static float fgRGB[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, fgRGB);
	    envColor = 0;
	  }
	}
      }

      if (doFG)
      {
	G2D->setGLColorfromint (job.fg);
	glDrawArrays(GL_QUADS, job.vertOffset, job.vertCount);
      }
    }
  }
  jobCount = 0;
  numFloats = 0;
}

csGLFontCache::TextJob& csGLFontCache::GetJob (int fg, int bg, 
					       GLuint texture, int bgOffset)
{
  TextJob& newJob = jobs.GetExtend (jobCount);
  jobCount++;
  newJob.ClearRanges ();
  newJob.vertOffset = numFloats / 2;
  newJob.bgVertOffset = (numFloats + bgOffset) / 2;
  newJob.fg = fg; newJob.bg = bg;
  newJob.texture = texture;
  return newJob;
}

void csGLFontCache::WriteString (iFont *font, int pen_x, int pen_y, 
				 int fg, int bg, const utf8_char* text,
				 uint flags)
{
  if (!text || !*text) return;

  BeginText();

  if (!(flags & CS_WRITE_BASELINE)) pen_y += font->GetAscent ();

  int maxwidth, maxheight;
  font->GetMaxSize (maxwidth, maxheight);

  KnownFont* knownFont = GetCachedFont (font);
  if (knownFont == 0) knownFont = CacheFont (font);

  if (pen_y <= ClipY1) return;
  pen_y = G2D->Height - pen_y/* - maxheight*/;

  int textLen = strlen ((char*)text);

  if (bg >= 0)
  {
    texcoords.SetLength (numFloats + textLen * 16);
    verts2d.SetLength (numFloats + textLen * 16);
  }
  else
  {
    texcoords.SetLength (numFloats + textLen * 8);
    verts2d.SetLength (numFloats + textLen * 8);
  }
  int bgVertOffset = (textLen + 1) * 8;
  float* tcPtr;
  float* vertPtr;
  float* bgTcPtr;
  float* bgVertPtr;

  float x1 = pen_x;
  float x2 = x1, y1 = 0, y2 = 0;
  int advance = 0;
  bool firstchar = true;
  float oldH = 0.0f;

  TextJob* job = 0;

  while (textLen > 0)
  {
    utf32_char glyph;
    int skip = csUnicodeTransform::UTF8Decode (text, textLen, glyph, 0);
    if (skip == 0) break;

    text += skip;
    textLen -= skip;

    const GLGlyphCacheData* cacheData = 
      (GLGlyphCacheData*)GetCacheData (knownFont, glyph, flags);
    if (cacheData == 0)
    {
      cacheData = (GLGlyphCacheData*)CacheGlyphUnsafe (knownFont, glyph, 
	flags);
    }
    if (!cacheData->hasGlyph)
    {
      // fall back to the default glyph (CS_FONT_DEFAULT_GLYPH)
      cacheData = (GLGlyphCacheData*)CacheGlyph (knownFont, 
	CS_FONT_DEFAULT_GLYPH, flags);
      if (!cacheData->hasGlyph) continue;
    }

    const int newTexNum = cacheData->texNum;
    const GLuint newHandle = textures[newTexNum].handle;
    if (!job || (job->texture != newHandle) || !(usedTexs & (1 << newTexNum)))
    {
      job = &GetJob (fg, bg, newHandle, bgVertOffset);
      // Fetch pointers again, the vertex cache might've been flushed
      tcPtr = texcoords.GetArray() + numFloats;
      vertPtr = verts2d.GetArray() + numFloats;
      bgTcPtr = texcoords.GetArray() + numFloats + bgVertOffset;
      bgVertPtr = verts2d.GetArray() + numFloats + bgVertOffset;
    }
    usedTexs |= (1 << newTexNum);

    advance += cacheData->bmetrics.left;
    
    // Hack: in case the first char has a negative left bitmap offset,
    // some of the background isn't drawn. Fix that.
    if (firstchar)
    {
      if (advance < 0)
      {
	advance = 0;
      }
      firstchar = false;
    }

    y1 = cacheData->bmetrics.top + pen_y;
    y2 = y1 - cacheData->bmetrics.height;

    bool needBgJob = false;
    if (bg >= 0)
    {
      float bx1 = x2;
      float bx2 = x2; //x2;
      float by1 = y1;
      float by2 = y2;

      if (advance > 0)
      {
	bx2 += (float)advance;
	// The texcoords are irrelevant for the BG
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx1;
	*bgVertPtr++ = by1;
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx2;
	*bgVertPtr++ = by1;
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx2;
	*bgVertPtr++ = by2;
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx1;
	*bgVertPtr++ = by2;
	job->bgVertCount += 4;
	bgVertOffset += 8;
      }
      else if (advance < 0)
      {
	/*
	  Negative advance slightly complicates things. This character 
	  overlaps with the last one. So we add the overlapping BG to the
	  current job, but create a new job for the next char with a 
	  transparent bg.
	 */
	bx1 = x1;
	bx2 = bx1 + (float)(cacheData->bmetrics.left + 
	  cacheData->bmetrics.width);
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx1;
	*bgVertPtr++ = by1;
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx2;
	*bgVertPtr++ = by1;
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx2;
	*bgVertPtr++ = by2;
	*bgTcPtr++ = 0.0f;
	*bgTcPtr++ = 0.0f;
	*bgVertPtr++ = bx1;
	*bgVertPtr++ = by2;
	job->bgVertCount += 4;
	bgVertOffset += 8;

	// The glyph needs to be drawn transparently, so fetch another job
	job = &GetJob (fg, -1, job->texture, bgVertOffset);
	// Later, fetch a BG job again
	needBgJob = true;
      }
      advance = 0;

    }

    float x_left = x1;
    x1 = x1 + cacheData->bmetrics.left;
    x2 = x1 + cacheData->bmetrics.width;
    float tx1, tx2, ty1, ty2;

    tx1 = cacheData->tx1;
    tx2 = cacheData->tx2;
    ty1 = cacheData->ty1;
    ty2 = cacheData->ty2;

    *tcPtr++ = tx1;
    *tcPtr++ = ty1;
    *vertPtr++ = x1;
    *vertPtr++ = y1;
    *tcPtr++ = tx2;
    *tcPtr++ = ty1;
    *vertPtr++ = x2;
    *vertPtr++ = y1;
    *tcPtr++ = tx2;
    *tcPtr++ = ty2;
    *vertPtr++ = x2;
    *vertPtr++ = y2;
    *tcPtr++ = tx1;
    *tcPtr++ = ty2;
    *vertPtr++ = x1;
    *vertPtr++ = y2;
    numFloats += 8;
    bgVertOffset -= 8;
    job->vertCount += 4;

    advance += cacheData->glyphMetrics.advance - 
      (cacheData->bmetrics.width + cacheData->bmetrics.left);

    if (needBgJob)
    {
      // Just in case fetched a transparent job for negative advance
      job = &GetJob (fg, bg, job->texture, bgVertOffset);
    }

    x1 = x_left + cacheData->glyphMetrics.advance;
    oldH = y2 - y1;
  }

  // "Trailing" background
  if ((bg >= 0) & (advance > 0))
  {
    float bx1 = x2;
    float bx2 = bx1 + (float)advance;
    float by1 = y1;
    float by2 = y2;

    *bgTcPtr++ = 0.0f;
    *bgTcPtr++ = 0.0f;
    *bgVertPtr++ = bx1;
    *bgVertPtr++ = by1;
    *bgTcPtr++ = 0.0f;
    *bgTcPtr++ = 0.0f;
    *bgVertPtr++ = bx2;
    *bgVertPtr++ = by1;
    *bgTcPtr++ = 0.0f;
    *bgTcPtr++ = 0.0f;
    *bgVertPtr++ = bx2;
    *bgVertPtr++ = by2;
    *bgTcPtr++ = 0.0f;
    *bgTcPtr++ = 0.0f;
    *bgVertPtr++ = bx1;
    *bgVertPtr++ = by2;
    job->bgVertCount += 4;
    bgVertOffset += 8;
  }

  if (bg >= 0)
  {
    // Make sure the next data added to the cached comes in after the
    // BG stuff
    numFloats = (job->bgVertOffset + job->bgVertCount) * 2;
  }
  if (numFloats > maxFloats) FlushArrays();
}

void csGLFontCache::BeginText ()
{
  if (textWriting) return;

  vaEnabled = glIsEnabled (GL_VERTEX_ARRAY) == GL_TRUE;
  tcaEnabled = glIsEnabled (GL_TEXTURE_COORD_ARRAY) == GL_TRUE;
  caEnabled = glIsEnabled (GL_COLOR_ARRAY) == GL_TRUE;

  if (!vaEnabled) glEnableClientState (GL_VERTEX_ARRAY);
  if (!tcaEnabled) glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  if (caEnabled) glDisableClientState (GL_COLOR_ARRAY);

  textWriting = true;
  needStates = true;
}

void csGLFontCache::FlushText ()
{
  if (!textWriting) return;

  FlushArrays ();

  if (!vaEnabled) glDisableClientState (GL_VERTEX_ARRAY);
  if (!tcaEnabled) glDisableClientState (GL_TEXTURE_COORD_ARRAY);
  if (caEnabled) glEnableClientState (GL_COLOR_ARRAY);

  statecache->Disable_GL_BLEND ();
  statecache->Disable_GL_ALPHA_TEST ();
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  PurgeEmptyPlanes ();

  textWriting = false;
}

void csGLFontCache::DumpFontCache (csRefArray<iImage>& pages)
{
  for (int t = 0; t < textures.Length(); t++)
  {
    csRef<iImage> page;
    page.AttachNew (new csImageMemory (texSize, texSize, 
      CS_IMGFMT_TRUECOLOR | CS_IMGFMT_ALPHA));

    statecache->SetTexture (GL_TEXTURE_2D, textures[t].handle);
    glGetTexImage (GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
      page->GetImageData ());
    pages.Push (page);
  }
}
