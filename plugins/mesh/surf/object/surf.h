/*
    Copyright (C) 2001 by Jorrit Tyberghein

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

#ifndef _SURF_H_
#define _SURF_H_

#include "csgeom/vector3.h"
#include "csgeom/transfrm.h"
#include "csutil/cscolor.h"
#include "imesh/object.h"
#include "imesh/surf.h"
#include "ivideo/graph3d.h"

#define ALL_FEATURES (CS_OBJECT_FEATURE_LIGHTING)

struct iMaterialWrapper;
class csSurfMeshObjectFactory;

/**
 * Surf version of mesh object.
 */
class csSurfMeshObject : public iMeshObject
{
private:
  iMeshObjectFactory* factory;
  int xres, yres;
  csVector3 topleft;
  float xscale, yscale;
  iMaterialWrapper* material;
  UInt MixMode;
  iMeshObjectDrawCallback* vis_cb;
  bool do_lighting;
  csColor color;
  float current_lod;
  uint32 current_features;

  G3DTriangleMesh mesh;
  bool initialized;
  csBox3 object_bbox;
  csVector3 surface_normal;
  csVector3 max_radius;
  long shapenr;

  /**
   * Camera space bounding box is cached here.
   * GetCameraBoundingBox() will check the current camera number from the
   * camera to see if it needs to recalculate this.
   */
  csBox3 camera_bbox;

  /// Current camera number.
  long cur_cameranr;
  /// Current movable number.
  long cur_movablenr;

  /**
   * Setup this object. This function will check if setup is needed.
   */
  void SetupObject ();

  /// Generate a mesh with a surface.
  void GenerateSurface (G3DTriangleMesh& mesh);
  /// Recalculate the object space bounding box.
  void RecalcObjectBBox ();
  /// Recalculate the surface normal.
  void RecalcSurfaceNormal ();

public:
  /// Constructor.
  csSurfMeshObject (iMeshObjectFactory* factory);

  /// Destructor.
  virtual ~csSurfMeshObject ();

  /// Get the bounding box in transformed space.
  void GetTransformedBoundingBox (long cameranr, long movablenr,
      const csReversibleTransform& trans, csBox3& cbox);
  /**
   * Get the coordinates of the surface in screen coordinates.
   * Fills in the boundingBox with the X and Y locations of the surface.
   * Returns the max Z location of the surface, or -1 if not
   * on-screen. If the surface is not on-screen, the X and Y values are not
   * valid.
   */
  float GetScreenBoundingBox (long cameranr, long movablenr, float fov,
  	float sx, float sy,
	const csReversibleTransform& trans, csBox2& sbox, csBox3& cbox);

  void SetMaterialWrapper (iMaterialWrapper* material)
  {
    csSurfMeshObject::material = material;
  }
  iMaterialWrapper* GetMaterialWrapper () const { return material; }
  void SetMixMode (UInt mode) { MixMode = mode; }
  UInt GetMixMode () const { return MixMode; }
  void SetTopLeftCorner (const csVector3& tl)
  {
    initialized = false;
    topleft = tl;
    shapenr++;
  }
  csVector3 GetTopLeftCorner () const { return topleft; }
  void SetResolution (int x, int y)
  {
    initialized = false;
    xres = x; yres = y;
    shapenr++;
  }
  int GetXResolution () const { return xres; }
  int GetYResolution () const { return yres; }
  void SetScale (float x, float y)
  {
    initialized = false;
    xscale = x; yscale = y;
    shapenr++;
  }
  float GetXScale () const { return xscale; }
  float GetYScale () const { return yscale; }
  /// Set lighting.
  void SetLighting (bool l) { do_lighting = l; }
  /// Is lighting enabled.
  bool IsLighting () const { return do_lighting; }
  /// Set the color to use. Will be added to the lighting values.
  void SetColor (const csColor& col) { color = col; }
  /// Get the color.
  csColor GetColor () const { return color; }

  ///------------------------ iMeshObject implementation ------------------------
  SCF_DECLARE_IBASE;

