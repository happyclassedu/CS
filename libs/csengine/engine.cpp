/*
    Copyright (C) 1998-2001 by Jorrit Tyberghein

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
#include "csutil/scf.h"
#include "csutil/cspmeter.h"
#include "csengine/engine.h"
#include "csengine/halo.h"
#include "csengine/camera.h"
#include "csengine/campos.h"
#include "csengine/light.h"
#include "csengine/polyplan.h"
#include "csengine/polytmap.h"
#include "csengine/polygon.h"
#include "csengine/pol2d.h"
#include "csengine/polytext.h"
#include "csengine/thing.h"
#include "csengine/meshobj.h"
#include "csengine/cscoll.h"
#include "csengine/sector.h"
#include "csengine/cbufcube.h"
#include "csengine/texture.h"
#include "csengine/material.h"
#include "csengine/lghtmap.h"
#include "csengine/stats.h"
#include "csengine/cbuffer.h"
#include "csengine/lppool.h"
#include "csengine/radiosty.h"
#include "csengine/region.h"
#include "csgeom/fastsqrt.h"
#include "csgeom/polypool.h"
#include "csgfx/csimage.h"
#include "csutil/util.h"
#include "csutil/cfgacc.h"
#include "igraphic/image.h"
#include "igraphic/imageio.h"
#include "isys/vfs.h"
#include "ivideo/halo.h"
#include "ivideo/txtmgr.h"
#include "ivideo/graph3d.h"
#include "isys/event.h"
#include "iutil/cfgmgr.h"
#include "iutil/databuff.h"
#include "imap/reader.h"
#include "imesh/lighting.h"
#include "ivaria/reporter.h"

//---------------------------------------------------------------------------

void csEngine::Report (const char* description, ...)
{
  va_list arg;
  va_start (arg, description);

  if (Reporter)
  {
    Reporter->ReportV (CS_REPORTER_SEVERITY_NOTIFY,
    	"crystalspace.engine.notify", description, arg);
  }
  else
  {
    System->PrintfV (CS_MSG_CONSOLE, description, arg);
  }
  va_end (arg);
}

void csEngine::Warn (const char* description, ...)
{
  va_list arg;
  va_start (arg, description);

  if (Reporter)
  {
    Reporter->ReportV (CS_REPORTER_SEVERITY_WARNING,
    	"crystalspace.engine.warning", description, arg);
  }
  else
  {
    System->PrintfV (CS_MSG_WARNING, description, arg);
  }
  va_end (arg);
}

void csEngine::ReportBug (const char* description, ...)
{
  va_list arg;
  va_start (arg, description);

  if (Reporter)
  {
    Reporter->ReportV (CS_REPORTER_SEVERITY_BUG,
    	"crystalspace.engine.bug", description, arg);
  }
  else
  {
    System->PrintfV (CS_MSG_INTERNAL_ERROR, description, arg);
  }
  va_end (arg);
}

//---------------------------------------------------------------------------

csLightIt::csLightIt (csEngine* e, csRegion* r) : engine (e), region (r)
{
  Restart ();
}

bool csLightIt::NextSector ()
{
  sector_idx++;
  if (region)
    while (sector_idx < engine->sectors.Length ()
	  && !region->IsInRegion (GetLastSector ()))
      sector_idx++;
  if (sector_idx >= engine->sectors.Length ()) return false;
  return true;
}

void csLightIt::Restart ()
{
  sector_idx = -1;
  light_idx = 0;
}

csLight* csLightIt::Fetch ()
{
  csSector* sector;
  if (sector_idx == -1)
  {
    if (!NextSector ()) return NULL;
    light_idx = -1;
  }

  if (sector_idx >= engine->sectors.Length ()) return NULL;
  sector = engine->sectors[sector_idx]->GetPrivateObject ();

  // Try next light.
  light_idx++;

  if (light_idx >= sector->GetLightCount ())
  {
    // Go to next sector.
    light_idx = -1;
    if (!NextSector ()) return NULL;
    // Initialize iterator to start of sector and recurse.
    return Fetch ();
  }

  return sector->GetLight (light_idx);
}

csSector* csLightIt::GetLastSector ()
{
  return engine->sectors[sector_idx]->GetPrivateObject ();
}

//---------------------------------------------------------------------------

csSectorIt::csSectorIt (csSector* sector, const csVector3& pos, float radius)
{
  csSectorIt::sector = sector;
  csSectorIt::pos = pos;
  csSectorIt::radius = radius;
  recursive_it = NULL;

  Restart ();
}

csSectorIt::~csSectorIt ()
{
  delete recursive_it;
}

void csSectorIt::Restart ()
{
  cur_poly = -1;
  delete recursive_it;
  recursive_it = NULL;
  has_ended = false;
}

csSector* csSectorIt::Fetch ()
{
  if (has_ended) return NULL;

  if (recursive_it)
  {
    csSector* rc = recursive_it->Fetch ();
    if (rc)
    {
      last_pos = recursive_it->GetLastPosition ();
      return rc;
    }
    delete recursive_it;
    recursive_it = NULL;
  }

  if (cur_poly == -1)
  {
    cur_poly = 0;
    last_pos = pos;
    return sector;
  }

#if 0
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  // @@@ This function should try to use the octree if available to
  // quickly discard lots of polygons that cannot be close enough.
  while (cur_poly < sector->GetPolygonCount ())
  {
    csPolygon3D* p = sector->GetPolygon3D (cur_poly);
    cur_poly++;
    csPortal* po = p->GetPortal ();
    if (po)
    {
      const csPlane3& wpl = p->GetPlane ()->GetWorldPlane ();
      float d = wpl.Distance (pos);
      if (d < radius && csMath3::Visible (pos, wpl))
      {
        if (!po->GetSector ()) po->CompleteSector ();
	csVector3 new_pos;
	if (po->flags.Check (CS_PORTAL_WARP))
	  new_pos = po->Warp (pos);
	else
	  new_pos = pos;
	recursive_it = new csSectorIt (po->GetSector (), new_pos, radius);
	return Fetch ();
      }
    }
  }
#endif

  // End search.
  has_ended = true;
  return NULL;
}

//---------------------------------------------------------------------------

csObjectIt::csObjectIt (csEngine* e, csSector* sector,
  const csVector3& pos, float radius)
{
  csObjectIt::engine = e;
  csObjectIt::start_sector = sector;
  csObjectIt::start_pos = pos;
  csObjectIt::radius = radius;
  sectors_it = new csSectorIt (sector, pos, radius);

  Restart ();
}

csObjectIt::~csObjectIt ()
{
  delete sectors_it;
}

void csObjectIt::Restart ()
{
  CurrentList = ITERATE_DYNLIGHTS;
  cur_object = engine->GetFirstDynLight ();
  sectors_it->Restart ();
}

void csObjectIt::StartStatLights ()
{
  CurrentList = ITERATE_STATLIGHTS;
  cur_idx = 0;
}

void csObjectIt::StartMeshes ()
{
  CurrentList = ITERATE_MESHES;
  cur_idx = 0;
}

void csObjectIt::EndSearch ()
{
  CurrentList = ITERATE_NONE;
}

iObject* csObjectIt::Fetch ()
{
  if (CurrentList == ITERATE_NONE) return NULL;

  // Handle csDynLight.
  if (CurrentList == ITERATE_DYNLIGHTS)
  {
    if (cur_object == NULL)
      CurrentList = ITERATE_SECTORS;
    else
    {
      do
      {
        iObject* rc = cur_object;
	iDynLight *dl = SCF_QUERY_INTERFACE_FAST (rc, iDynLight);
	if (!dl) cur_object = NULL;
	else
	{
          cur_object = dl->GetNext ()->QueryObject ();
          float r = dl->QueryLight ()->GetRadius () + radius;
          if (csSquaredDist::PointPoint (dl->QueryLight ()->GetCenter (),
		cur_pos) <= r*r)
          return rc;
	}
      }
      while (cur_object);
      if (cur_object == NULL)
        CurrentList = ITERATE_SECTORS;
    }
  }

  // Handle this sector first.
  if (CurrentList == ITERATE_SECTORS)
  {
    cur_sector = sectors_it->Fetch ();
    if (!cur_sector)
    {
      EndSearch ();
      return NULL;
    }
    cur_pos = sectors_it->GetLastPosition ();

    StartStatLights ();
    return cur_sector;
  }

  // Handle csLight.
  if (CurrentList == ITERATE_STATLIGHTS)
  {
    if (cur_idx >= cur_sector->GetLightCount ())
      StartMeshes ();
    else
    {
      do
      {
        iObject* rc = cur_sector->GetLight (cur_idx)->scfiLight.QueryObject ();
        cur_idx++;
	iStatLight* sl = SCF_QUERY_INTERFACE_FAST (rc, iStatLight);
	if (sl)
	{
          float r = sl->QueryLight ()->GetRadius () + radius;
          if (csSquaredDist::PointPoint (sl->QueryLight ()->GetCenter (),
	    cur_pos) <= r*r)
              return rc;
	}
      }
      while (cur_idx < cur_sector->GetLightCount ());
      if (cur_idx >= cur_sector->GetLightCount ())
        StartMeshes ();
    }
  }

  // Handle csMeshWrapper.
  if (CurrentList == ITERATE_MESHES)
  {
    if (cur_idx >= cur_sector->GetMeshCount ())
      CurrentList = ITERATE_SECTORS;
    else
    {
      iObject* rc = cur_sector->GetMesh (cur_idx)->scfiMeshWrapper.QueryObject ();
      cur_idx++;
      return rc;
    }
  }
  else
    CurrentList = ITERATE_SECTORS;

  return NULL;
}

//---------------------------------------------------------------------------

int csEngine::frame_width;
int csEngine::frame_height;
iSystem* csEngine::System = NULL;
csEngine* csEngine::current_engine = NULL;
iEngine* csEngine::current_iengine = NULL;
bool csEngine::use_new_radiosity = false;
int csEngine::max_process_polygons = 2000000000;
int csEngine::cur_process_polygons = 0;
bool csEngine::do_lighting_cache = true;
bool csEngine::do_not_force_relight = false;
bool csEngine::do_force_relight = false;
bool csEngine::do_not_force_revis = false;
bool csEngine::do_force_revis = false;
bool csEngine::do_rad_debug = false;

SCF_IMPLEMENT_IBASE (csEngine)
  SCF_IMPLEMENTS_INTERFACE (iEngine)
  SCF_IMPLEMENTS_EMBEDDED_INTERFACE (iPlugin)
  SCF_IMPLEMENTS_EMBEDDED_INTERFACE (iConfig)
  SCF_IMPLEMENTS_EMBEDDED_INTERFACE (iObject)
SCF_IMPLEMENT_IBASE_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csEngine::eiPlugin)
  SCF_IMPLEMENTS_INTERFACE (iPlugin)
SCF_IMPLEMENT_EMBEDDED_IBASE_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csEngine::iObjectInterface)
  void *itf = csObject::QueryInterface (iInterfaceID, iVersion);
  if (itf) return itf;
SCF_IMPLEMENT_EMBEDDED_IBASE_END

SCF_IMPLEMENT_FACTORY (csEngine)

SCF_EXPORT_CLASS_TABLE (engine)
  SCF_EXPORT_CLASS_DEP (csEngine, "crystalspace.engine.3d",
    "Crystal Space 3D Engine",
      "crystalspace.kernel., "
      "crystalspace.graphics3d., "
      "crystalspace.graphic.image.io.")
SCF_EXPORT_CLASS_TABLE_END

csEngine::csEngine (iBase *iParent) : sectors (true), camera_positions (16, 16)
{
  SCF_CONSTRUCT_IBASE (iParent);
  SCF_CONSTRUCT_EMBEDDED_IBASE (scfiPlugin);
  SCF_CONSTRUCT_EMBEDDED_IBASE (scfiConfig);
  SCF_CONSTRUCT_EMBEDDED_IBASE (scfiObject);
  engine_mode = CS_ENGINE_AUTODETECT;
  first_dyn_lights = NULL;
  System = NULL;
  VFS = NULL;
  G3D = NULL;
  G2D = NULL;
  Reporter = NULL;
  ImageLoader = NULL;
  textures = NULL;
  materials = NULL;
  c_buffer = NULL;
  cbufcube = NULL;
  current_camera = NULL;
  current_engine = this;
  current_iengine = SCF_QUERY_INTERFACE (this, iEngine);
  current_iengine->DecRef ();
  use_pvs = false;
  use_pvs_only = false;
  freeze_pvs = false;
  engine_states = NULL;
  rad_debug = NULL;
  nextframe_pending = 0;

  cbufcube = new csCBufferCube (1024);
  InitCuller ();

  textures = new csTextureList ();
  materials = new csMaterialList ();

  render_pol2d_pool = new csPoly2DPool (csPolygon2DFactory::SharedFactory());
  lightpatch_pool = new csLightPatchPool ();

  BuildSqrtTable ();
  resize = false;

  thing_type = new csThingObjectType (NULL);
  ClearRenderPriorities ();
}

// @@@ Hack
iCamera* camera_hack = NULL;

csEngine::~csEngine ()
{
  DeleteAll ();
  if (G3D) G3D->DecRef ();
  if (ImageLoader) ImageLoader->DecRef();
  if (VFS) VFS->DecRef ();
  if (Reporter) Reporter->DecRef ();
  delete materials;
  delete textures;
  delete render_pol2d_pool;
  delete lightpatch_pool;
  delete cbufcube;
  delete rad_debug;
  delete c_buffer;
  
  // @@@ temp hack
  if (camera_hack)
    camera_hack->DecRef ();
  camera_hack = NULL;
}

bool csEngine::Initialize (iSystem* sys)
{
#if defined(JORRIT_DEBUG)
  printf ("csPolygon3D %ld\n", (long)sizeof (csPolygon3D));
  printf ("csPolyPlane %ld\n", (long)sizeof (csPolyPlane));
  printf ("csPolyTxtPlane %ld\n", (long)sizeof (csPolyTxtPlane));
  printf ("csPolyTexture %ld\n", (long)sizeof (csPolyTexture));
  printf ("csPolygonSet %ld\n", (long)sizeof (csPolygonSet));
  printf ("csLightMapped %ld\n", (long)sizeof (csLightMapped));
  printf ("csGouraudShaded %ld\n", (long)sizeof (csGouraudShaded));
  printf ("csLightMap %ld\n", (long)sizeof (csLightMap));
#endif

  System = sys;

  if (!(G3D = CS_QUERY_PLUGIN_ID (sys, CS_FUNCID_VIDEO, iGraphics3D)))
    return false;

  if (!(VFS = CS_QUERY_PLUGIN_ID (sys, CS_FUNCID_VFS, iVFS)))
    return false;

  G2D = G3D->GetDriver2D ();

  // don't check for failure; the engine can work without the image loader
  ImageLoader = CS_QUERY_PLUGIN_ID (sys, CS_FUNCID_IMGLOADER, iImageIO);
  if (!ImageLoader)
    Warn ("No image loader. Loading images will fail.");

  Reporter = CS_QUERY_PLUGIN_ID (sys, CS_FUNCID_REPORTER, iReporter);

  // Tell system driver that we want to handle broadcast events
  if (!System->CallOnEvents (&scfiPlugin, CSMASK_Broadcast))
    return false;
  
  csConfigAccess cfg(System, "/config/engine.cfg");
  ReadConfig (cfg);

  thing_type->Initialize (sys);

  return true;
}

// Handle some system-driver broadcasts
bool csEngine::HandleEvent (iEvent &Event)
{
  if (Event.Type == csevBroadcast)
    switch (Event.Command.Code)
    {
      case cscmdSystemOpen:
      {
        csGraphics3DCaps *caps = G3D->GetCaps ();
        fogmethod = caps->fog;
        NeedPO2Maps = caps->NeedsPO2Maps;
        MaxAspectRatio = caps->MaxAspectRatio;

        frame_width = G3D->GetWidth ();
        frame_height = G3D->GetHeight ();
        if (csCamera::GetDefaultFOV () == 0)
          csCamera::SetDefaultFOV (frame_height, frame_width);

        // @@@ Ugly hack to always have a camera in current_camera.
        // This is needed for the lighting routines.
        if (!current_camera)
        {
          current_camera = &(new csCamera ())->scfiCamera;
          camera_hack = current_camera;
        }

        // Allow context resizing since we handle cscmdContextResize
        G2D->AllowCanvasResize (true);

        StartEngine ();

        return true;
      }
      case cscmdSystemClose:
      {
        // We must free all material and texture handles since after
        // G3D->Close() they all become invalid, no matter whenever
        // we did or didn't an IncRef on them.
        DeleteAll ();
        return true;
      }
      case cscmdContextResize:
      {
	if (engine_states)
	  engine_states->Resize ((iGraphics2D*) Event.Command.Info);
	else
	  if (((iGraphics2D*) Event.Command.Info) == G2D)
	    resize = true;
	return false;
      }
      case cscmdContextClose:
      {
	if (engine_states)
	{
	  engine_states->Close ((iGraphics2D*) Event.Command.Info);
	  if (!engine_states->Length())
	  {
	    delete engine_states;
	    engine_states = NULL;
	  }
	}
	return false;
      }
    } /* endswitch */

  return false;
}

