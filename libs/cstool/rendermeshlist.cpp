/*
  Copyright (C) 2003 by Marten Svanfeldt

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

#include "csgeom/math3d.h"
#include "csgeom/transfrm.h"

#include "csutil/objreg.h"
#include "csutil/ref.h"
#include "cstool/rendermeshlist.h"


#include "iengine/engine.h"
#include "ivideo/rendermesh.h"


csRenderMeshList::csRenderMeshList (iEngine* engine)
{
  csRenderMeshList::engine = engine;
}

csRenderMeshList::~csRenderMeshList ()
{
}

void csRenderMeshList::AddRenderMeshes (csRenderMesh** meshes, int num,
                                   long renderPriority,
				   csZBufMode z_buf_mode, 
				   const csBox3& bbox)
{
  renderMeshListInfo* entry;

  //check if we have rp or need to add it
  if (renderPriority > renderList.Length () - 1 || renderList.Get
    ((uint32)renderPriority) == 0)
  {
    entry = new renderMeshListInfo;
    entry->renderPriority = renderPriority;
    if (engine)
    {
      entry->sortingOption = engine->GetRenderPrioritySorting (renderPriority);
    }
    else
    {
      entry->sortingOption = 0;
    }

    renderList.Put ((uint32)renderPriority, entry);
  }
  else
  {
    entry = renderList.Get ((uint32)renderPriority);
  }

  for (int i = 0; i < num; ++i)
  {
    meshes[i]->z_buf_mode = z_buf_mode;
    entry->meshList.Push (meshListEntry (meshes[i], bbox));
  }
}

void csRenderMeshList::CullToSphere (const csSphere& sphere)
{
  csVector3 sphCenter (sphere.GetCenter ());
  float sphSqRadius = sphere.GetRadius ();

  for (int i = 0; i < renderList.Length(); i++)
  {
    renderMeshListInfo*& mli = renderList[i];
    if (!mli) continue;

    for (int j = mli->meshList.Length(); --j >= 0;)
    {
      const meshListEntry& entry = mli->meshList[j];
      if (!csIntersect3::BoxSphere (entry.bbox, sphCenter,
	sphSqRadius))
	mli->meshList.DeleteIndex (j);
    }
  }
}

void csRenderMeshList::Empty ()
{
  csPDelArray < renderMeshListInfo >::Iterator it = renderList.GetIterator ();

  while (it.HasNext ())
  {
    renderMeshListInfo* listEnt = it.Next ();
    if (0 == listEnt)
      continue;

    listEnt->meshList.Empty ();
  }
}

int csRenderMeshList::SortMeshMaterial (meshListEntry const& me1, 
					meshListEntry const& me2)
{
  const csRenderMesh* m1 = me1.rm;
  const csRenderMesh* m2 = me2.rm;

  if (m1->portal != 0 && m2->portal == 0)
    return 1;
  else if (m2->portal != 0 && m1->portal == 0)
    return -1;
  else
  {
    if (m1->material > m2->material)
      return 1;
    if (m1->material < m2->material)
      return -1;
  }
  if (m1->geometryInstance > m2->geometryInstance)
    return 1;
  if (m1->geometryInstance < m2->geometryInstance)
    return -1;
  return 0;
}

int csRenderMeshList::SortMeshBack2Front(meshListEntry const& me1, 
					 meshListEntry const& me2)
{
  const csRenderMesh* m1 = me1.rm;
  const csRenderMesh* m2 = me2.rm;

  float z1 = m1->camera_origin.z;
  float z2 = m2->camera_origin.z;
  if (z1 < z2) return 1;
  else if (z1 > z2) return -1;
  return SortMeshMaterial (me1, me2);
}

int csRenderMeshList::SortMeshFront2Back(meshListEntry const& me1, 
					 meshListEntry const& me2)
{
  const csRenderMesh* m1 = me1.rm;
  const csRenderMesh* m2 = me2.rm;

  // We need GetT2OTranslation() but that does a matrix*vector multiplication.
  // Since we only need one component ('z') we can do it a bit more optimal.
  // @@@ Even more optimal would be to do the calculation in the mesh
  // itself and put the 'z' in the csRenderMesh structure.
  const csReversibleTransform& t1 = m1->object2camera;
  const csReversibleTransform& t2 = m2->object2camera;
  const csMatrix3& t1m = t1.GetO2T ();
  const csMatrix3& t2m = t2.GetO2T ();
  const csVector3& t1v = t1.GetO2TTranslation ();
  const csVector3& t2v = t2.GetO2TTranslation ();
  float z1 = - t1m.m31*t1v.x - t1m.m32*t1v.y - t1m.m33*t1v.z;
  float z2 = - t2m.m31*t2v.x - t2m.m32*t2v.y - t2m.m33*t2v.z;

  if (z1 < z2) return -1;
  else if (z1 > z2) return 1;
  return SortMeshMaterial (me1, me2);
}

int csRenderMeshList::SortMeshLists ()
{
  int numObjects = 0;
  csPDelArray < renderMeshListInfo >::Iterator it = renderList.GetIterator ();
  while (it.HasNext ())
  {
    renderMeshListInfo* listEnt = it.Next ();
    if (0 == listEnt)
      continue;

    if (listEnt->sortingOption == CS_RENDPRI_BACK2FRONT)
    {
      listEnt->meshList.Sort (SortMeshBack2Front);
    }
    else if (listEnt->sortingOption == CS_RENDPRI_FRONT2BACK)
    {
      listEnt->meshList.Sort (SortMeshFront2Back);
    }
    else
    {
      listEnt->meshList.Sort (SortMeshMaterial);
    }
    numObjects += listEnt->meshList.Length ();
  }
  return numObjects;
}

void csRenderMeshList::GetSortedMeshes (csRenderMesh** meshes)
{
  csPDelArray < renderMeshListInfo >::Iterator it = renderList.GetIterator ();
  while (it.HasNext ())
  {
    renderMeshListInfo* listEnt = it.Next ();
    if (listEnt)
    {
      int numObjects = listEnt->meshList.Length ();
      for (int j = 0 ; j < numObjects ; j++)
	*meshes++ = listEnt->meshList[j].rm;
    }
  }
}

