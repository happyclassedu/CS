/*
    Copyright (C) 2000-2001 by Jorrit Tyberghein

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
#include "qint.h"
#include "csengine/region.h"
#include "csengine/meshobj.h"
#include "csengine/cscoll.h"
#include "csengine/thing.h"
#include "csengine/texture.h"
#include "csengine/sector.h"
#include "csengine/engine.h"
#include "csengine/campos.h"
#include "csengine/material.h"
#include "csengine/polytmap.h"
#include "csengine/curve.h"
#include "csutil/csvector.h"
#include "ivideo/txtmgr.h"

CS_DECLARE_TYPED_VECTOR_NODELETE (csObjectVectorNodelete, iObject);

//---------------------------------------------------------------------------

SCF_IMPLEMENT_IBASE_EXT (csRegion)
  SCF_IMPLEMENTS_EMBEDDED_INTERFACE (iRegion)
SCF_IMPLEMENT_IBASE_EXT_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csRegion::Region)
  SCF_IMPLEMENTS_INTERFACE (iRegion)
SCF_IMPLEMENT_EMBEDDED_IBASE_END

csRegion::csRegion (iEngine* e) : csObject ()
{
  SCF_CONSTRUCT_EMBEDDED_IBASE (scfiRegion);
  engine = e;
}

csRegion::~csRegion ()
{
  scfiRegion.Clear ();
}

void csRegion::Region::Clear ()
{
  scfParent->ObjRemoveAll ();
}

void csRegion::Region::DeleteAll ()
{
  iObjectIterator *iter;

  // First we need to copy the objects to a csVector to avoid
  // messing up the iterator while we are deleting them.
  csObjectVectorNodelete copy;
  for (iter = scfParent->GetIterator ();
  	!iter->IsFinished () ; iter->Next ())
  {
    iObject* o = iter->GetObject ();
    copy.Push (o);
  }
  iter->DecRef ();

  // Now we iterate over all objects in the 'copy' vector and
  // delete them. This will release them as csObject children
  // from this region parent.
  // Note that we traverse the list again and again for every
  // object type since the order in which objects types are deleted
  // is important. i.e. we should first delete all meshes and things
  // and only then delete the sectors.
  int i;
  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iCollection* o = SCF_QUERY_INTERFACE_FAST (obj, iCollection);
      if (!o) continue;

      scfParent->engine->GetCollections ()->RemoveCollection (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iMeshWrapper* o = SCF_QUERY_INTERFACE_FAST (obj, iMeshWrapper);
      if (!o) continue;

      scfParent->engine->GetMeshes ()->RemoveMesh (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iMeshFactoryWrapper* o = SCF_QUERY_INTERFACE_FAST (obj, iMeshFactoryWrapper);
      if (!o) continue;

      scfParent->engine->GetMeshFactories ()->RemoveMeshFactory (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iSector* o = SCF_QUERY_INTERFACE_FAST (obj, iSector);
      if (!o) continue;

      o->GetPrivateObject ()->CleanupReferences ();
      scfParent->engine->GetSectors ()->Remove (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iMaterialWrapper* o = SCF_QUERY_INTERFACE_FAST (obj, iMaterialWrapper);
      if (!o) continue;

      scfParent->engine->GetMaterialList ()->Remove (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iTextureWrapper* o = SCF_QUERY_INTERFACE_FAST (obj, iTextureWrapper);
      if (!o) continue;

      scfParent->engine->GetTextureList ()->Remove (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* obj = copy[i];
      iCameraPosition* o = SCF_QUERY_INTERFACE_FAST (obj, iCameraPosition);
      if (!o) continue;

      scfParent->engine->GetCameraPositions ()->RemoveCameraPosition (o);
      scfParent->ObjRemove (obj);
      copy[i] = NULL;
      o->DecRef ();
    }

#ifdef CS_DEBUG
  // Sanity check (only in debug mode). There should be no more
  // non-NULL references in the copy array now.
  for (i = 0 ; i < copy.Length () ; i++)
    if (copy[i])
    {
      iObject* o = copy[i];
      csEngine::current_engine->ReportBug ("\
There is still an object in the array after deleting region contents!\n\
Object name is '%s'",
	o->GetName () ? o->GetName () : "<NoName>");
      CS_ASSERT (false);
    }
#endif // CS_DEBUG
}

bool csRegion::Region::PrepareTextures ()
{
  iObjectIterator *iter;
  iTextureManager* txtmgr = csEngine::current_engine->G3D->GetTextureManager();
  //txtmgr->ResetPalette ();

  // First register all textures to the texture manager.
  {
    for (iter = scfParent->GetIterator ();
  	!iter->IsFinished () ; iter->Next())
    {
      iTextureWrapper* csth =
        SCF_QUERY_INTERFACE_FAST (iter->GetObject (), iTextureWrapper);
      if (csth)
      {
        if (!csth->GetTextureHandle ())
          csth->Register (txtmgr);
        csth->DecRef ();
      }
    }
    iter->DecRef ();
  }

  // Prepare all the textures.
  //@@@ Only prepare new textures: txtmgr->PrepareTextures ();
  {
    for (iter = scfParent->GetIterator ();
  	!iter->IsFinished () ; iter->Next())
    {
      iTextureWrapper* csth =
        SCF_QUERY_INTERFACE_FAST (iter->GetObject (), iTextureWrapper);
      if (csth)
      {
        csth->GetTextureHandle ()->Prepare ();
        csth->DecRef ();
      }
    }
    iter->DecRef ();
  }

  // Then register all materials to the texture manager.
  {
    for (iter = scfParent->GetIterator ();
  	!iter->IsFinished () ; iter->Next())
    {
      iMaterialWrapper* csmh =
        SCF_QUERY_INTERFACE_FAST (iter->GetObject (), iMaterialWrapper);
      if (csmh)
      {
        if (!csmh->GetMaterialHandle ())
          csmh->Register (txtmgr);
        csmh->DecRef ();
      }
    }
    iter->DecRef ();
  }

  // Prepare all the materials.
  //@@@ Only prepare new materials: txtmgr->PrepareMaterials ();
  {
    for (iter = scfParent->GetIterator ();
  	!iter->IsFinished () ; iter->Next())
    {
      iMaterialWrapper* csmh =
        SCF_QUERY_INTERFACE_FAST (iter->GetObject (), iMaterialWrapper);
      if (csmh)
      {
        csmh->GetMaterialHandle ()->Prepare ();
        csmh->DecRef ();
      }
    }
    iter->DecRef ();
  }

  return true;
}

bool csRegion::Region::ShineLights ()
{
  scfParent->engine->ShineLights (this);
  return true;
}

bool csRegion::Region::Prepare ()
{
  if (!PrepareTextures ()) return false;
  if (!ShineLights ()) return false;
  return true;
}

void csRegion::AddToRegion (iObject* obj)
{
  ObjAdd (obj);
}

void csRegion::ReleaseFromRegion (iObject* obj)
{
  ObjRemove (obj);
}

iSector* csRegion::Region::FindSector (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iSector, iName);
}

iMeshWrapper* csRegion::Region::FindMeshObject (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iMeshWrapper, iName);
}

iMeshFactoryWrapper* csRegion::Region::FindMeshFactory (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iMeshFactoryWrapper, iName);
}

iTextureWrapper* csRegion::Region::FindTexture (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iTextureWrapper, iName);
}

iMaterialWrapper* csRegion::Region::FindMaterial (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iMaterialWrapper, iName);
}

iCameraPosition* csRegion::Region::FindCameraPosition (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iCameraPosition, iName);
}

iCollection* csRegion::Region::FindCollection (const char *iName)
{
  return CS_GET_NAMED_CHILD_OBJECT_FAST(scfParent, iCollection, iName);
}

bool csRegion::IsInRegion (iObject* iobj)
{
  return iobj->GetObjectParent () == this;
}

bool csRegion::Region::IsInRegion (iObject* iobj)
{
  return scfParent->IsInRegion (iobj);
}