void csEngine::DeleteAll ()
{
  nextframe_pending = 0;
  if (G3D) G3D->ClearCache ();
  halos.DeleteAll ();
  int i;
  for (i = collections.Length () -1; i >= 0 ; i--)
    RemoveCollection ((csCollection*)collections.Get (i));

  for (i = meshes.Length () -1; i >= 0 ; i--)
    RemoveMesh ((csMeshWrapper*)meshes.Get (i));

  mesh_factories.DeleteAll ();
  curve_templates.DeleteAll ();

  for (i = 0 ; i < sectors.Length () ; i++)
  {
    csSector* sect = sectors[i]->GetPrivateObject ();
    if (sect) sect->CleanupReferences ();
  }
  sectors.DeleteAll ();

  camera_positions.DeleteAll ();

  for (i = 0 ; i < planes.Length () ; i++)
  {
    csPolyTxtPlane* p = (csPolyTxtPlane*)planes[i];
    planes[i] = NULL;
    p->DecRef ();
  }
  planes.DeleteAll ();

  while (first_dyn_lights)
  {
    csDynLight* dyn = first_dyn_lights->GetNext ();
    delete first_dyn_lights;
    first_dyn_lights = dyn;
  }
  delete materials; 
  materials = new csMaterialList ();
  delete textures; 
  textures = new csTextureList ();

  // Delete engine states and their references to cullers before cullers are
  // deleted in InitCuller below.
  if (engine_states)
  {
    engine_states->DeleteAll ();
    delete engine_states;
    engine_states = NULL;
  }

  InitCuller ();
  delete render_pol2d_pool;
  render_pol2d_pool = new csPoly2DPool (csPolygon2DFactory::SharedFactory());
  delete lightpatch_pool;
  lightpatch_pool = new csLightPatchPool ();
  use_pvs = false;

  // Clear all regions.
  region = NULL;
  regions.DeleteAll ();

  // Clear all render priorities.
  ClearRenderPriorities ();
  
  // remove objects
  QueryObject ()->ObjRemoveAll ();
}

