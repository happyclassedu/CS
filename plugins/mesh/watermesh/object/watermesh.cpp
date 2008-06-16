/*
Copyright (C) 2004 by Jorrit Tyberghein

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
#include "csqsqrt.h"

#include "csgeom/box.h"
#include "csgeom/frustum.h"
#include "csgeom/math3d.h"
#include "csgeom/trimesh.h"
#include "csgfx/renderbuffer.h"
#include "csgfx/shadervarcontext.h"
#include "cstool/rviewclipper.h"
#include "iengine/camera.h"
#include "iengine/material.h"
#include "iengine/mesh.h"
#include "iengine/movable.h"
#include "iengine/rview.h"
#include "iutil/objreg.h"
#include "ivideo/graph2d.h"
#include "ivideo/graph3d.h"
#include "ivideo/material.h"
#include "ivideo/rendermesh.h"

#include "watermesh.h"


CS_IMPLEMENT_PLUGIN

using namespace CS::Plugins::WaterMesh;

csWaterMeshObject::csWaterMeshObject (csWaterMeshObjectFactory* factory) :
  scfImplementationType (this)
{
  myRenderBufferAccessor.AttachNew (new RenderBufferAccessor (this));

  csWaterMeshObject::factory = factory;
  logparent = 0;
  initialized = false;

  material = 0;
  MixMode = 0;

  color.red = 0;
  color.green = 0;
  color.blue = 0;
  factory_color_nr = (uint)~0;
  mesh_colors_dirty_flag = true;

  current_lod = 1;
  current_features = 0;

  g3d = csQueryRegistry<iGraphics3D> (factory->object_reg);

  variableContext.AttachNew (new csShaderVariableContext);
}

csWaterMeshObject::~csWaterMeshObject ()
{
}

iMeshObjectFactory* csWaterMeshObject::GetFactory () const
{
  return (csWaterMeshObjectFactory*)factory;
}

bool csWaterMeshObject::SetMaterialWrapper (iMaterialWrapper* mat)
{
  material = mat;
  return true;
}

void csWaterMeshObject::SetupBufferHolder ()
{
  if (bufferHolder == 0)
    bufferHolder.AttachNew (new csRenderBufferHolder);

  // Make sure the factory is ok and his its buffers.
  factory->SetupFactory ();

  // When creating buffers we basically have two ways. Either
  // we can create the buffer immediatelly and supply it in the context.
  // Or else we create an accessor for the buffer so that the
  // shader/renderer can fetch the buffer later. The first approach
  // is best in case of buffers that are almost always needed. For
  // example, the vertex positions are certainly needed in all cases.
  // The second approach is best in case of buffers that are only
  // needed in some situations. For example, normals and colors may
  // only be needed by some shaders. So delaying creation avoids
  // the creation of potentially unneeded buffers there.

  // Indices are fetched directly from the factory.
  bufferHolder->SetRenderBuffer (CS_BUFFER_INDEX, factory->index_buffer);

  // Vertices are fetched from the factory.
  bufferHolder->SetRenderBuffer (CS_BUFFER_POSITION, factory->vertex_buffer);

  // Texels are fetched from the factory.
  bufferHolder->SetRenderBuffer (CS_BUFFER_TEXCOORD0, factory->texel_buffer);

  // Normals are fetched from the factory but we use an accessor
  // for those because they are not always needed.
  // Colors are fetched from the object because we need to add the mesh
  // base color to the static colors in the factory.
  bufferHolder->SetAccessor (myRenderBufferAccessor, CS_BUFFER_NORMAL_MASK | CS_BUFFER_COLOR_MASK);
}

void csWaterMeshObject::SetupObject ()
{
  if (!initialized)
  {
    initialized = true;
    SetupBufferHolder ();
  }
}

/*
* This function actually supplies the meshes to render to the
* 3D renderer (which will call g3d->DrawMesh()). In this simple
* case there is only one render mesh but more complex objects
* can have multiple render meshes.
*/
csRenderMesh** csWaterMeshObject::GetRenderMeshes (
  int& n, iRenderView* rview, 
  iMovable* movable, uint32 frustum_mask)
{
  n = 0;

  if (vis_cb) if (!vis_cb->BeforeDrawing (this, rview)) return false;

  SetupObject ();

  iCamera* camera = rview->GetCamera ();

  int clip_portal, clip_plane, clip_z_plane;
  CS::RenderViewClipper::CalculateClipSettings (rview->GetRenderContext (),
      frustum_mask, clip_portal, clip_plane, clip_z_plane);

  const csReversibleTransform o2wt = movable->GetFullTransform ();
  const csVector3& wo = o2wt.GetOrigin ();

  CS_ASSERT (material != 0);
  material->Visit ();

  factory->PrepareBuffers ();
  if (factory_color_nr != factory->color_nr)
  {
    // The factory colors have changed. Set the
    // colors dirty flag to true to force an update
    // there in the PreGetShaderVariableValue.
    factory_color_nr = factory->color_nr;
    mesh_colors_dirty_flag = true;
  }

  bool rmCreated;
  csRenderMesh*& meshPtr = rmHolder.GetUnusedMesh (rmCreated,
    rview->GetCurrentFrameNumber ());

  meshPtr->mixmode = MixMode;
  meshPtr->clip_portal = clip_portal;
  meshPtr->clip_plane = clip_plane;
  meshPtr->clip_z_plane = clip_z_plane;
  meshPtr->do_mirror = camera->IsMirrored ();
  meshPtr->meshtype = CS_MESHTYPE_TRIANGLES;
  meshPtr->indexstart = 0;
  meshPtr->indexend = WATER_TRIS * 3;	// 12 triangles.
  meshPtr->material = material;
  meshPtr->worldspace_origin = wo;
  meshPtr->object2world = o2wt;
  if (rmCreated)
  {
    meshPtr->buffers = bufferHolder;
    meshPtr->variablecontext = variableContext;
  }

  meshPtr->geometryInstance = (void*)factory;

  n = 1;
  return &meshPtr;
}

