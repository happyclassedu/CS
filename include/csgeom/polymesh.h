/*
    Crystal Space 3D engine
    Copyright (C) 2003 by Jorrit Tyberghein

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

#ifndef __CS_CSGEOM_POLYMESH_H__
#define __CS_CSGEOM_POLYMESH_H__

#include "igeom/polymesh.h"
#include "csgeom/vector3.h"
#include "csgeom/box.h"

/**
 * \addtogroup geom_utils
 * @{ */
 
/**
 * A convenience polygon mesh implementation that you can feed
 * with vertices and polygons from another source.
 * This mesh optionally supports deformable.
 */
class csPolygonMesh : public iPolygonMesh
{
private:
  bool deformable;
  uint32 change_nr;

  int vt_count;
  csVector3* vt;
  bool delete_vt;	// If true this class is responsible for cleanup.

  int po_count;
  csMeshedPolygon* po;
  bool delete_po;	// If true this class is responsible for cleanup.

public:
  /**
   * Construct a polygon mesh.
   */
  csPolygonMesh ()
  {
    SCF_CONSTRUCT_IBASE (0);
    deformable = false;
    change_nr = 0;
    vt = 0;
    vt_count = 0;
    delete_vt = false;
    po = 0;
    po_count = 0;
    delete_po = false;
  }

  virtual ~csPolygonMesh ()
  {
    if (delete_vt) delete[] vt;
    if (delete_po) delete[] po;
  }

  /**
   * Set the vertices to use for this polygon mesh.
   * If 'delete_vt' is true then this class will do the cleanup itself
   * at destruction. Otherwise you have to make sure that the pointer
   * to the vertices remains valid until this object is deleted.
   */
  void SetVertices (csVector3* vt, int vt_count, bool delete_vt)
  {
    csPolygonMesh::vt = vt;
    csPolygonMesh::vt_count = vt_count;
    csPolygonMesh::delete_vt = delete_vt;
    ShapeChanged ();
  }

  /**
   * Set the polygons to use for this polygon mesh.
   * If 'delete_po' is true then this class will do the cleanup itself
   * at destruction. Otherwise you have to make sure that the pointer
   * to the polygons remains valid until this object is deleted.
   */
  void SetPolygons (csMeshedPolygon* po, int po_count, bool delete_po)
  {
    csPolygonMesh::po = po;
    csPolygonMesh::po_count = po_count;
    csPolygonMesh::delete_po = delete_po;
    ShapeChanged ();
  }

  /**
   * Set vertex count. This will make room for the specified number
   * of vertices so that the user can update them. This class will delete
   * the vertices itself later.
   */
  void SetVertexCount (int vt_count)
  {
    vt = new csVector3[vt_count];
    csPolygonMesh::vt_count = vt_count;
    delete_vt = true;
    ShapeChanged ();
  }

  /**
   * Set polygon count. This will make room for the specified number
   * of polygons so that the user can update them. This class will delete
   * the polygons itself later.
   */
  void SetPolygonCount (int po_count)
  {
    po = new csMeshedPolygon[po_count];
    csPolygonMesh::po_count = po_count;
    delete_po = true;
    ShapeChanged ();
  }

  /**
   * Set deformable on or off.
   */
  void SetDeformable (bool deformable)
  {
    csPolygonMesh::deformable = deformable;
  }

  /**
   * Indicate that the shape has changed.
   */
  void ShapeChanged ()
  {
    change_nr++;
  }

  SCF_DECLARE_IBASE;

  virtual int GetVertexCount () { return vt_count; }
  virtual csVector3* GetVertices () { return vt; }
  virtual int GetPolygonCount () { return po_count; }
  virtual csMeshedPolygon* GetPolygons () { return po; }
  virtual void Cleanup () { }
  virtual bool IsDeformable () const { return deformable; }
  virtual uint32 GetChangeNumber () const { return change_nr; }
};

/**
 * A convenience polygon mesh implementation that represents a cube.
 */
class csPolygonMeshBox : public iPolygonMesh
{
private:
  csVector3 vertices[8];
  csMeshedPolygon polygons[6];
  int vertex_indices[4*6];
  uint32 change_nr;

public:
  /**
   * Construct a cube polygon mesh.
   */
  csPolygonMeshBox (const csBox3& box)
  {
    SCF_CONSTRUCT_IBASE (0);
    change_nr = 0;
    int i;
    for (i = 0 ; i < 6 ; i++)
    {
      polygons[i].num_vertices = 4;
      polygons[i].vertices = &vertex_indices[i*4];
    }
    vertex_indices[0*4+0] = 4;
    vertex_indices[0*4+1] = 5;
    vertex_indices[0*4+2] = 1;
    vertex_indices[0*4+3] = 0;
    vertex_indices[1*4+0] = 5;
    vertex_indices[1*4+1] = 7;
    vertex_indices[1*4+2] = 3;
    vertex_indices[1*4+3] = 1;
    vertex_indices[2*4+0] = 7;
    vertex_indices[2*4+1] = 6;
    vertex_indices[2*4+2] = 2;
    vertex_indices[2*4+3] = 3;
    vertex_indices[3*4+0] = 6;
    vertex_indices[3*4+1] = 4;
    vertex_indices[3*4+2] = 0;
    vertex_indices[3*4+3] = 2;
    vertex_indices[4*4+0] = 6;
    vertex_indices[4*4+1] = 7;
    vertex_indices[4*4+2] = 5;
    vertex_indices[4*4+3] = 4;
    vertex_indices[5*4+0] = 0;
    vertex_indices[5*4+1] = 1;
    vertex_indices[5*4+2] = 3;
    vertex_indices[5*4+3] = 2;
    SetBox (box);
  }

  virtual ~csPolygonMeshBox ()
  {
  }

  /**
   * Set the box.
   */
  void SetBox (const csBox3& box)
  {
    change_nr++;
    vertices[0] = box.GetCorner (0);
    vertices[1] = box.GetCorner (1);
    vertices[2] = box.GetCorner (2);
    vertices[3] = box.GetCorner (3);
    vertices[4] = box.GetCorner (4);
    vertices[5] = box.GetCorner (5);
    vertices[6] = box.GetCorner (6);
    vertices[7] = box.GetCorner (7);
  }

  SCF_DECLARE_IBASE;

  virtual int GetVertexCount () { return 8; }
  virtual csVector3* GetVertices () { return vertices; }
  virtual int GetPolygonCount () { return 6; }
  virtual csMeshedPolygon* GetPolygons () { return polygons; }
  virtual void Cleanup () { }
  virtual bool IsDeformable () const { return false; }
  virtual uint32 GetChangeNumber () const { return change_nr; }
};



/** @} */

#endif // __CS_CSGEOM_POLYMESH_H__