iObject *csEngine::QueryObject ()
{
  return &scfiObject;
}

void csEngine::RegisterRenderPriority (const char* name, long priority)
{
  int i;
  // If our priority goes over the number of defined priorities
  // then we have to initialize.
  for (i = render_priorities.Length () ; i <= priority ; i++)
    render_priorities[i] = NULL;

  char* n = (char*)render_priorities[priority];
  delete[] n;
  n = csStrNew (name);
  render_priorities[priority] = n;
  if (!strcmp (name, "sky")) render_priority_sky = priority;
  else if (!strcmp (name, "wall")) render_priority_wall = priority;
  else if (!strcmp (name, "object")) render_priority_object = priority;
  else if (!strcmp (name, "alpha")) render_priority_alpha = priority;
}

long csEngine::GetRenderPriority (const char* name) const
{
  int i;
  for (i = 0 ; i < render_priorities.Length () ; i++)
  {
    char* n = (char*)render_priorities[i];
    if (n && !strcmp (name, n)) return i;
  }
  return 0;
}

void csEngine::ClearRenderPriorities ()
{
  int i;
  for (i = 0 ; i < render_priorities.Length () ; i++)
  {
    char* n = (char*)render_priorities[i];
    delete[] n;
  }
  render_priorities.DeleteAll ();
  RegisterRenderPriority ("sky", 2);
  RegisterRenderPriority ("wall", 4);
  RegisterRenderPriority ("object", 6);
  RegisterRenderPriority ("alpha", 8);
}

void csEngine::ResolveEngineMode ()
{
  if (engine_mode == CS_ENGINE_AUTODETECT)
  {
    // Here we do some heuristic. We scan all sectors and see if there
    // are some that have big octrees. If so we select CS_ENGINE_FRONT2BACK.
    // If not we select CS_ENGINE_BACK2FRONT.
    // @@@ It might be an interesting option to try to find a good engine
    // mode for every sector and switch dynamically based on the current
    // sector (only switch based on the position of the camera, not switch
    // based on the sector we are currently rendering).
    int switch_f2b = 0;
    for (int i = 0 ; i < sectors.Length () ; i++)
    {
      csSector* s = sectors[i]->GetPrivateObject ();
      csMeshWrapper* ss = s->GetCullerMesh ();
      if (ss)
      {
#if 0
// @@@ Disabled because we can't get at the tree now.
        csPolygonTree* ptree = ss->GetStaticTree ();
        csOctree* otree = (csOctree*)ptree;
	int num_nodes = otree->GetRoot ()->CountChildren ();
	if (num_nodes > 30) switch_f2b += 10;
	else if (num_nodes > 15) switch_f2b += 5;
#else
	switch_f2b += 10;
#endif
      }
      if (switch_f2b >= 10) break;
    }
    if (switch_f2b >= 10)
    {
      engine_mode = CS_ENGINE_FRONT2BACK;
      Report ("Engine is using front2back mode.");
    }
    else
    {
      engine_mode = CS_ENGINE_BACK2FRONT;
      Report ("Engine is using back2front mode.");
    }
  }
}

void csEngine::EnableLightingCache (bool en)
{
  do_lighting_cache = en;
  if (!do_lighting_cache) do_force_relight = true;
  else do_force_relight = false;
}

int csEngine::GetLightmapCellSize () const
{
  return csLightMap::lightcell_size;
}

void csEngine::SetLightmapCellSize (int Size)
{
  csLightMap::lightcell_size = Size;
}

void csEngine::InitCuller ()
{
  delete c_buffer; c_buffer = NULL;
  c_buffer = new csCBuffer (0, frame_width-1, frame_height);
}

void csEngine::PrepareTextures ()
{
  int i;

  iTextureManager* txtmgr = G3D->GetTextureManager ();
  txtmgr->ResetPalette ();

  // First register all textures to the texture manager.
  for (i = 0; i < textures->Length (); i++)
  {
    iTextureWrapper *csth = textures->Get (i);
    if (!csth->GetTextureHandle ())
      csth->Register (txtmgr);
  }

  // Prepare all the textures.
  txtmgr->PrepareTextures ();

  // Then register all materials to the texture manager.
  for (i = 0; i < materials->Length (); i++)
  {
    iMaterialWrapper *csmh = materials->Get (i);
    if (!csmh->GetMaterialHandle ())
      csmh->Register (txtmgr);
  }

  // Prepare all the materials.
  txtmgr->PrepareMaterials ();
}

// If a STAT_BSP level the loader call to UpdateMove is redundant when called
// before csEngine::Prepare().
void csEngine::PrepareMeshes ()
{
  int i;
  for (i = 0 ; i < meshes.Length () ; i++)
  {
    csMeshWrapper* sp = (csMeshWrapper*)meshes[i];
    sp->GetMovable ().UpdateMove ();
  }
}

bool csEngine::Prepare ()
{
  PrepareTextures ();
  PrepareMeshes ();
  // The images are no longer needed by the 3D engine.
  iTextureManager *txtmgr = G3D->GetTextureManager ();
  txtmgr->FreeImages ();

  G3D->ClearCache ();

  // Prepare lightmaps if we have any sectors
  if (sectors.Length ())
    ShineLights ();

  CheckConsistency ();

  return true;
}

