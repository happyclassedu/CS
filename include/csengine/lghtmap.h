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

#ifndef LIGHTMAP_H
#define LIGHTMAP_H

#include "csutil/scf.h"
#include "ilghtmap.h"

class csPolyTexture;
class csPolygonSet;
class csPolygon3D;
class csLight;
class csWorld;
class Dumper;

/**
 * This is a shadow-map for a pseudo-dynamic light.
 */
class csShadowMap
{
  ///
  friend class csLightMap;
  ///
  friend class csPolyTexture;

private:
  csShadowMap* next;
  unsigned char* map;

  /**
   * The pseudo-dynamic light.
   */
  csLight* light;

  ///
  csShadowMap ();
  ///
  virtual ~csShadowMap ();

  ///
  unsigned char* GetShadowMap () { return map; }

  ///
  void Alloc (csLight* light, int w, int h, int lms);
  ///
  void MipmapLightMap (int w, int h, int lms, csShadowMap* source, int w2, int h2, int lms2);
  ///
  void CopyLightMap (csShadowMap* source, int size);
};

/**
 * A small class encapsulating an RGB lightmap.
 */
class csRGBLightMap
{
private:
  int max_sizeRGB;	// Max size for every map.
  unsigned char* map;

public:
  ///
  void Clear ()
  {
    CHK (delete [] map); map = NULL;
    max_sizeRGB = 0;
  }

  ///
  csRGBLightMap () : max_sizeRGB (0), map (NULL) { }
  ///
  ~csRGBLightMap () { Clear (); }

  ///
  int GetMaxSize () { return max_sizeRGB; }
  ///
  void SetMaxSize (int s) { max_sizeRGB = s; }

  /// Get data.
  unsigned char* GetMap () { return map; }
  /// Get red map.
  unsigned char* GetRed () { return map; }
  /// Get green map.
  unsigned char* GetGreen () { return map+max_sizeRGB; }
  /// Get blue map.
  unsigned char* GetBlue () { return map+(max_sizeRGB<<1); }

  /// Set color maps.
  void SetMap (unsigned char* m) { map = m; }

  ///
  void Alloc (int size)
  {
    max_sizeRGB = size;
    CHK (delete [] map);
    CHK (map = new unsigned char [size*3]);
  }

  ///
  void Copy (csRGBLightMap& other, int size)
  {
    Clear ();
    if (other.map) { Alloc (size); memcpy (map, other.map, size*3); }
  }
};

/**
 * The static and all dynamic lightmaps for one or more mipmap-levels of a polygon.
 */
class csLightMap : public iLightMap
{
  ///
  friend class csPolyTexture;
  ///
  friend class Dumper;

private:
  /**
   * A color lightmap containing all static lighting information
   * for the static lights (no pseudo-dynamic lights are here).
   */
  csRGBLightMap static_lm;

  /**
   * The final lightmap that is going to be used for rendering.
   * In many cases this is just a copy of static_lm. But it may
   * contain extra lighting information obtained from all the
   * pseudo-dynamic shadowmaps and also the true dynamic lights.
   */
  csRGBLightMap real_lm;

  /**
   * Linked list of shadow-maps (for the pseudo-dynamic lights).
   * These shadowmaps are applied to static_lm to get real_lm.
   */
  csShadowMap* first_smap;

  /// Size of the lighted texture.
  long size;
  /// Size of the lightmap.
  long lm_size;

  /// LightMap dims (possibly po2 depending on used 3D driver).
  int lwidth, lheight;
  /// Original lightmap dims (non-po2 possibly).
  int rwidth, rheight;
  
  /**
   * Mean lighting value of this lightmap.
   * (only for static lighting currently).
   */
  unsigned char mean_r;
  unsigned char mean_g;
  unsigned char mean_b;

  /// The hicolor cache ptr.
  void *cachedata;