bool csWaterMeshObject::HitBeamOutline (const csVector3& start,
                                        const csVector3& end, csVector3& isect, float* pr)
{
  // This is now closer to an outline hitting method. It will
  // return as soon as it touches any triangle in the mesh, and
  // will be a bit faster than its more accurate cousin (below).

  csSegment3 seg (start, end);
  int i, max = WATER_TRIS;
  csTriangle *tr = factory->GetTriangles();
  csVector3 *vrt = factory->GetVertices ();
  for (i = 0 ; i < max ; i++)
  {
    if (csIntersect3::SegmentTriangle (seg, vrt[tr[i].a], vrt[tr[i].b],
      vrt[tr[i].c], isect))
    {
      if (pr) *pr = csQsqrt (csSquaredDist::PointPoint (start, isect) /
        csSquaredDist::PointPoint (start, end));

      return true;
    }
  }
  return false;
}

bool csWaterMeshObject::HitBeamObject (const csVector3& start,
                                       const csVector3& end, csVector3& isect, float *pr, int* polygon_idx,
                                       iMaterialWrapper** material)
{
  if (material) *material = csWaterMeshObject::material;
  if (polygon_idx) *polygon_idx = -1;
  // This is the slow version. Use for an accurate hit on the object.
  // It will cycle through every triangle in the mesh serching for the
  // closest intersection. Slower, but returns the closest hit.
  // Usage is optional.

  csSegment3 seg (start, end);
  int i, max = WATER_TRIS;
  float tot_dist = csSquaredDist::PointPoint (start, end);
  float dist, temp;
  float itot_dist = 1 / tot_dist;
  dist = temp = tot_dist;
  csVector3 *vrt = factory->GetVertices (), tmp;
  csTriangle *tr = factory->GetTriangles();
  for (i = 0 ; i < max ; i++)
  {
    if (csIntersect3::SegmentTriangle (seg, vrt[tr[i].a], vrt[tr[i].b],
      vrt[tr[i].c], tmp))
    {
      temp = csSquaredDist::PointPoint (start, tmp);
      if (temp < dist)
      {
        isect = tmp;
        dist = temp;
        if (polygon_idx) *polygon_idx = i;
      }
    }
  }
  if (pr) *pr = csQsqrt (dist * itot_dist);
  if (dist >= tot_dist)
    return false;
  return true;
}

iObjectModel* csWaterMeshObject::GetObjectModel ()
{
  return factory->GetObjectModel ();
}