void csEngine::ShineLights (csRegion* region)
{
  if (!do_not_force_relight)
  {
    // If recalculation is not forced then we check if the 'precalc_info'
    // file exists on the VFS. If not then we will recalculate in any case.
    // If the file exists but is not valid (some flags are different) then
    // we recalculate again.
    // If we recalculate then we also update this 'precalc_info' file with
    // the new settings.
    struct PrecalcInfo
    {
      int lm_version;           // This number identifies a version of the lightmap format.
                                // If different then the format is different and we need
                                // to recalculate.
      int normal_light_level;   // Normal light level (unlighted level).
      int ambient_red;
      int ambient_green;
      int ambient_blue;
      int reflect;
      int radiosity;
      float cosinus_factor;
      int lightmap_size;
    };
    PrecalcInfo current;
    memset (&current, 0, sizeof (current));
    current.lm_version = 1;
    current.normal_light_level = NORMAL_LIGHT_LEVEL;
    current.ambient_red = csLight::ambient_red;
    current.ambient_green = csLight::ambient_green;
    current.ambient_blue = csLight::ambient_blue;
    current.reflect = csSector::cfg_reflections;
    current.radiosity = (int)csSector::do_radiosity;
    current.cosinus_factor = csPolyTexture::cfg_cosinus_factor;
    current.lightmap_size = csLightMap::lightcell_size;
    char *reason = NULL;

    iDataBuffer *data = VFS->ReadFile ("precalc_info");
    if (!data)
      reason = "no 'precalc_info' found";
    else
    {
      char *input = **data;
      while (*input)
      {
        char *keyword = input + strspn (input, " \t");
        char *endkw = strchr (input, '=');
        if (!endkw) break;
        *endkw++ = 0;
        input = strchr (endkw, '\n');
        if (input) *input++ = 0;
        float xf;
        sscanf (endkw, "%f", &xf);
        int xi = QRound (xf);

#define CHECK(keyw,cond,reas) \
        else if (!strcmp (keyword, keyw)) \
        { if (cond) { reason = reas " changed"; break; } }

        if (false) {}
        CHECK ("LMVERSION", xi != current.lm_version, "lightmap format")
        CHECK ("LIGHTMAP_SIZE", xi != current.lightmap_size, "lightmap size")

#undef CHECK
      }
    }
    if (data) data->DecRef ();

    if (reason)
    {
      char data [500];
      sprintf (data,
        "LMVERSION=%d\n"
        "NORMALLIGHTLEVEL=%d\n"
        "AMBIENT_RED=%d\n"
        "AMBIENT_GREEN=%d\n"
        "AMBIENT_BLUE=%d\n"
        "REFLECT=%d\n"
        "RADIOSITY=%d\n"
        "COSINUS_FACTOR=%g\n"
        "LIGHTMAP_SIZE=%d\n",
        current.lm_version, current.normal_light_level, current.ambient_red,
        current.ambient_green, current.ambient_blue, current.reflect,
        current.radiosity, current.cosinus_factor,
        current.lightmap_size);
      VFS->WriteFile ("precalc_info", data, strlen (data));
      Report ("Lightmap data is not up to date (reason: %s).", reason);
      do_force_relight = true;
    }
  }

  if (do_force_relight)
  {
    Report ("Recalculation of lightmaps forced.");
    Report ("  Pseudo-radiosity system %s.",
	csSector::do_radiosity ? "enabled" : "disabled");
    Report ("  Maximum number of visits per sector = %d.",
	csSector::cfg_reflections);
  }
  else
  {
    // If no recalculation is forced we set these variables to default to
    // make sure that we don't do too many unneeded calculations.
    csSector::do_radiosity = false;
    csSector::cfg_reflections = 1;
  }

  csLightIt* lit = NewLightIterator (region);

  // Count number of lights to process.
  csLight* l;
  int light_count = 0;
  lit->Restart ();
  while (lit->Fetch ()) light_count++;

  bool do_relight = false;
  if (do_force_relight && IsLightingCacheEnabled ())
    do_relight = true;

  int sn = 0;
  int num_meshes = meshes.Length ();
  csProgressMeter meter (System);

  if (do_relight)
  {
    Report ("Initializing lighting (%d meshes).", num_meshes);
    meter.SetTotal (num_meshes);
    meter.Restart ();
  }
  for (sn = 0; sn < num_meshes ; sn++)
  {
    csMeshWrapper* s = (csMeshWrapper*)meshes[sn];
    if (!region || region->IsInRegion (s))
    {
      iLightingInfo* linfo = SCF_QUERY_INTERFACE (s->GetMeshObject (),
      	iLightingInfo);
      if (linfo)
      {
        if (do_force_relight || !IsLightingCacheEnabled ())
          linfo->InitializeDefault ();
	else
	  linfo->ReadFromCache (0);	// ID?
	linfo->DecRef ();
      }
    }
    if (do_relight) meter.Step();
  }

  csTime start, stop;
  start = System->GetTime ();
  if (do_relight)
  {
    Report ("\nShining lights (%d lights).", light_count);
    meter.SetTotal (light_count);
    meter.Restart ();
  }
  lit->Restart ();
  while ((l = lit->Fetch ()) != NULL)
  {
    ((csStatLight*)l)->CalculateLighting ();
    if (do_relight) meter.Step();
  }
  stop = System->GetTime ();
  if (do_relight)
    Report ("\nTime taken: %.4f seconds.", (float)(stop-start)/1000.);

  // Render radiosity
  if (use_new_radiosity && !do_not_force_relight && do_force_relight)
  {
    start = System->GetTime ();
    csRadiosity *rad = new csRadiosity(this);
    if (do_rad_debug)
    {
      rad_debug = rad;
    }
    else
    {
      rad->DoRadiosity();
      delete rad;
    }
    stop = System->GetTime ();
    if (do_relight)
      Report ("\nTime taken: %.4f seconds.", (float)(stop-start)/1000.);
  }

  if (do_relight)
  {
    Report ("Caching lighting (%d meshes).", num_meshes);
    meter.SetTotal (num_meshes);
    meter.Restart ();
  }
  for (sn = 0; sn < num_meshes ; sn++)
  {
    csMeshWrapper* s = (csMeshWrapper*)meshes[sn];
    if (!region || region->IsInRegion (s))
    {
      iLightingInfo* linfo = SCF_QUERY_INTERFACE (s->GetMeshObject (),
      	iLightingInfo);
      if (linfo)
      {
        if (do_relight)
          linfo->WriteToCache (0);	// @@@ id
	linfo->PrepareLighting ();
        linfo->DecRef ();
      }
    }
    if (do_relight) meter.Step();
  }

  csThing::current_light_frame_number++;

  if (do_relight)
    Report ("\nUpdating VFS....");
  if (!VFS->Sync ())
    Warn ("Error updating lighttable cache!\n");
  if (do_relight)
    Report ("DONE!");

  delete lit;
}

void csEngine::InvalidateLightmaps ()
{
#if 0
//@@@@ TODO!!
  csPolyIt* pit = NewPolyIterator ();
  csPolygon3D* p;
  while ((p = pit->Fetch ()) != NULL)
  {
    csPolyTexLightMap* lmi = p->GetLightMapInfo ();
    if (lmi && lmi->GetLightMap ())
      ((csLightMap*)lmi->GetLightMap ())->MakeDirtyDynamicLights ();
  }
  delete pit;
#endif
}

bool csEngine::CheckConsistency ()
{
  return false;
}

void csEngine::StartEngine ()
{
  DeleteAll ();
}

void csEngine::StartDraw (iCamera* c, iClipper2D* view, csRenderView& rview)
{
  Stats::polygons_considered = 0;
  Stats::polygons_drawn = 0;
  Stats::portals_drawn = 0;
  Stats::polygons_rejected = 0;
  Stats::polygons_accepted = 0;

  // Make sure we select an engine mode.
  ResolveEngineMode ();

  current_camera = c;
  rview.SetEngine (this);

  // This flag is set in HandleEvent on a cscmdContextResize event
  if (resize)
  {
    resize = false;
    Resize ();
  }

  top_clipper = view;

  rview.GetClipPlane ().Set (0, 0, 1, -1);   //@@@CHECK!!!

  // Calculate frustum for screen dimensions (at z=1).
  float leftx = - c->GetShiftX () * c->GetInvFOV ();
  float rightx = (frame_width - c->GetShiftX ()) * c->GetInvFOV ();
  float topy = - c->GetShiftY () * c->GetInvFOV ();
  float boty = (frame_height - c->GetShiftY ()) * c->GetInvFOV ();
  rview.SetFrustum (leftx, rightx, topy, boty);

  // Initialize the 2D/3D culler.
  if (engine_mode == CS_ENGINE_FRONT2BACK)
  {
    if (c_buffer)
    {
      c_buffer->Initialize ();
      c_buffer->InsertPolygon (view->GetClipPoly (), view->GetVertexCount (),
      	true);
    }
  }

  cur_process_polygons = 0;
}

void csEngine::Draw (iCamera* c, iClipper2D* view)
{
  ControlMeshes ();

  csRenderView rview (c, view, G3D, G2D);
  StartDraw (c, view, rview);
  rview.SetCallback (NULL);

  // First initialize G3D with the right clipper.
  G3D->SetClipper (view, CS_CLIPPER_TOPLEVEL);	// We are at top-level.
  G3D->ResetNearPlane ();
  G3D->SetPerspectiveAspect (c->GetFOV ());
  
  iSector* s = c->GetSector ();
  s->Draw (&rview);

  // draw all halos on the screen
  csTime Elapsed, Current;
  System->GetElapsedTime (Elapsed, Current);
  for (int halo = halos.Length () - 1; halo >= 0; halo--)
    if (!halos.Get (halo)->Process (Elapsed, *this))
      halos.Delete (halo);

  G3D->SetClipper (NULL, CS_CLIPPER_NONE);
}

void csEngine::DrawFunc (iCamera* c, iClipper2D* view,
  iDrawFuncCallback* callback)
{
  ControlMeshes ();

  csRenderView rview (c, view, G3D, G2D);
  StartDraw (c, view, rview);
  rview.SetCallback (callback);

  // First initialize G3D with the right clipper.
  G3D->SetClipper (view, CS_CLIPPER_TOPLEVEL);	// We are at top-level.
  G3D->ResetNearPlane ();
  G3D->SetPerspectiveAspect (c->GetFOV ());

  iSector* s = c->GetSector ();
  s->Draw (&rview);

  G3D->SetClipper (NULL, CS_CLIPPER_NONE);
}

void csEngine::AddHalo (csLight* Light)
{
  if (!Light->GetHalo () || Light->flags.Check (CS_LIGHT_ACTIVEHALO))
    return;

  // Transform light pos into camera space and see if it is directly visible
  csVector3 v = current_camera->GetTransform ().Other2This (Light->GetCenter ());

  // Check if light is behind us
  if (v.z <= SMALL_Z)
    return;

  // Project X,Y into screen plane
  float iz = current_camera->GetFOV () / v.z;
  v.x = v.x * iz + current_camera->GetShiftX ();
  v.y = frame_height - 1 - (v.y * iz + current_camera->GetShiftY ());

  // If halo is not inside visible region, return
  if (!top_clipper->IsInside (csVector2 (v.x, v.y)))
    return;

  // Check if light is not obscured by anything
  float zv = G3D->GetZBuffValue (QRound (v.x), QRound (v.y));
  if (v.z > zv)
    return;

  // Halo size is 1/4 of the screen height; also we make sure its odd
  int hs = (frame_height / 4) | 1;

  if (Light->GetHalo()->Type == cshtFlare)
  {
    // put a new light flare into the queue
    // the cast is safe because of the type check above
    halos.Push (new csLightFlareHalo (Light, (csFlareHalo*)Light->GetHalo(), 
      hs));
    return;
  }

  // Okay, put the light into the queue: first we generate the alphamap
  unsigned char *Alpha = Light->GetHalo ()->Generate (hs);
  iHalo *handle = G3D->CreateHalo (Light->GetColor ().red,
    Light->GetColor ().green, Light->GetColor ().blue, Alpha, hs, hs);

  // We don't need alpha map anymore
  delete [] Alpha;

  // Does 3D rasterizer support halos?
  if (!handle)
    return;

  halos.Push (new csLightHalo (Light, handle));
}