  /**
   * Convert three lightmap tables to the right mixing mode.
   * This function assumes that mixing mode is one of FAST_Wxx
   * and will not function correctly if called with NOCOLOR, TRUE_RGB
   * or FAST_RGB.<br>
   * This function correctly recognizes a dynamic lightmap which only
   * contains shadow information and does not do the conversion in that
   * case.
   */
  void ConvertToMixingMode (unsigned char* mr, unsigned char* mg,
			    unsigned char* mb, int sz);

  /**
   * Calculate the sizes of this lightmap.
   */
  void SetSize (int w, int h, int lms);

public:
  ///
  csLightMap ();
  ///
  virtual ~csLightMap ();

  ///
  csRGBLightMap& GetStaticMap () { return static_lm; }
  ///
  csRGBLightMap& GetRealMap () { return real_lm; }

  ///
  long GetSize () { return lm_size; }

  /**
   * Allocate the lightmap. 'w' and 'h' are the size of the
   * bounding box in lightmap space.
   * r,g,b is the ambient light color used to initialize the lightmap.
   */
  void Alloc (int w, int h, int lms, int r, int g, int b);

  /**
   * Allocate this lightmap by mipmapping the given source lightmap.
   */
  void MipmapLightMap (int w, int h, int lms, csLightMap* source, int w2, int h2, int lms2);

  /// Copy a lightmap.
  void CopyLightMap (csLightMap* source);

  /**
   * Create a ShadowMap for this LightMap.
   */
  csShadowMap* NewShadowMap (csLight* light, int w, int h, int lms);

  /**
   * Allocate the static RGBLightMap.
   */
  void AllocStaticLM (int w, int h, int lms);

  /**
   * Find a ShadowMap for a specific pseudo-dynamic light.
   */
  csShadowMap* FindShadowMap (csLight* light);

  /**
   * Delete a ShadowMap. NOTE!!! This function only works
   * if the ShadowMap was the LAST CREATED for this LightMap!!!
   * It is ment for dynamic lights that do not reach the polygon
   * but this can only be seen after trying.
   */
  void DelShadowMap (csShadowMap* plm);

  /**
   * Read from the cache. Return true if succesful.
   * Index is the index of the polygon in the containing object. It is used
   * for identifying the lightmap on disk.
   */
  bool ReadFromCache (int w, int h, int lms, csPolygonSet* owner,
    csPolygon3D* poly, int index, csWorld* world);

  /**
   * Cache the lightmaps in the precalculation area.
   */
  void Cache (csPolygonSet* owner, csPolygon3D* poly, int index,
    csWorld* world);

  /**
   * Scale the lightmap one step down. This is used in
   * 'High Quality Lightmap Mode'.
   */
  void Scale (int w, int h, int new_lms);

  /**
   * Convert the lightmaps to the correct mixing mode.
   * This function does nothing unless the mixing mode is
   * nocolor.<p>
   *
   * This function also calculates the mean color of the lightmap.
   */
  void ConvertToMixingMode ();

  /**
   * Convert the lightmaps to a 3D driver dependent size.
   */
  void ConvertFor3dDriver (bool requirePO2, int maxAspect = 32767);

  //------------------------ iLightMap implementation ------------------------
  DECLARE_IBASE;
  ///
  virtual unsigned char *GetMap (int nMap);
  ///
  virtual int GetWidth ()
  { return lwidth; }
  ///
  virtual int GetHeight ()
  { return lheight; }
  ///
  virtual int GetRealWidth ()
  { return rwidth; }
  ///
  virtual int GetRealHeight ()
  { return rheight; }
  ///
  virtual void *GetCacheData ()
  { return cachedata; }
  ///
  virtual void SetCacheData (void *d)
  { cachedata = d; }
  ///
  virtual void GetMeanLighting (int &r, int &g, int &b)
  { r = mean_r; g = mean_g; b = mean_b; }
};

#endif // LIGHTMAP_H