void csWaterMeshObject::PreGetBuffer (csRenderBufferHolder *holder, 
                                      csRenderBufferName buffer)
{
  if (buffer == CS_BUFFER_COLOR)
  {
    if (mesh_colors_dirty_flag)
    {
      if (!color_buffer)
      {
        color_buffer = csRenderBuffer::CreateRenderBuffer (
          WATER_VERTS, CS_BUF_STATIC,
          CS_BUFCOMP_FLOAT, 3);
      }
      mesh_colors_dirty_flag = false;
      const csColor* factory_colors = factory->colors;
      int i;
      csColor colors[WATER_VERTS];
      for (i = 0 ; i < WATER_VERTS ; i++)
        colors[i] = factory_colors[i]+color;
      // Copy the data into the render buffer
      // since we don't keep a local copy of the color buffer here.
      color_buffer->CopyInto (colors, WATER_VERTS);
    }
    holder->SetRenderBuffer (CS_BUFFER_COLOR, color_buffer);
  } 
  else 
  {
    factory->PreGetBuffer (holder, buffer);
  }
}

//----------------------------------------------------------------------

csWaterMeshObjectFactory::csWaterMeshObjectFactory (
    iMeshObjectType *pParent, iObjectRegistry* object_reg)
  : scfImplementationType (this, pParent)
{
  csWaterMeshObjectFactory::object_reg = object_reg;

  csStringID base_mesh_id = GetBaseID (object_reg);
  csRef<csTriangleMeshPointer> trimesh_base;
  trimesh_base.AttachNew (new csTriangleMeshPointer (
	vertices, WATER_VERTS, triangles, WATER_TRIS));
  SetTriangleData (base_mesh_id, trimesh_base);

  logparent = 0;
  water_type = pParent;
  initialized = false;
  object_bbox_valid = false;
  color_nr = 0;

  g3d = csQueryRegistry<iGraphics3D> (object_reg);

  mesh_vertices_dirty_flag = true;
  mesh_texels_dirty_flag = true;
  mesh_normals_dirty_flag = true;
  mesh_triangle_dirty_flag = true;
}

csWaterMeshObjectFactory::~csWaterMeshObjectFactory ()
{
}

void csWaterMeshObjectFactory::CalculateBBoxRadius ()
{
  object_bbox_valid = true;
  csVector3& v0 = vertices[0];
  object_bbox.StartBoundingBox (v0);
  int i;
  for (i = 1 ; i < WATER_VERTS ; i++)
  {
    csVector3& v = vertices[i];
    object_bbox.AddBoundingVertexSmart (v);
  }

  const csVector3& center = object_bbox.GetCenter ();
  float max_sqradius = 0.0f;
  for (i = 0 ; i < WATER_VERTS ; i++)
  {
    csVector3& v = vertices[i];
    float sqradius = csSquaredDist::PointPoint (center, v);
    if (sqradius > max_sqradius) max_sqradius = sqradius;
  }

  radius = csQsqrt (max_sqradius);
}

void csWaterMeshObjectFactory::GetRadius (float& radius, csVector3& center)
{
  SetupFactory ();
  if (!object_bbox_valid) CalculateBBoxRadius ();
  radius = this->radius;
  center = object_bbox.GetCenter();
}

const csBox3& csWaterMeshObjectFactory::GetObjectBoundingBox ()
{
  SetupFactory ();
  if (!object_bbox_valid) CalculateBBoxRadius ();
  return object_bbox;
}

void csWaterMeshObjectFactory::SetObjectBoundingBox (const csBox3& bbox)
{
  object_bbox_valid = true;
  object_bbox = bbox;
}

void csWaterMeshObjectFactory::SetupFactory ()
{
  if (!initialized)
  {
    initialized = true;
    object_bbox_valid = false;
	  
  	int i = 0;
  	
    int rtWVs = (int)sqrt(WATER_VERTS);
    
  	for(i = 0; i < WATER_VERTS; i++)
  	{
//		printf("Setting vertex %d to: <%d, %d, %d>\n", i, i % rtWVs, 0, i / rtWVs);
	    vertices[i].Set((int)(i % rtWVs), 0, (int)(i / rtWVs));
  	}
   
   
   // SET TEXELS!!!
   
    for(i = 0; i < WATER_VERTS; i++)
    {
  	  normals[i].Set((int)rand(), (int)rand(), (int)rand());
  	  colors[i].Set(0, 0, 1);
    }
    
    for(i = 0; i < WATER_TRIS; i+=2)
    {
    	int j = i / 2;
    	int x = j % (rtWVs - 1);
    	int y = j / (rtWVs - 1);
    	
//    	printf("Setting triangle %d to: <%d %d %d>\n", i, x + y * rtWVs, 
//    					x + (y + 1) * rtWVs, 
//    					(x + 1) + y * rtWVs);
    	triangles[i].Set(x + y * rtWVs, 
    					x + (y + 1) * rtWVs, 
    					(x + 1) + y * rtWVs);
    	
//    	printf("Setting triangle %d to: <%d %d %d>\n", i + 1, (x + 1) + y * rtWVs, 
//    					x + (y + 1) * rtWVs, 
//    					(x + 1) + (y + 1) * rtWVs);
    	triangles[i + 1].Set((x + 1) + y * rtWVs, 
    					x + (y + 1) * rtWVs, 
    					(x + 1) + (y + 1) * rtWVs);
    }
    
    PrepareBuffers ();
  }
}