void csEngine::RemoveHalo (csLight* Light)
{
  int idx = halos.FindKey (Light);
  if(idx!=-1)
    halos.Delete (idx);
}

csStatLight* csEngine::FindCsLight (float x, float y, float z, float dist)
  const
{
  csStatLight* l;
  int sn = sectors.Length ();
  while (sn > 0)
  {
    sn--;
    csSector* s = sectors[sn]->GetPrivateObject ();
    l = s->FindLight (x, y, z, dist);
    if (l) return l;
  }
  return NULL;
}

csStatLight* csEngine::FindCsLight (unsigned long light_id) const
{
  csStatLight* l;
  int sn = sectors.Length ();
  while (sn > 0)
  {
    sn--;
    csSector* s = sectors[sn]->GetPrivateObject ();
    l = s->FindLight (light_id);
    if (l) return l;
  }
  return NULL;
}

csStatLight* csEngine::FindCsLight (const char* name, bool /*regionOnly*/) const
{
  //@@@### regionOnly
  csStatLight* l;
  int sn = sectors.Length ();
  while (sn > 0)
  {
    sn--;
    csSector* s = sectors[sn]->GetPrivateObject ();
    l = s->GetLight (name);
    if (l) return l;
  }
  return NULL;
}

iStatLight* csEngine::FindLight (const char* name, bool regionOnly) const
{
  return &FindCsLight(name, regionOnly)->scfiStatLight;
}

void csEngine::AddDynLight (csDynLight* dyn)
{
  dyn->SetNext (first_dyn_lights);
  dyn->SetPrev (NULL);
  if (first_dyn_lights) first_dyn_lights->SetPrev (dyn);
  first_dyn_lights = dyn;
}

void csEngine::RemoveDynLight (csDynLight* dyn)
{
  if (dyn->GetNext ()) dyn->GetNext ()->SetPrev (dyn->GetPrev ());
  if (dyn->GetPrev ()) dyn->GetPrev ()->SetNext (dyn->GetNext ());
  else if (dyn == first_dyn_lights) first_dyn_lights = dyn->GetNext ();
  dyn->SetNext (NULL);
  dyn->SetPrev (NULL);
}

void csEngine::ControlMeshes ()
{
  csTime elapsed_time, current_time;
  System->GetElapsedTime (elapsed_time, current_time);

  nextframe_pending = current_time;

  // Delete particle systems that self-destructed now.
  // @@@ Need to optimize this?
  int i = meshes.Length ()-1;
  while (i >= 0)
  {
    csMeshWrapper* sp = (csMeshWrapper*)meshes[i];
    if (sp->WantToDie ())
      RemoveMesh (sp);
    i--;
  }
}

void csEngine::ReadConfig (iConfigFile* Config)
{
  csLightMap::SetLightCellSize (Config->GetInt ("Engine.Lighting.LightmapSize", 16));

  csLight::ambient_red = Config->GetInt ("Engine.Lighting.Ambient.Red", DEFAULT_LIGHT_LEVEL);
  csLight::ambient_green = Config->GetInt ("Engine.Lighting.Ambient.Green", DEFAULT_LIGHT_LEVEL);
  csLight::ambient_blue = Config->GetInt ("Engine.Lighting.Ambient.Blue", DEFAULT_LIGHT_LEVEL);
  int ambient_white = Config->GetInt ("Engine.Lighting.Ambient.White", DEFAULT_LIGHT_LEVEL);
  csLight::ambient_red += ambient_white;
  csLight::ambient_green += ambient_white;
  csLight::ambient_blue += ambient_white;

  // Do not allow too black environments since software renderer hates it
  if (csLight::ambient_red + csLight::ambient_green + csLight::ambient_blue < 6)
    csLight::ambient_red = csLight::ambient_green = csLight::ambient_blue = 2;

  csSector::cfg_reflections = Config->GetInt ("Engine.Lighting.Reflections", csSector::cfg_reflections);
  csPolyTexture::cfg_cosinus_factor = Config->GetFloat ("Engine.Lighting.CosinusFactor", csPolyTexture::cfg_cosinus_factor);
  //@@@ NOT THE RIGHT PLACE! csSprite3D::global_lighting_quality = Config->GetInt ("Engine.Lighting.SpriteQuality", csSprite3D::global_lighting_quality);
  csSector::do_radiosity = Config->GetBool ("Engine.Lighting.Radiosity", csSector::do_radiosity);

  // radiosity options
  csEngine::use_new_radiosity = Config->GetBool
        ("Engine.Lighting.Radiosity.Enable", csEngine::use_new_radiosity);
  csRadiosity::do_static_specular = Config->GetBool
        ("Engine.Lighting.Radiosity.DoStaticSpecular", csRadiosity::do_static_specular);
  csRadiosity::static_specular_amount = Config->GetFloat
        ("Engine.Lighting.Radiosity.StaticSpecularAmount", csRadiosity::static_specular_amount);
  csRadiosity::static_specular_tightness = Config->GetInt
        ("Engine.Lighting.Radiosity.StaticSpecularTightness", csRadiosity::static_specular_tightness);
  csRadiosity::colour_bleed = Config->GetFloat
        ("Engine.Lighting.Radiosity.ColourBleed", csRadiosity::colour_bleed);
  csRadiosity::stop_priority = Config->GetFloat
        ("Engine.Lighting.Radiosity.StopPriority", csRadiosity::stop_priority);
  csRadiosity::stop_improvement = Config->GetFloat
        ("Engine.Lighting.Radiosity.StopImprovement", csRadiosity::stop_improvement);
  csRadiosity::stop_iterations = Config->GetInt
        ("Engine.Lighting.Radiosity.StopIterations", csRadiosity::stop_iterations);
  csRadiosity::source_patch_size = Config->GetInt
        ("Engine.Lighting.Radiosity.SourcePatchSize", csRadiosity::source_patch_size);
}

void csEngine::RemoveMesh (csMeshWrapper* mesh)
{
  mesh->GetMovable ().ClearSectors ();
  int idx = meshes.Find (mesh);
  if (idx == -1) return;
  meshes[idx] = NULL;
  meshes.Delete (idx);
  mesh->DecRef ();
}

void csEngine::RemoveMesh (iMeshWrapper* mesh)
{
  RemoveMesh (mesh->GetPrivateObject ());
}

void csEngine::RemoveCollection (csCollection* collection)
{
  collection->GetMovable ().ClearSectors ();
  int idx = collections.Find (collection);
  if (idx == -1) return;
  collections.Delete (idx);
}

struct LightAndDist
{
  iLight* light;
  float sqdist;
};

// csLightArray is a subclass of csCleanable which is registered
// to csEngine.cleanup.
class csLightArray : public iBase
{
public:
  SCF_DECLARE_IBASE;
  
  LightAndDist* array;
  // Size is the physical size of the array. num_lights is the number of lights in it.
  int size, num_lights;

  csLightArray () : array (NULL), size (0), num_lights (0) { SCF_CONSTRUCT_IBASE(NULL); }
  virtual ~csLightArray () { delete [] array; }
  void Reset () { num_lights = 0; }
  void AddLight (iLight* l, float sqdist)
  {
    if (num_lights >= size)
    {
      LightAndDist* new_array;
      new_array = new LightAndDist [size+5];
      if (array)
      {
        memcpy (new_array, array, sizeof (LightAndDist)*num_lights);
        delete [] array;
      }
      array = new_array;
      size += 5;
    }
    array[num_lights].light = l;
    array[num_lights++].sqdist = sqdist;
  };
  iLight* GetLight (int i) { return array[i].light; }
};

SCF_IMPLEMENT_IBASE(csLightArray)
 SCF_IMPLEMENTS_INTERFACE (iBase)
SCF_IMPLEMENT_IBASE_END;

int compare_light (const void* p1, const void* p2)
{
  LightAndDist* sp1 = (LightAndDist*)p1;
  LightAndDist* sp2 = (LightAndDist*)p2;
  float z1 = sp1->sqdist;
  float z2 = sp2->sqdist;
  if (z1 < z2) return -1;
  else if (z1 > z2) return 1;
  return 0;
}