  virtual iMeshObjectFactory* GetFactory () const { return factory; }
  virtual bool DrawTest (iRenderView* rview, iMovable* movable);
  virtual void UpdateLighting (iLight** lights, int num_lights,
      	iMovable* movable);
  virtual bool Draw (iRenderView* rview, iMovable* movable, csZBufMode mode);
  virtual void SetVisibleCallback (iMeshObjectDrawCallback* cb)
  {
    if (cb) cb->IncRef ();
    if (vis_cb) vis_cb->DecRef ();
    vis_cb = cb;
  }
  virtual iMeshObjectDrawCallback* GetVisibleCallback () const
  {
    return vis_cb;
  }
  virtual void GetObjectBoundingBox (csBox3& bbox, int type = CS_BBOX_NORMAL);
  virtual csVector3 GetRadius () { return max_radius; }
  virtual void NextFrame (cs_time /*current_time*/) { }
  virtual bool WantToDie () const { return false; }
  virtual void HardTransform (const csReversibleTransform& t);
  virtual bool SupportsHardTransform () const { return true; }
  virtual bool HitBeamObject (const csVector3&, const csVector3&,
  	csVector3&, float*) { return false; }
  virtual long GetShapeNumber () const { return shapenr; }
  virtual uint32 GetLODFeatures () const { return current_features; }
  virtual void SetLODFeatures (uint32 mask, uint32 value)
  {
    mask &= ALL_FEATURES;
    current_features = (current_features & ~mask) | (value & mask);
  }
  virtual void SetLOD (float lod) { current_lod = lod; }
  virtual float GetLOD () const { return current_lod; }
  virtual int GetLODPolygonCount (float /*lod*/) const
  {
    return 0;	// @@@ Implement me please!
  }

  //------------------------- iSurfaceState implementation ----------------
  class SurfaceState : public iSurfaceState
  {
    SCF_DECLARE_EMBEDDED_IBASE (csSurfMeshObject);
    virtual void SetResolution (int x, int y)
    {
      scfParent->SetResolution (x, y);
    }
    virtual int GetXResolution () const
    {
      return scfParent->GetXResolution ();
    }
    virtual int GetYResolution () const
    {
      return scfParent->GetYResolution ();
    }
    virtual void SetTopLeftCorner (const csVector3& tl)
    {
      scfParent->SetTopLeftCorner (tl);
    }
    virtual csVector3 GetTopLeftCorner () const
    {
      return scfParent->GetTopLeftCorner ();
    }
    virtual void SetScale (float x, float y)
    {
      scfParent->SetScale (x, y);
    }
    virtual float GetXScale () const
    {
      return scfParent->GetXScale ();
    }
    virtual float GetYScale () const
    {
      return scfParent->GetYScale ();
    }
    virtual void SetMaterialWrapper (iMaterialWrapper* material)
    {
      scfParent->SetMaterialWrapper (material);
    }
    virtual iMaterialWrapper* GetMaterialWrapper () const
    {
      return scfParent->GetMaterialWrapper ();
    }
    virtual void SetMixMode (UInt mode)
    {
      scfParent->SetMixMode (mode);
    }
    virtual UInt GetMixMode () const
    {
      return scfParent->GetMixMode ();
    }
    virtual void SetLighting (bool l)
    {
      scfParent->SetLighting (l);
    }
    virtual bool IsLighting () const
    {
      return scfParent->IsLighting ();
    }
    virtual void SetColor (const csColor& col)
    {
      scfParent->SetColor (col);
    }
    virtual csColor GetColor () const
    {
      return scfParent->GetColor ();
    }
  } scfiSurfaceState;
  friend class SurfaceState;
};

/**
 * Factory for balls.
 */
class csSurfMeshObjectFactory : public iMeshObjectFactory
{
public:
  /// Constructor.
  csSurfMeshObjectFactory (iBase *pParent);

  /// Destructor.
  virtual ~csSurfMeshObjectFactory ();

  //------------------------ iMeshObjectFactory implementation --------------
  SCF_DECLARE_IBASE;

  virtual iMeshObject* NewInstance ();
  virtual void HardTransform (const csReversibleTransform&) { }
  virtual bool SupportsHardTransform () const { return false; }
};

/**
 * Surf type. This is the plugin you have to use to create instances
 * of csSurfMeshObjectFactory.
 */
class csSurfMeshObjectType : public iMeshObjectType
{
public:
  /// Constructor.
  csSurfMeshObjectType (iBase*);

  /// Destructor.
  virtual ~csSurfMeshObjectType ();

  /// Register plugin with the system driver
  virtual bool Initialize (iSystem *pSystem);

  //------------------------ iMeshObjectType implementation --------------
  SCF_DECLARE_IBASE;

  /// New factory.
  virtual iMeshObjectFactory* NewFactory ();
  virtual uint32 GetFeatures () const
  {
    return ALL_FEATURES;
  }
};

#endif // _SURF_H_