void csWaterMeshObjectFactory::PreGetBuffer (csRenderBufferHolder* holder, 
                                             csRenderBufferName buffer)
{
  if (buffer == CS_BUFFER_NORMAL)
  {
    if (mesh_normals_dirty_flag)
    {
      mesh_normals_dirty_flag = false;
      if (!normal_buffer)
      {
        normal_buffer = csRenderBuffer::CreateRenderBuffer (
          WATER_VERTS, CS_BUF_STATIC,
          CS_BUFCOMP_FLOAT, 3);
      }
      // Don't copy the data, have the buffer store a pointer instead.
      normal_buffer->SetData (normals);
    }
    holder->SetRenderBuffer (CS_BUFFER_NORMAL, normal_buffer);
  }
}

void csWaterMeshObjectFactory::Invalidate ()
{
  object_bbox_valid = false;

  mesh_vertices_dirty_flag = true;
  mesh_texels_dirty_flag = true;
  mesh_normals_dirty_flag = true;
  mesh_triangle_dirty_flag = true;

  color_nr++;

  ShapeChanged ();
}

void csWaterMeshObjectFactory::PrepareBuffers ()
{
  if (mesh_vertices_dirty_flag)
  {
    mesh_vertices_dirty_flag = false;
    if (!vertex_buffer)
    {
      // Create a buffer that doesn't copy the data.
      vertex_buffer = csRenderBuffer::CreateRenderBuffer (
        WATER_VERTS, CS_BUF_STATIC, CS_BUFCOMP_FLOAT,
        3);
    }
    vertex_buffer->CopyInto (vertices, WATER_VERTS);
  }
  if (mesh_texels_dirty_flag)
  {
    mesh_texels_dirty_flag = false;
    if (!texel_buffer)
    {
      // Create a buffer that doesn't copy the data.
      texel_buffer = csRenderBuffer::CreateRenderBuffer (
        WATER_VERTS, CS_BUF_STATIC, CS_BUFCOMP_FLOAT,
        2);
    }
    texel_buffer->CopyInto (texels, WATER_VERTS);
  }
  if (mesh_triangle_dirty_flag)
  {
    mesh_triangle_dirty_flag = false;
    if (!index_buffer)
      index_buffer = csRenderBuffer::CreateIndexRenderBuffer (
      WATER_TRIS*3,
      CS_BUF_STATIC, CS_BUFCOMP_UNSIGNED_INT,
      0, WATER_VERTS-1);
    index_buffer->CopyInto (triangles, WATER_TRIS*3);
  }
}

csPtr<iMeshObject> csWaterMeshObjectFactory::NewInstance ()
{
  csRef<csWaterMeshObject> cm;
  cm.AttachNew (new csWaterMeshObject (this));

  csRef<iMeshObject> im = scfQueryInterface<iMeshObject> (cm);
  return csPtr<iMeshObject> (im);
}

//----------------------------------------------------------------------

SCF_IMPLEMENT_FACTORY (csWaterMeshObjectType)


csWaterMeshObjectType::csWaterMeshObjectType (iBase* pParent) : 
scfImplementationType (this, pParent), object_reg(0)
{
}

csWaterMeshObjectType::~csWaterMeshObjectType ()
{
}

csPtr<iMeshObjectFactory> csWaterMeshObjectType::NewFactory ()
{
  csRef<csWaterMeshObjectFactory> cm;
  cm.AttachNew (new csWaterMeshObjectFactory (this,
    object_reg));
  csRef<iMeshObjectFactory> ifact (
    scfQueryInterface<iMeshObjectFactory> (cm));
  return csPtr<iMeshObjectFactory> (ifact);
}

bool csWaterMeshObjectType::Initialize (iObjectRegistry* object_reg)
{
  csWaterMeshObjectType::object_reg = object_reg;
  return true;
}