int csEngine::GetNearbyLights (csSector* sector, const csVector3& pos, ULong flags,
  	iLight** lights, int max_num_lights)
{
  int i;
  float sqdist;

  // This is a static light array which is adapted to the
  // right size everytime it is used. In the beginning it means
  // that this array will grow a lot but finally it will
  // stabilize to a maximum size (not big). The advantage of
  // this approach is that we don't have a static array which can
  // overflow. And we don't have to do allocation every time we
  // come here. We register this memory to the 'cleanup' array
  // in csEngine so that it will be freed later.

  static csLightArray* light_array = NULL;
  if (!light_array)
  {
    light_array = new csLightArray ();
    csEngine::current_engine->cleanup.Push (light_array);
  }
  light_array->Reset ();

  // Add all dynamic lights to the array (if CS_NLIGHT_DYNAMIC is set).
  if (flags & CS_NLIGHT_DYNAMIC)
  {
    csDynLight* dl = first_dyn_lights;
    while (dl)
    {
      if (dl->GetSector () == sector)
      {
        sqdist = csSquaredDist::PointPoint (pos, dl->GetCenter ());
        if (sqdist < dl->GetSquaredRadius ())
	{
	  iLight* il = SCF_QUERY_INTERFACE (dl, iLight);
	  light_array->AddLight (il, sqdist);
	  il->DecRef ();
	}
      }
      dl = dl->GetNext ();
    }
  }

  // Add all static lights to the array (if CS_NLIGHT_STATIC is set).
  if (flags & CS_NLIGHT_STATIC)
  {
    for (i = 0 ; i < sector->GetLightCount () ; i++)
    {
      csStatLight* sl = sector->GetLight (i);
      sqdist = csSquaredDist::PointPoint (pos, sl->GetCenter ());
      if (sqdist < sl->GetSquaredRadius ())
      {
	iLight* il = SCF_QUERY_INTERFACE (sl, iLight);
        light_array->AddLight (il, sqdist);
	il->DecRef ();
      }
    }
  }

  if (light_array->num_lights <= max_num_lights)
  {
    // The number of lights that we found is smaller than what fits
    // in the array given us by the user. So we just copy them all
    // and don't need to sort.
    for (i = 0 ; i < light_array->num_lights ; i++)
      lights[i] = light_array->GetLight (i);
    return light_array->num_lights;
  }
  else
  {
    // We found more lights than we can put in the given array
    // so we sort the lights and then return the nearest.
    qsort (light_array->array, light_array->num_lights, sizeof (LightAndDist), compare_light);
    for (i = 0 ; i < max_num_lights; i++)
      lights[i] = light_array->GetLight (i);
    return max_num_lights;
  }
}

csSectorIt* csEngine::GetNearbySectors (csSector* sector,
	const csVector3& pos, float radius)
{
  csSectorIt* it = new csSectorIt (sector, pos, radius);
  return it;
}

csObjectIt* csEngine::GetNearbyObjects (csSector* sector,
  const csVector3& pos, float radius)
{
  csObjectIt* it = new csObjectIt (this, sector, pos, radius);
  return it;
}

int csEngine::GetTextureFormat () const
{
  iTextureManager* txtmgr = G3D->GetTextureManager ();
  return txtmgr->GetTextureFormat ();
}

void csEngine::SelectRegion (const char *iName)
{
  if (iName == NULL)
  {
    region = NULL; // Default engine region.
    return;
  }

  region = (csRegion*)regions.FindByName (iName);
  if (!region)
  {
    region = new csRegion (this);
    region->SetName (iName);
    regions.Push (region);
  }
}

void csEngine::SelectRegion (iRegion* region)
{
  if (!region) { csEngine::region = NULL; return; }
  csEngine::region = (csRegion*)(region->GetPrivateObject ());
}

iRegion* csEngine::GetCurrentRegion () const
{
  return region ? &region->scfiRegion : NULL;
}

iRegion* csEngine::FindRegion (const char *name) const
{
  csRegion *r = (csRegion*)regions.FindByName (name);
  return r ? &r->scfiRegion : NULL;
}

void csEngine::AddToCurrentRegion (csObject* obj)
{
  if (region)
  {
    region->AddToRegion (obj);
  }
}

iTextureWrapper* csEngine::CreateTexture (const char *iName, const char *iFileName,
  csColor *iTransp, int iFlags)
{
  if (!ImageLoader)
    return NULL;

  // First of all, load the image file
  iDataBuffer *data = VFS->ReadFile (iFileName);
  if (!data || !data->GetSize ())
  {
    if (data) data->DecRef ();
    Warn ("Cannot read image file \"%s\" from VFS.", iFileName);
    return NULL;
  }

  iImage *ifile = ImageLoader->Load (data->GetUint8 (), data->GetSize (),
    CS_IMGFMT_TRUECOLOR);// GetTextureFormat ());
  data->DecRef ();

  if (!ifile)
  {
    Warn ("Unknown image file format: \"%s\".", iFileName);
    return NULL;
  }

  iDataBuffer *xname = VFS->ExpandPath (iFileName);
  ifile->SetName (**xname);
  xname->DecRef ();

  // Okay, now create the respective texture handle object
  iTextureWrapper *tex = GetTextures ()->FindByName (iName);
  if (tex)
    tex->SetImageFile (ifile);
  else
    tex = GetTextures ()->NewTexture (ifile);

  tex->SetFlags (iFlags);
  tex->QueryObject ()->SetName (iName);

  // dereference image pointer since tex already incremented it
  ifile->DecRef ();

  if (iTransp)
    tex->SetKeyColor (QInt (iTransp->red * 255.2),
      QInt (iTransp->green * 255.2), QInt (iTransp->blue * 255.2));

  return tex;
}

iMaterialWrapper* csEngine::CreateMaterial (const char *iName, iTextureWrapper* texture)
{
  csMaterial* mat = new csMaterial (texture);
  iMaterialWrapper* wrapper = materials->NewMaterial (mat);
  wrapper->QueryObject ()->SetName (iName);
  return wrapper;
}

iCameraPosition *csEngine::CreateCameraPosition (const char *iName, const char *iSector,
  const csVector3 &iPos, const csVector3 &iForward, const csVector3 &iUpward)
{
  csCameraPosition *cp = (csCameraPosition *)camera_positions.FindByName (iName);
  if (cp)
    cp->Set (iSector, iPos, iForward, iUpward);
  else
  {
    cp = new csCameraPosition (iName, iSector, iPos, iForward, iUpward);
    camera_positions.Push (cp);
  }

  return &(cp->scfiCameraPosition);
}

bool csEngine::CreatePlane (const char *iName, const csVector3 &iOrigin,
  const csMatrix3 &iMatrix)
{
  csPolyTxtPlane *ppl = new csPolyTxtPlane ();
  ppl->SetName (iName);
  ppl->SetTextureSpace (iMatrix, iOrigin);
  planes.Push (ppl);
  return true;
}

iMeshWrapper* csEngine::CreateSectorWallsMesh (csSector* sector,
	const char *iName)
{
  iMeshObjectType* thing_type = GetThingType ();
  iMeshObjectFactory* thing_fact = thing_type->NewFactory ();
  iMeshObject* thing_obj = SCF_QUERY_INTERFACE (thing_fact, iMeshObject);
  thing_fact->DecRef ();

  csMeshWrapper* thing_wrap = new csMeshWrapper (&scfiObject, thing_obj);

  thing_obj->DecRef ();
  thing_wrap->SetName (iName);
  meshes.Push (thing_wrap);
  thing_wrap->GetMovable ().SetSector (sector);
  thing_wrap->GetMovable ().UpdateMove ();
  thing_wrap->flags.Set (CS_ENTITY_CONVEX);
  thing_wrap->SetZBufMode (CS_ZBUF_FILL);
  thing_wrap->SetRenderPriority (GetWallRenderPriority ());

  iMeshWrapper* mesh_wrap = SCF_QUERY_INTERFACE (thing_wrap, iMeshWrapper);
  mesh_wrap->DecRef ();
  return mesh_wrap;
}

iMeshWrapper* csEngine::CreateSectorWallsMesh (iSector* sector,
	const char *iName)
{
  return CreateSectorWallsMesh (sector->GetPrivateObject (), iName);
}

iSector* csEngine::CreateSector (const char *iName, bool link)
{
  iSector* sector = &(new csSector (this))->scfiSector;
  sector->QueryObject ()->SetName (iName);
  if (link)
  {
    sectors.Push (sector);
    sector->DecRef ();
  }
  return sector;
}

csObject* csEngine::FindObjectInRegion (csRegion* region,
	const csNamedObjVector& vector, const char* name) const
{
  for (int i = vector.Length () - 1; i >= 0; i--)
  {
    csObject* o = (csObject *)vector[i];
    if (!region->IsInRegion (o)) continue;
    const char* oname = o->GetName ();
    if (name == oname || (name && oname && !strcmp (oname, name)))
      return o;
  }
  return NULL;
}

iObject* csEngine::FindObjectInRegion (csRegion* region,
	const csNamedObjectVector& vector, const char* name) const
{
  for (int i = vector.Length () - 1; i >= 0; i--)
  {
    iObject* o = vector.GetObject (i);
    if (!region->IsInRegion (o)) continue;
    const char* oname = o->GetName ();
    if (name == oname || (name && oname && !strcmp (oname, name)))
      return o;
  }
  return NULL;
}

iSector *csEngine::FindSector (const char *iName, bool regionOnly) const
{
  iSector* sec;
  if (regionOnly && region)
  {
    iObject* obj = FindObjectInRegion (region, sectors, iName);
    if (!obj) return NULL;
    sec = SCF_QUERY_INTERFACE_FAST (obj, iSector);
  }
  else
    sec = sectors.FindByName (iName);
  return sec;
}

