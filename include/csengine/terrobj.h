/*
    Copyright (C) 2001 by Jorrit Tyberghein
    Portions written by Richard D Shank

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

#ifndef __CS_TERROBJ_H__
#define __CS_TERROBJ_H__

#include "csutil/csobject.h"
#include "csutil/csvector.h"
#include "iterrain/object.h"
#include "iengine/terrain.h"

struct iEngine;
struct iTerrainWrapper;
class csRenderView;
class csSector;
class csTerrainFactoryWrapper;
class Dumper;

/**
 * The holder class for all implementations of iTerrainObject.
 */
class csTerrainWrapper : public csObject
{
  friend class Dumper;

private:
  /// Bounding box for polygon trees.
//  csPolyTreeBBox bbox;  // don't think this is needed

  /// a pointer to the engine
  iEngine *pEngine;

  /// List of sectors.
  csVector sectors;

  /// Mesh object corresponding with this csTerrainWrapper.
  iTerrainObject *pTerrObj;

  /// Optional reference to the parent csTerrainFactoryWrapper.
  csTerrainFactoryWrapper *pFactory;

  /**
   * Destructor.  This is private in order to force clients to use DecRef()
   * for object destruction.
   */
  virtual ~csTerrainWrapper ();

public:
  /// Constructor.
  csTerrainWrapper (iEngine *pEng, iTerrainObject *pTerr);
  /// Constructor.
  csTerrainWrapper (iEngine *pEng );

  /// Set the terrain factory.
  void SetFactory (csTerrainFactoryWrapper* factory)
  {
    csTerrainWrapper::pFactory = factory;
  }

  /// Get the terrain factory.
  csTerrainFactoryWrapper* GetFactory ()
  {
    return pFactory;
  }

  /// Add a sector
  void AddSector( csSector *pSector );
  /// Clear all sectors
  void ClearSectors( void );

  /// Set the mesh object.
  void SetTerrainObject( iTerrainObject *pObj );
  /// Get the mesh object.
  iTerrainObject* GetTerrainObject()const
  {
    return pTerrObj;
  }

  /**
   * Light object according to the given array of lights (i.e.
   * fill the vertex color array).
   */
  virtual void UpdateLighting( iLight** lights, int iNumLights );

  /**
   * Draw this terrain object given a camera transformation.
   */
  virtual void Draw( iRenderView *iRView, bool use_z_buf = true );

  
  virtual int CollisionDetect (csTransform *p);

  SCF_DECLARE_IBASE_EXT (csObject);

  //------------------- iTerrainWrapper implementation ------------------//
  struct TerrainWrapper : public iTerrainWrapper
  {
    SCF_DECLARE_EMBEDDED_IBASE (csTerrainWrapper);
    virtual csTerrainWrapper* GetPrivateObject ()
    {
      return (csTerrainWrapper*)scfParent;
    }
    virtual iTerrainObject* GetTerrainObject ()
    {
      return scfParent->GetTerrainObject ();
    }
    virtual void SetTerrainObject (iTerrainObject *t)
    {
      scfParent->SetTerrainObject (t);
    }
    virtual iObject *QueryObject()
    {
      return scfParent;
    }
    virtual void UpdateLighting (iLight** lights, int num_lights)
    {
      (void)lights; (void)num_lights;
      printf ("UpdateLighting in iTerrainWrapper DOES NOT WORK YET!\n");
      // @@@ TODO!!!
      //scfParent->UpdateLighting (lights, num_lights);
    }
  } scfiTerrainWrapper;
};

/**
 * The holder class for all implementations of iTerrainObjectFactory.
 */
class csTerrainFactoryWrapper : public csObject
{
  friend class Dumper;

private:
  /// Mesh object factory corresponding with this csTerrainFactoryWrapper.
  iTerrainObjectFactory* pTerrFact;

private:
  /// Destructor.
  virtual ~csTerrainFactoryWrapper ();

public:
  /// Constructor.
  csTerrainFactoryWrapper (iTerrainObjectFactory *pFactory);
  /// Constructor.
  csTerrainFactoryWrapper ();

  /// Set the terrain object factory.
  void SetTerrainObjectFactory (iTerrainObjectFactory *pFactory);

  /// Get the mesh object factory.
  iTerrainObjectFactory* GetTerrainObjectFactory ()
  {
    return pTerrFact;
  }

  /**
   * Create a new mesh object for this template.
   */
  csTerrainWrapper* NewTerrainObject( iEngine *pEng );

  SCF_DECLARE_IBASE_EXT (csObject);

  //------------------ iTerrainFactoryWrapper implementation -----------------//
  struct TerrainFactoryWrapper : public iTerrainFactoryWrapper
  {
    SCF_DECLARE_EMBEDDED_IBASE (csTerrainFactoryWrapper);
    virtual iTerrainObjectFactory* GetTerrainObjectFactory ()
    {
      return scfParent->GetTerrainObjectFactory ();
    }
    virtual void SetTerrainObjectFactory (iTerrainObjectFactory *f)
    {
      scfParent->SetTerrainObjectFactory (f);
    }
    virtual iObject *QueryObject()
    {
      return scfParent;
    }
  } scfiTerrainFactoryWrapper;
};

#endif // __CS_TERROBJ_H__