iMeshWrapper *csEngine::FindMeshObject (const char *iName, bool regionOnly)
  const
{
  csMeshWrapper* mesh;
  if (regionOnly && region)
    mesh = (csMeshWrapper*)FindObjectInRegion (region, meshes, iName);
  else
    mesh = (csMeshWrapper*)meshes.FindByName (iName);
  if (!mesh) return NULL;
  iMeshWrapper* imesh = SCF_QUERY_INTERFACE (mesh, iMeshWrapper);
  imesh->DecRef ();
  return imesh;
}

iMeshFactoryWrapper *csEngine::FindMeshFactory (const char *iName,
  bool regionOnly) const
{
  csMeshFactoryWrapper* fact;
  if (regionOnly && region)
    fact = (csMeshFactoryWrapper*)FindObjectInRegion (region, mesh_factories, iName);
  else
    fact = (csMeshFactoryWrapper*)mesh_factories.FindByName (iName);
  if (!fact) return NULL;
  iMeshFactoryWrapper* ifact = &fact->scfiMeshFactoryWrapper;
  return ifact;
}

iMaterial* csEngine::CreateBaseMaterial (iTextureWrapper* txt)
{
  csMaterial* mat = new csMaterial ();
  if (txt) mat->SetTextureWrapper (txt);
  iMaterial* imat = SCF_QUERY_INTERFACE (mat, iMaterial);
  imat->DecRef ();
  return imat;
}

iMaterial* csEngine::CreateBaseMaterial (iTextureWrapper* txt,
  	int num_layers, iTextureWrapper** wrappers, csTextureLayer* layers)
{
  csMaterial* mat = new csMaterial ();
  if (txt) mat->SetTextureWrapper (txt);
  int i;
  for (i = 0 ; i < num_layers ; i++)
  {
    mat->AddTextureLayer (wrappers[i], layers[i].mode,
    	layers[i].uscale, layers[i].vscale, layers[i].ushift, layers[i].vshift);
  }
  iMaterial* imat = SCF_QUERY_INTERFACE (mat, iMaterial);
  imat->DecRef ();
  return imat;
}

iTextureList* csEngine::GetTextureList () const
{
  return &(GetTextures ()->scfiTextureList);
}

iMaterialList* csEngine::GetMaterialList () const
{
  return &(GetMaterials ()->scfiMaterialList);
}

iMaterialWrapper* csEngine::FindMaterial (const char* iName,
  bool regionOnly) const
{
  iMaterialWrapper* wr;
  if (regionOnly && region)
  {
    iObject* obj = FindObjectInRegion (region, *materials, iName);
    if (!obj) return NULL;
    wr = SCF_QUERY_INTERFACE_FAST (obj, iMaterialWrapper);
  }
  else
    wr = materials->FindByName (iName);
  return wr;
}

iTextureWrapper* csEngine::FindTexture (const char* iName,
  bool regionOnly) const
{
  iTextureWrapper* wr;
  if (regionOnly && region)
  {
    iObject* obj = FindObjectInRegion (region, *textures, iName);
    if (!obj) return NULL;
    wr = SCF_QUERY_INTERFACE_FAST (obj, iTextureWrapper);
  }
  else
    wr = textures->FindByName (iName);
  return wr;
}

iCameraPosition* csEngine::FindCameraPosition (const char* iName,
  bool regionOnly) const
{
  csCameraPosition* wr;
  if (regionOnly && region)
    wr = (csCameraPosition*)FindObjectInRegion (region, camera_positions, iName);
  else
    wr = (csCameraPosition*)camera_positions.FindByName (iName);
  if (!wr) return NULL;
  return &wr->scfiCameraPosition;
}

iCollection* csEngine::FindCollection (const char* iName,
  bool regionOnly) const
{
  csCollection* c;
  if (regionOnly && region)
    c = (csCollection*)FindObjectInRegion (region, collections, iName);
  else
    c = (csCollection*)collections.FindByName (iName);
  if (!c) return NULL;
  return &c->scfiCollection;
}

iCamera* csEngine::CreateCamera ()
{
  return &(new csCamera ())->scfiCamera;
}

iCollection* csEngine::CreateCollection (const char* iName)
{
  csCollection *c = (csCollection *)collections.FindByName (iName);
  if (c)
    return &(c->scfiCollection);

  c = new csCollection(this);
  c->SetName(iName);
  collections.Push(c);
  return &(c->scfiCollection);
}

iStatLight* csEngine::CreateLight (const char* name,
	const csVector3& pos, float radius, const csColor& color,
	bool pseudoDyn)
{
  csStatLight* light = new csStatLight (pos.x, pos.y, pos.z, radius,
  	color.red, color.green, color.blue, pseudoDyn);
  if (name) light->SetName (name);
  iStatLight* il = SCF_QUERY_INTERFACE (light, iStatLight);
  il->DecRef ();
  return il;
}

iDynLight* csEngine::CreateDynLight (const csVector3& pos, float radius,
  	const csColor& color)
{
  csDynLight* light = new csDynLight (pos.x, pos.y, pos.z, radius,
  	color.red, color.green, color.blue);
  AddDynLight (light);
  iDynLight* il = SCF_QUERY_INTERFACE (light, iDynLight);
  il->DecRef ();
  return il;
}

void csEngine::RemoveDynLight (iDynLight* light)
{
  RemoveDynLight (light->GetPrivateObject ());
}

iMeshFactoryWrapper* csEngine::CreateMeshFactory (const char* classId,
	const char* name)
{
  if (name != NULL)
  {
    iMeshFactoryWrapper* factwrap = FindMeshFactory (name);
    if (factwrap != NULL) return factwrap;
  }

  iMeshObjectType* type = CS_QUERY_PLUGIN_CLASS (System, classId, "MeshObj",
  	iMeshObjectType);
  if (!type) type = CS_LOAD_PLUGIN (System, classId, "MeshObj", iMeshObjectType);
  if (!type) return NULL;
  iMeshObjectFactory* fact = type->NewFactory ();
  if (!fact) return NULL;
  // don't pass the name to avoid a second search
  iMeshFactoryWrapper *fwrap = CreateMeshFactory (fact, NULL);
  if (fwrap && name) fwrap->QueryObject()->SetName(name);
  fact->DecRef ();
  type->DecRef ();
  return fwrap;
}

iMeshFactoryWrapper* csEngine::CreateMeshFactory (iMeshObjectFactory *fact,
	const char* name)
{
  if (name != NULL)
  {
    iMeshFactoryWrapper* factwrap = FindMeshFactory (name);
    if (factwrap != NULL) return factwrap;
  }

  csMeshFactoryWrapper* mfactwrap = new csMeshFactoryWrapper (fact);
  if (name) mfactwrap->SetName (name);
  mfactwrap->IncRef ();
  mesh_factories.Push (mfactwrap);
  return &mfactwrap->scfiMeshFactoryWrapper;
}

iMeshFactoryWrapper* csEngine::CreateMeshFactory (const char* name)
{
  if (name != NULL)
  {
    iMeshFactoryWrapper* factwrap = FindMeshFactory (name);
    if (factwrap != NULL) return factwrap;
  }

  csMeshFactoryWrapper* mfactwrap = new csMeshFactoryWrapper ();
  if (name) mfactwrap->SetName (name);
  mfactwrap->IncRef ();
  mesh_factories.Push (mfactwrap);
  return &mfactwrap->scfiMeshFactoryWrapper;
}

void csEngine::DeleteMeshFactory (const char* iName, bool regionOnly)
{
  csMeshFactoryWrapper* fact;
  if (regionOnly && region)
  {
    fact = (csMeshFactoryWrapper*)FindObjectInRegion (region,
    	mesh_factories, iName);
    if (fact) region->ReleaseFromRegion (fact);
  }
  else
  {
    int i;
    for (i = 0 ; i < mesh_factories.Length () ; i++)
    {
      csMeshFactoryWrapper* fact = (csMeshFactoryWrapper*)mesh_factories[i];
      if (fact->GetName () && !strcmp (fact->GetName (), iName))
      {
        mesh_factories.Delete (i);
	return;
      }
    }
  }
}

iMeshFactoryWrapper* csEngine::LoadMeshFactory (
  	const char* classId, const char* name,
	const char* loaderClassId,
	iDataBuffer* input)
{
  iLoaderPlugin* plug = CS_QUERY_PLUGIN_CLASS (System, loaderClassId, "MeshLdr",
  	iLoaderPlugin);
  if (!plug)
    plug = CS_LOAD_PLUGIN (System, loaderClassId, "MeshLdr", iLoaderPlugin);
  if (!plug)
    return NULL;

  iMeshFactoryWrapper* fact = CreateMeshFactory (classId, name);
  if (!fact) return NULL;

  char* buf = **input;
  iBase* mof = plug->Parse (buf, this, fact);
  plug->DecRef ();
  if (!mof) { DeleteMeshFactory (name); return NULL; }
  fact->SetMeshObjectFactory ((iMeshObjectFactory*)mof);
  return fact;
}

iMeshWrapper* csEngine::LoadMeshObject (
  	const char* classId, const char* name,
	const char* loaderClassId,
	iDataBuffer* input, iSector* sector, const csVector3& pos)
{
  iLoaderPlugin* plug = CS_QUERY_PLUGIN_CLASS (System, loaderClassId, "MeshLdr",
  	iLoaderPlugin);
  if (!plug)
    plug = CS_LOAD_PLUGIN (System, loaderClassId, "MeshLdr", iLoaderPlugin);
  if (!plug)
    return NULL;

  csMeshWrapper* meshwrap = new csMeshWrapper (&scfiObject);
  if (name) meshwrap->SetName (name);
  meshwrap->IncRef ();
  meshes.Push (meshwrap);
  if (sector)
  {
    meshwrap->GetMovable ().SetSector (sector->GetPrivateObject ());
    meshwrap->GetMovable ().SetPosition (pos);
    meshwrap->GetMovable ().UpdateMove ();
  }
  iMeshWrapper* imw = SCF_QUERY_INTERFACE (meshwrap, iMeshWrapper);
  imw->DecRef ();

  char* buf = **input;
  iBase* mof = plug->Parse (buf, this, imw);
  plug->DecRef ();
  if (!mof) { meshwrap->DecRef(); return NULL; }
  meshwrap->SetMeshObject ((iMeshObject*)mof);

  return imw;
}

iMeshWrapper* csEngine::CreateMeshObject (iMeshFactoryWrapper* factory,
  	const char* name, iSector* sector, const csVector3& pos)
{
  iMeshObjectFactory* fact = factory->GetMeshObjectFactory ();
  iMeshObject* mesh = fact->NewInstance ();
  iMeshWrapper* meshwrap = CreateMeshObject (mesh, name, sector, pos);
  mesh->DecRef ();
  return meshwrap;
}

iMeshWrapper* csEngine::CreateMeshObject (iMeshObject* mesh,
  	const char* name, iSector* sector, const csVector3& pos)
{
  csMeshWrapper* meshwrap = new csMeshWrapper (&scfiObject, mesh);
  if (name) meshwrap->SetName (name);
  meshes.Push (meshwrap);
  if (sector)
  {
    meshwrap->GetMovable ().SetSector (sector->GetPrivateObject ());
    meshwrap->GetMovable ().SetPosition (pos);
    meshwrap->GetMovable ().UpdateMove ();
  }
  return &meshwrap->scfiMeshWrapper;
}

iMeshWrapper* csEngine::CreateMeshObject (const char* name)
{
  csMeshWrapper* meshwrap = new csMeshWrapper (&scfiObject);
  if (name) meshwrap->SetName (name);
  meshes.Push (meshwrap);
  return &meshwrap->scfiMeshWrapper;
}

int csEngine::GetMeshObjectCount () const
{
  return meshes.Length ();
}

iMeshWrapper *csEngine::GetMeshObject (int n) const
{
  return &((csMeshWrapper*)meshes.Get(n))->scfiMeshWrapper;
}

int csEngine::GetMeshFactoryCount () const
{
  return mesh_factories.Length ();
}

iMeshFactoryWrapper *csEngine::GetMeshFactory (int n) const
{
  return &((csMeshFactoryWrapper*)mesh_factories.Get(n))->
  	scfiMeshFactoryWrapper;
}


iPolyTxtPlane* csEngine::CreatePolyTxtPlane (const char* name)
{
  csPolyTxtPlane* pl = new csPolyTxtPlane ();
  planes.Push (pl);
  if (name) pl->SetName (name);
  return &(pl->scfiPolyTxtPlane);
}

iPolyTxtPlane* csEngine::FindPolyTxtPlane (const char *iName,
  bool regionOnly) const
{
  csPolyTxtPlane* pl;
  if (regionOnly && region)
    pl = (csPolyTxtPlane*)FindObjectInRegion (region, planes, iName);
  else
    pl = (csPolyTxtPlane*)planes.FindByName (iName);
  if (!pl) return NULL;
  return &(pl->scfiPolyTxtPlane);
}

iCurveTemplate* csEngine::CreateBezierTemplate (const char* name)
{
  csBezierTemplate* ptemplate = new csBezierTemplate ();
  if (name) ptemplate->SetName (name);
  curve_templates.Push (ptemplate);
  iCurveTemplate* itmpl = SCF_QUERY_INTERFACE (ptemplate, iCurveTemplate);
  itmpl->DecRef ();
  return itmpl;
}

iCurveTemplate* csEngine::FindCurveTemplate (const char *iName,
	bool regionOnly) const
{
  csCurveTemplate* pl;
  if (regionOnly && region)
    pl = (csCurveTemplate*)FindObjectInRegion (region, curve_templates, iName);
  else
    pl = (csCurveTemplate*)curve_templates.FindByName (iName);
  if (!pl) return NULL;
  iCurveTemplate* itmpl = SCF_QUERY_INTERFACE (pl, iCurveTemplate);
  itmpl->DecRef ();
  return itmpl;
}

//----------------Begin-Multi-Context-Support------------------------------

void csEngine::Resize ()
{
  frame_width = G3D->GetWidth ();
  frame_height = G3D->GetHeight ();
  // Reset the culler.
  InitCuller ();
}

csEngine::csEngineState::csEngineState (csEngine *e)
{
  engine    = e;
  c_buffer = e->c_buffer;
  cbufcube = e->cbufcube;
  G2D      = e->G2D;
  G3D      = e->G3D;
  resize   = false;
}

csEngine::csEngineState::~csEngineState ()
{
  if (engine->G2D == G2D)
  {
    engine->G3D->DecRef ();
    engine->G3D      = NULL;
    engine->G2D      = NULL;
    engine->c_buffer = NULL;
    engine->cbufcube = NULL;
  }
  delete c_buffer;
  delete cbufcube;
}

void csEngine::csEngineState::Activate ()
{
  engine->c_buffer     = c_buffer;
  engine->cbufcube     = cbufcube;
  engine->frame_width  = G3D->GetWidth ();
  engine->frame_height = G3D->GetHeight ();

  if (resize)
  {
    engine->Resize ();

    c_buffer = engine->c_buffer;
    cbufcube = engine->cbufcube;
    resize   = false;
  }
}

void csEngine::csEngineStateVector::Close (iGraphics2D *g2d)
{
  // Hack-- with the glide back buffer implementations of procedural textures
  // circumstances are that many G3D can be associated with one G2D.
  // It is impossible to tell which is which so destroy them all, and rely on
  // regeneration the next time the context is set to the surviving G3Ds associated
  // with this G2D
  for (int i = 0; i < Length (); i++)
    if (((csEngineState*)root [i])->G2D == g2d)
    {
      //Delete (i);
      break;
    }
}

void csEngine::csEngineStateVector::Resize (iGraphics2D *g2d)
{
  // With the glide back buffer implementation of procedural textures
  // circumstances are that many G3D can be associated with one G2D, so
  // we test for width and height also.
  for (int i = 0; i < Length (); i++)
  {
    csEngineState *state = (csEngineState*)root [i];
    if (state->G2D == g2d)
      if ((state->G2D->GetWidth() == state->G3D->GetWidth ()) &&
	  (state->G2D->GetHeight() == state->G3D->GetHeight ()))
	  ((csEngineState*)root [i])->resize = true;
  }
}

void csEngine::SetContext (iGraphics3D* g3d)
{
  G2D = g3d->GetDriver2D ();
  if (g3d != G3D)
  {
    if (G3D) G3D->DecRef ();
    G3D = g3d;
    if (!engine_states)
    {
      engine_states = new csEngineStateVector();
      Resize ();
      engine_states->Push (new csEngineState (this));
    }
    else
    {
      int idg3d = engine_states->FindKey (g3d);
      if (idg3d < 0)
      {
	// Null out the culler which belongs to another state so its not deleted.
	c_buffer = NULL;
	cbufcube = NULL;
	frame_width = G3D->GetWidth ();
	frame_height = G3D->GetHeight ();
	cbufcube = new csCBufferCube (1024);
	InitCuller ();
	engine_states->Push (new csEngineState (this));
      }
      else
      {
	csEngineState *state = (csEngineState *)engine_states->Get (idg3d);
	state->Activate ();
      }
    }
    G3D->IncRef ();
  }
}

iClipper2D* csEngine::GetTopLevelClipper () const
{
  return top_clipper;
}

int csEngine::GetCollectionCount () const
{
  return collections.Length ();
}

iCollection* csEngine::GetCollection (int idx) const
{
  return &((csCollection*)collections.Get(idx))->scfiCollection;
}

int csEngine::GetCameraPositionCount () const
{
  return camera_positions.Length ();
}

iCameraPosition* csEngine::GetCameraPosition (int idx) const
{
  return &((csCameraPosition*)camera_positions.Get (idx))->scfiCameraPosition;
}

iCameraPosition* csEngine::GetCameraPosition (const char* name) const
{
  return &((csCameraPosition*)camera_positions.FindByName (name))
  	->scfiCameraPosition;
}

void csEngine::SetAmbientLight (const csColor &c)
{
  csLight::ambient_red = int(c.red * 255);
  csLight::ambient_green = int(c.green * 255);
  csLight::ambient_blue = int(c.blue * 255.0);
}

void csEngine::GetAmbientLight (csColor &c) const
{
  c.red = csLight::ambient_red / 255.0;
  c.green = csLight::ambient_green / 255.0;
  c.blue = csLight::ambient_blue / 255.0;
}

//-------------------End-Multi-Context-Support--------------------------------
