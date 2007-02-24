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
#include "csqint.h"

#include "csgeom/math3d.h"
#include "csgeom/poly3d.h"
#include "cstool/collider.h"
#include "cstool/cspixmap.h"
#include "csutil/csstring.h"
#include "csutil/flags.h"
#include "csutil/scanstr.h"
#include "iengine/camera.h"
#include "iengine/campos.h"
#include "iengine/light.h"
#include "iengine/material.h"
#include "iengine/movable.h"
#include "iengine/region.h"
#include "iengine/sector.h"
#include "iengine/sharevar.h"
#include "imesh/objmodel.h"
#include "igeom/polymesh.h"
#include "imap/loader.h"
#include "imesh/particles.h"
#include "imesh/lighting.h"
#include "imesh/object.h"
#include "imesh/partsys.h"
#include "imesh/sprite3d.h"
#include "imesh/thing.h"
#include "isndsys.h"
#include "ivaria/collider.h"
#include "ivaria/engseq.h"
#include "ivaria/reporter.h"
#include "ivaria/view.h"
#include "ivideo/graph3d.h"

#include "bot.h"
#include "command.h"
#include "infmaze.h"
#include "walktest.h"

extern WalkTest* Sys;


void RandomColor (float& r, float& g, float& b)
{
  float sig = (float)(900+(rand () % 100))/1000.;
  float sm1= (float)(rand () % 1000)/1000.;
  float sm2 = (float)(rand () % 1000)/1000.;
  switch ((rand ()>>3) % 3)
  {
    case 0: r = sig; g = sm1; b = sm2; break;
    case 1: r = sm1; g = sig; b = sm2; break;
    case 2: r = sm1; g = sm2; b = sig; break;
  }
}

extern iMeshWrapper* add_meshobj (const char* tname, char* sname, iSector* where,
	csVector3 const& pos, float size);
extern void move_mesh (iMeshWrapper* sprite, iSector* where,
	csVector3 const& pos);

//===========================================================================
// Demo particle system (rain).
//===========================================================================
void add_particles_rain (iSector* sector, char* matname, int num, float speed,
	bool do_camera)
{
  iEngine* engine = Sys->view->GetEngine ();
  // First check if the material exists.
  iMaterialWrapper* mat = engine->GetMaterialList ()->FindByName (matname);
  if (!mat)
  {
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY,
    	"Can't find material '%s' in memory!", matname);
    return;
  }

  csBox3 bbox;
  if (do_camera)
    bbox.Set (-5, -5, -5, 5, 5, 5);
  else
    sector->CalculateSectorBBox (bbox, true);

  csRef<iMeshFactoryWrapper> mfw = engine->CreateMeshFactory (
      "crystalspace.mesh.object.particles", "rain");
  if (!mfw) return;

  csRef<iMeshWrapper> exp = engine->CreateMeshWrapper (mfw, "custom rain",
	sector, csVector3 (0, 0, 0));

  if (do_camera)
  {
    iEngine* e = Sys->view->GetEngine ();
    int c = e->GetAlphaRenderPriority ();
    exp->GetFlags ().Set (CS_ENTITY_CAMERA);
    exp->SetRenderPriority (c);
  }
  exp->SetZBufMode(CS_ZBUF_TEST);
  exp->GetMeshObject()->SetMixMode (CS_FX_ADD);
  exp->GetMeshObject()->SetMaterialWrapper (mat);

  csRef<iParticleBuiltinEmitterFactory> emit_factory = 
      csLoadPluginCheck<iParticleBuiltinEmitterFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.emitter", false);
  csRef<iParticleBuiltinEffectorFactory> eff_factory = 
      csLoadPluginCheck<iParticleBuiltinEffectorFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.effector", false);

  csRef<iParticleBuiltinEmitterBox> boxemit = emit_factory->CreateBox ();
  // Time to live depends on height of sector.
  float velocity = 2.84f * speed / 2.0f;
  float seconds_to_live = (bbox.MaxY () - bbox.MinY ()) / velocity;
  csBox3 emit_bbox = bbox;
  emit_bbox.SetMin (1, emit_bbox.MaxY ());
  boxemit->SetBox (emit_bbox);
  boxemit->SetParticlePlacement (CS_PARTICLE_BUILTIN_VOLUME);
  boxemit->SetEmissionRate (float (num) / seconds_to_live);
  boxemit->SetInitialMass (5.0f, 7.5f);
  boxemit->SetUniformVelocity (true);
  boxemit->SetInitialTTL (seconds_to_live, seconds_to_live);
  boxemit->SetInitialVelocity (csVector3 (0, -velocity, 0),
      csVector3 (0));

  csRef<iParticleBuiltinEffectorLinColor> lincol = eff_factory->
    CreateLinColor ();
  lincol->AddColor (csColor4 (.25,.25,.25,1), seconds_to_live);

  csRef<iParticleSystem> partstate =
  	scfQueryInterface<iParticleSystem> (exp->GetMeshObject ());
  partstate->SetMinBoundingBox (bbox);
  partstate->SetParticleSize (csVector2 (0.3f/50.0f, 0.3f));
  partstate->SetParticleRenderOrientation (CS_PARTICLE_ORIENT_COMMON);
  partstate->SetCommonDirection (csVector3 (0, 1, 0));
  partstate->AddEmitter (boxemit);
  partstate->AddEffector (lincol);
}

//===========================================================================
// Demo particle system (snow).
//===========================================================================
void add_particles_snow (iSector* sector, char* matname, int num, float speed)
{
  iEngine* engine = Sys->view->GetEngine ();
  // First check if the material exists.
  iMaterialWrapper* mat = Sys->view->GetEngine ()->GetMaterialList ()->
  	FindByName (matname);
  if (!mat)
  {
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY, "Can't find material '%s' in memory!", matname);
    return;
  }

  csBox3 bbox;
  sector->CalculateSectorBBox (bbox, true);

  csRef<iMeshFactoryWrapper> mfw = engine->CreateMeshFactory (
      "crystalspace.mesh.object.particles", "snow");
  if (!mfw) return;

  csRef<iMeshWrapper> exp = engine->CreateMeshWrapper (mfw, "custom snow",
	sector, csVector3 (0, 0, 0));

  exp->SetZBufMode(CS_ZBUF_TEST);
  exp->GetMeshObject()->SetMixMode (CS_FX_ADD);
  exp->GetMeshObject()->SetMaterialWrapper (mat);

  csRef<iParticleBuiltinEmitterFactory> emit_factory = 
      csLoadPluginCheck<iParticleBuiltinEmitterFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.emitter", false);
  csRef<iParticleBuiltinEffectorFactory> eff_factory = 
      csLoadPluginCheck<iParticleBuiltinEffectorFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.effector", false);

  csRef<iParticleBuiltinEmitterBox> boxemit = emit_factory->CreateBox ();
  // Time to live depends on height of sector.
  float velocity = 2.0f * speed / 2.0f;
  float seconds_to_live = (bbox.MaxY () - bbox.MinY ()) / velocity;
  csBox3 emit_bbox = bbox;
  emit_bbox.SetMin (1, emit_bbox.MaxY ());
  boxemit->SetBox (emit_bbox);
  boxemit->SetParticlePlacement (CS_PARTICLE_BUILTIN_VOLUME);
  boxemit->SetEmissionRate (float (num) / seconds_to_live);
  boxemit->SetInitialMass (5.0f, 7.5f);
  boxemit->SetUniformVelocity (true);
  boxemit->SetInitialTTL (seconds_to_live, seconds_to_live);
  boxemit->SetInitialVelocity (csVector3 (0, -velocity, 0),
      csVector3 (0));

  csRef<iParticleBuiltinEffectorLinColor> lincol = eff_factory->
    CreateLinColor ();
  lincol->AddColor (csColor4 (.25,.25,.25,1), seconds_to_live);

  csRef<iParticleBuiltinEffectorForce> force = eff_factory->
    CreateForce ();
  force->SetRandomAcceleration (csVector3 (1.5f, 0.0f, 1.5f));

  csRef<iParticleSystem> partstate =
  	scfQueryInterface<iParticleSystem> (exp->GetMeshObject ());
  partstate->SetMinBoundingBox (bbox);
  partstate->SetParticleSize (csVector2 (0.07f, 0.07f));
  partstate->AddEmitter (boxemit);
  partstate->AddEffector (lincol);
  partstate->AddEffector (force);
}

//===========================================================================
// Demo particle system (fire).
//===========================================================================
void add_particles_fire (iSector* sector, char* matname, int num,
	const csVector3& origin)
{
  iEngine* engine = Sys->view->GetEngine ();

  // First check if the material exists.
  iMaterialWrapper* mat = engine->GetMaterialList ()->FindByName (matname);
  if (!mat)
  {
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY, "Can't find material '%s' in memory!", matname);
    return;
  }

  csRef<iMeshFactoryWrapper> mfw = engine->CreateMeshFactory (
      "crystalspace.mesh.object.particles", "fire");
  if (!mfw) return;

  csRef<iMeshWrapper> exp = engine->CreateMeshWrapper (mfw, "custom fire",
	sector, origin);

  exp->SetZBufMode(CS_ZBUF_TEST);
  exp->GetMeshObject()->SetMixMode (CS_FX_ADD);
  exp->GetMeshObject()->SetMaterialWrapper (mat);

  csRef<iParticleBuiltinEmitterFactory> emit_factory = 
      csLoadPluginCheck<iParticleBuiltinEmitterFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.emitter", false);
  csRef<iParticleBuiltinEffectorFactory> eff_factory = 
      csLoadPluginCheck<iParticleBuiltinEffectorFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.effector", false);

  csRef<iParticleBuiltinEmitterSphere> sphemit = emit_factory->CreateSphere ();
  float velocity = 0.5f;
  float seconds_to_live = 2.0f;
  sphemit->SetRadius (.2f);
  sphemit->SetParticlePlacement (CS_PARTICLE_BUILTIN_VOLUME);
  sphemit->SetEmissionRate (float (num) / seconds_to_live);
  sphemit->SetInitialMass (5.0f, 7.5f);
  sphemit->SetUniformVelocity (true);
  sphemit->SetInitialTTL (seconds_to_live, seconds_to_live);
  sphemit->SetInitialVelocity (csVector3 (0, velocity, 0),
      csVector3 (0));

  csRef<iParticleBuiltinEffectorLinColor> lincol = eff_factory->
    CreateLinColor ();
  lincol->AddColor (csColor4 (0.00f, 0.00f, 0.00f, 1.00f), 2.0000f);
  lincol->AddColor (csColor4 (1.00f, 0.35f, 0.00f, 0.00f), 1.5000f);
  lincol->AddColor (csColor4 (1.00f, 0.22f, 0.00f, 0.10f), 1.3125f);
  lincol->AddColor (csColor4 (1.00f, 0.12f, 0.00f, 0.30f), 1.1250f);
  lincol->AddColor (csColor4 (0.80f, 0.02f, 0.00f, 0.80f), 0.9375f);
  lincol->AddColor (csColor4 (0.60f, 0.00f, 0.00f, 0.90f), 0.7500f);
  lincol->AddColor (csColor4 (0.40f, 0.00f, 0.00f, 0.97f), 0.5625f);
  lincol->AddColor (csColor4 (0.20f, 0.00f, 0.00f, 1.00f), 0.3750f);
  lincol->AddColor (csColor4 (0.00f, 0.00f, 0.00f, 1.00f), 0.1875f);
  lincol->AddColor (csColor4 (0.00f, 0.00f, 0.00f, 1.00f), 0.0000f);

  csRef<iParticleBuiltinEffectorForce> force = eff_factory->
    CreateForce ();
  force->SetRandomAcceleration (csVector3 (1.5f, 1.5f, 1.5f));

  csRef<iParticleSystem> partstate =
  	scfQueryInterface<iParticleSystem> (exp->GetMeshObject ());
  //partstate->SetMinBoundingBox (bbox);
  partstate->SetParticleSize (csVector2 (0.04f, 0.08f));
  partstate->AddEmitter (sphemit);
  partstate->AddEffector (lincol);
  partstate->AddEffector (force);
}

//===========================================================================
// Demo particle system (fountain).
//===========================================================================
void add_particles_fountain (iSector* sector, char* matname, int num,
	const csVector3& origin)
{
  iEngine* engine = Sys->view->GetEngine ();

  // First check if the material exists.
  iMaterialWrapper* mat = engine->GetMaterialList ()->FindByName (matname);
  if (!mat)
  {
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY, "Can't find material '%s'!", matname);
    return;
  }

  csRef<iMeshFactoryWrapper> mfw = engine->CreateMeshFactory (
      "crystalspace.mesh.object.particles", "fountain");
  if (!mfw) return;

  csRef<iMeshWrapper> exp = engine->CreateMeshWrapper (mfw, "custom fountain",
	sector, origin);

  exp->SetZBufMode(CS_ZBUF_TEST);
  exp->GetMeshObject()->SetMixMode (CS_FX_ADD);
  exp->GetMeshObject()->SetMaterialWrapper (mat);

  csRef<iParticleBuiltinEmitterFactory> emit_factory = 
      csLoadPluginCheck<iParticleBuiltinEmitterFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.emitter", false);
  csRef<iParticleBuiltinEffectorFactory> eff_factory = 
      csLoadPluginCheck<iParticleBuiltinEffectorFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.effector", false);

  csRef<iParticleBuiltinEmitterCone> conemit = emit_factory->CreateCone ();
  float velocity = 3.0f;
  float seconds_to_live = 1.5f;
  conemit->SetExtent (csVector3 (0, 0.5f, 0));
  conemit->SetConeAngle (0.3f);
  conemit->SetParticlePlacement (CS_PARTICLE_BUILTIN_VOLUME);
  conemit->SetEmissionRate (float (num) / seconds_to_live);
  conemit->SetInitialMass (8.0f, 10.0f);
  conemit->SetInitialTTL (seconds_to_live, seconds_to_live);
  conemit->SetInitialVelocity (csVector3 (0, velocity, 0), csVector3 (0));

  csRef<iParticleBuiltinEffectorLinColor> lincol = eff_factory->
    CreateLinColor ();
  lincol->AddColor (csColor4 (0.25f, 0.35f, 0.55f, 1), seconds_to_live);

  csRef<iParticleBuiltinEffectorForce> force = eff_factory->
    CreateForce ();
  force->SetAcceleration (csVector3 (0.0f, -3.0f, 0.0f));

  csRef<iParticleSystem> partstate =
  	scfQueryInterface<iParticleSystem> (exp->GetMeshObject ());
  partstate->SetParticleSize (csVector2 (0.1f, 0.1f));
  partstate->AddEmitter (conemit);
  partstate->AddEffector (lincol);
  partstate->AddEffector (force);
}

//===========================================================================
// Demo particle system (explosion).
//===========================================================================
void add_particles_explosion (iSector* sector, iEngine* engine,
	const csVector3& center, char* matname)
{
  // First check if the material exists.
  iMaterialWrapper* mat = Sys->view->GetEngine ()->GetMaterialList ()->
  	FindByName (matname);
  if (!mat)
  {
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY, "Can't find material '%s' in memory!", matname);
    return;
  }

  csRef<iMeshFactoryWrapper> mfw = engine->CreateMeshFactory (
      "crystalspace.mesh.object.particles", "explosion");
  if (!mfw) return;

  csRef<iMeshWrapper> exp = engine->CreateMeshWrapper (mfw, "custom explosion",
	sector, center);

  exp->SetZBufMode(CS_ZBUF_TEST);
  exp->SetRenderPriority (engine->GetAlphaRenderPriority ());
  exp->GetMeshObject()->SetMaterialWrapper (mat);
  exp->GetMeshObject()->SetMixMode (CS_FX_ALPHA);
  exp->GetMeshObject()->SetColor (csColor (1, 1, 0));

  csRef<iParticleBuiltinEmitterFactory> emit_factory = 
      csLoadPluginCheck<iParticleBuiltinEmitterFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.emitter", false);
  csRef<iParticleBuiltinEffectorFactory> eff_factory = 
      csLoadPluginCheck<iParticleBuiltinEffectorFactory> (
        Sys->object_reg, "crystalspace.mesh.object.particles.effector", false);

  csRef<iParticleBuiltinEmitterSphere> sphereemit = emit_factory->
    CreateSphere ();
  sphereemit->SetRadius (0.1f);
  sphereemit->SetParticlePlacement (CS_PARTICLE_BUILTIN_CENTER);
  sphereemit->SetPosition (csVector3 (0, 0, 0));
  sphereemit->SetInitialVelocity (csVector3 (1, 0, 0), csVector3 (3, 3, 3));
  sphereemit->SetUniformVelocity (false);
  sphereemit->SetDuration (0.1f);
  sphereemit->SetEmissionRate (1000.0f);
  sphereemit->SetInitialTTL (1.0f, 1.0f);

  csRef<iParticleBuiltinEffectorLinColor> lincol = eff_factory->
    CreateLinColor ();
  lincol->AddColor (csColor4 (1,1,1,1), 1.0f);
  lincol->AddColor (csColor4 (1,1,1,0), 0.0f);

  csRef<iParticleSystem> partstate =
  	scfQueryInterface<iParticleSystem> (exp->GetMeshObject ());
  partstate->SetParticleSize (csVector2 (0.15f, 0.15f));
  partstate->SetRotationMode (CS_PARTICLE_ROTATE_VERTICES);
  partstate->SetIntegrationMode (CS_PARTICLE_INTEGRATE_BOTH);
  partstate->AddEmitter (sphereemit);
  partstate->AddEffector (lincol);

  Sys->Engine->DelayedRemoveObject (1100, exp);
  Sys->Engine->DelayedRemoveObject (1101, mfw);

  exp->PlaceMesh ();
}

//===========================================================================
// Everything for bots.
//===========================================================================

bool do_bots = false;

// Add a bot with some size at the specified positin.
void WalkTest::add_bot (float size, iSector* where, csVector3 const& pos,
	float dyn_radius, bool manual)
{
  csRef<iLight> dyn;
  if (dyn_radius)
  {
    float r, g, b;
    RandomColor (r, g, b);
    dyn = Sys->view->GetEngine ()->CreateLight ("",
    	pos, dyn_radius, csColor(r, g, b), CS_LIGHT_DYNAMICTYPE_DYNAMIC);
    where->GetLights ()->Add (dyn);
    dyn->Setup ();
    //@@@ BUG! Calling twice is needed.
    dyn->Setup ();
    Sys->dynamic_lights.Push (dyn);
  }
  iMeshFactoryWrapper* tmpl = Sys->view->GetEngine ()->GetMeshFactories ()
  	->FindByName ("bot");
  if (!tmpl) return;
  csRef<iMeshObject> botmesh (tmpl->GetMeshObjectFactory ()->NewInstance ());
  csRef<iMeshWrapper> botWrapper = Engine->CreateMeshWrapper (botmesh, "bot",
    where);

  csMatrix3 m; m.Identity (); m = m * size;
  botWrapper->GetMovable ()->SetTransform (m);
  
  botWrapper->GetMovable ()->UpdateMove ();
  csRef<iSprite3DState> state (SCF_QUERY_INTERFACE (botmesh, iSprite3DState));
  state->SetAction ("default");
  
  Bot* bot = new Bot (Sys->view->GetEngine(), botWrapper);
  bot->set_bot_move (pos);
  bot->set_bot_sector (where);
  bot->light = dyn;
  if (manual)
    manual_bots.Push (bot);
  else
    bots.Push (bot);
}

void WalkTest::del_bot (bool manual)
{
  if (manual)
  {
    if (manual_bots.Length () > 0)
      manual_bots.DeleteIndex (0);
  }
  else
  {
    if (bots.Length () > 0)
      bots.DeleteIndex (0);
  }
}

void WalkTest::move_bots (csTicks elapsed_time)
{
  size_t i;
  for (i = 0; i < bots.Length(); i++)
  {
    bots[i]->move (elapsed_time);
  }
}

//===========================================================================
// Everything for the missile.
//===========================================================================

#define DYN_TYPE_MISSILE 1
#define DYN_TYPE_RANDOM 2
#define DYN_TYPE_EXPLOSION 3

struct LightStruct
{
  int type;
};

struct MissileStruct
{
  int type;		// type == DYN_TYPE_MISSILE
  csOrthoTransform dir;
  csRef<iMeshWrapper> sprite;
  csRef<iSndSysSource> snd;
};

struct ExplosionStruct
{
  int type;		// type == DYN_TYPE_EXPLOSION
  float radius;
  int dir;
};

struct RandomLight
{
  int type;		// type == DYN_TYPE_RANDOM
  float dyn_move_dir;
  float dyn_move;
  float dyn_r1, dyn_g1, dyn_b1;
};

bool HandleDynLight (iLight* dyn, iEngine* engine)
{
  LightStruct* ls = (LightStruct*)(WalkDataObject::GetData(dyn->QueryObject ()));
  switch (ls->type)
  {
    case DYN_TYPE_MISSILE:
    {
      MissileStruct* ms = (MissileStruct*)(WalkDataObject::GetData(
      	dyn->QueryObject ()));
      csVector3 v (0, 0, 2.5);
      csVector3 old = dyn->GetCenter ();
      v = old + ms->dir.GetT2O () * v;
      iSector* s = dyn->GetSector ();
      bool mirror = false;
      csVector3 old_v = v;
      s = s->FollowSegment (ms->dir, v, mirror);
      if (ABS (v.x-old_v.x) > SMALL_EPSILON ||
      	ABS (v.y-old_v.y) > SMALL_EPSILON ||
	ABS (v.z-old_v.z) > SMALL_EPSILON)
      {
        v = old;
        if (ms->sprite)
      	{
          if ((rand () & 0x3) == 1)
	  {
	    int i;
	    if (do_bots)
	      for (i = 0 ; i < 40 ; i++)
            Sys->add_bot (1, dyn->GetSector (), dyn->GetCenter (), 0);
	  }
	  ms->sprite->GetMovable ()->ClearSectors ();
	  Sys->view->GetEngine ()->GetMeshes ()->Remove (ms->sprite);
	}
	csRef<WalkDataObject> ido (
		CS_GET_CHILD_OBJECT (dyn->QueryObject (), WalkDataObject));
        dyn->QueryObject ()->ObjRemove (ido);
        if (ms->snd)
        {
          ms->snd->GetStream ()->Pause();
        }
        delete ms;
        if (Sys->mySound)
        {
	  if (Sys->wMissile_boom)
	  {
	    iSndSysStream* st = Sys->wMissile_boom->GetStream ();
	    csRef<iSndSysSource> sndsource = Sys->mySound->
	      	CreateSource (st);
	    if (sndsource)
	    {
	      csRef<iSndSysSourceSoftware3D> sndsource3d
		= scfQueryInterface<iSndSysSourceSoftware3D> (sndsource);

	      sndsource3d->SetPosition (v);
	      sndsource3d->SetVolume (1.0f);
	      st->SetLoopState (CS_SNDSYS_STREAM_DONTLOOP);
	      st->Unpause ();
	    }
	  }
        }
        ExplosionStruct* es = new ExplosionStruct;
        es->type = DYN_TYPE_EXPLOSION;
        es->radius = 2;
        es->dir = 1;
        WalkDataObject* esdata = new WalkDataObject (es);
        dyn->QueryObject ()->ObjAdd (esdata);
	esdata->DecRef ();
        add_particles_explosion (dyn->GetSector (),
		engine, dyn->GetCenter (), "explo");
        return false;
      }
      else ms->dir.SetOrigin (v);
      if (dyn->GetSector () != s)
      {
	dyn->IncRef ();
        dyn->GetSector ()->GetLights ()->Remove (dyn);
        s->GetLights ()->Add (dyn);
	dyn->DecRef ();
      }
      dyn->SetCenter (v);
      dyn->Setup ();
      if (ms->sprite) move_mesh (ms->sprite, s, v);
      if (Sys->mySound && ms->snd)
      {
	csRef<iSndSysSourceSoftware3D> sndsource3d
		= scfQueryInterface<iSndSysSourceSoftware3D> (ms->snd);
	sndsource3d->SetPosition (v);
	sndsource3d->SetVolume (1.0f);
      }
      break;
    }
    case DYN_TYPE_EXPLOSION:
    {
      ExplosionStruct* es = (ExplosionStruct*)(WalkDataObject::GetData(
      	dyn->QueryObject ()));
      if (es->dir == 1)
      {
        es->radius += 3;
	if (es->radius > 6) es->dir = -1;
      }
      else
      {
        es->radius -= 2;
	if (es->radius < 1)
	{
	  csRef<WalkDataObject> ido (
	  	CS_GET_CHILD_OBJECT (dyn->QueryObject (), WalkDataObject));
	  dyn->QueryObject ()->ObjRemove (ido);
	  delete es;
	  dyn->GetSector ()->GetLights ()->Remove (dyn);
	  return true;
	}
      }
      dyn->SetCutoffDistance (es->radius);
      dyn->Setup ();
      break;
    }
    case DYN_TYPE_RANDOM:
    {
      RandomLight* rl = (RandomLight*)(WalkDataObject::GetData(
      	dyn->QueryObject ()));
      rl->dyn_move += rl->dyn_move_dir;
      if (rl->dyn_move < 0 || rl->dyn_move > 2)
      	rl->dyn_move_dir = -rl->dyn_move_dir;
      if (ABS (rl->dyn_r1-dyn->GetColor ().red) < .01 &&
      	  ABS (rl->dyn_g1-dyn->GetColor ().green) < .01 &&
	  ABS (rl->dyn_b1-dyn->GetColor ().blue) < .01)
        RandomColor (rl->dyn_r1, rl->dyn_g1, rl->dyn_b1);
      else
        dyn->SetColor (csColor ((rl->dyn_r1+7.*dyn->GetColor ().red)/8.,
		(rl->dyn_g1+7.*dyn->GetColor ().green)/8.,
		(rl->dyn_b1+7.*dyn->GetColor ().blue)/8.));
      dyn->SetCenter (dyn->GetCenter () + csVector3 (0, rl->dyn_move_dir, 0));
      dyn->Setup ();
      break;
    }
  }
  return false;
}

void show_lightning ()
{
  csRef<iEngineSequenceManager> seqmgr(CS_QUERY_REGISTRY (Sys->object_reg,
  	iEngineSequenceManager));
  if (seqmgr)
  {
    // This finds the light L1 (the colored light over the stairs) and
    // makes the lightning restore this color back after it runs.
    iLight *light = Sys->view->GetEngine ()->FindLight("l1");
    iSharedVariable *var = Sys->view->GetEngine ()->GetVariableList()
    	->FindByName("Lightning Restore Color");
    if (light && var)
    {
      var->SetColor (light->GetColor ());
    }
    seqmgr->RunSequenceByName ("seq_lightning", 0);
  }
  else
  {
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY,
    	             "Could not find engine sequence manager!");
  }
}

void fire_missile ()
{
  csVector3 dir (0, 0, 0);
  csVector3 pos = Sys->view->GetCamera ()->GetTransform ().This2Other (dir);
  float r, g, b;
  RandomColor (r, g, b);
  csRef<iLight> dyn =
  	Sys->view->GetEngine ()->CreateLight ("", pos, 4, csColor (r, g, b),
		CS_LIGHT_DYNAMICTYPE_DYNAMIC);
  Sys->view->GetCamera ()->GetSector ()->GetLights ()->Add (dyn);
  dyn->Setup ();
  // @@@ BUG!!! Calling twice is needed.
  dyn->Setup ();
  Sys->dynamic_lights.Push (dyn);

  MissileStruct* ms = new MissileStruct;
  ms->snd = 0;
  if (Sys->mySound)
  {
    iSndSysStream* sndstream = Sys->wMissile_whoosh->GetStream ();
    ms->snd = Sys->mySound->CreateSource (sndstream);
    if (ms->snd)
    {
      csRef<iSndSysSourceSoftware3D> sndsource3d
		= scfQueryInterface<iSndSysSourceSoftware3D> (ms->snd);

      sndsource3d->SetPosition (pos);
      sndsource3d->SetVolume (1.0f);
      ms->snd->GetStream ()->Unpause ();
    }
  }
  ms->type = DYN_TYPE_MISSILE;
  ms->dir = (csOrthoTransform)(Sys->view->GetCamera ()->GetTransform ());
  ms->sprite = 0;
  WalkDataObject* msdata = new WalkDataObject(ms);
  dyn->QueryObject ()->ObjAdd(msdata);
  msdata->DecRef ();

  csString misname;
  misname.Format ("missile%d", ((rand () >> 3) & 1)+1);

  iMeshFactoryWrapper *tmpl = Sys->view->GetEngine ()->GetMeshFactories ()
  	->FindByName (misname);
  if (!tmpl)
    Sys->Report (CS_REPORTER_SEVERITY_NOTIFY,
    	"Could not find '%s' sprite factory!", misname.GetData());
  else
  {
    csRef<iMeshWrapper> sp (
    	Sys->view->GetEngine ()->CreateMeshWrapper (tmpl,
	"missile",Sys->view->GetCamera ()->GetSector (), pos));

    ms->sprite = sp;
    csMatrix3 m = ms->dir.GetT2O ();
    sp->GetMovable ()->SetTransform (m);
    sp->GetMovable ()->UpdateMove ();
  }
}

void AttachRandomLight (iLight* light)
{
  RandomLight* rl = new RandomLight;
  rl->type = DYN_TYPE_RANDOM;
  rl->dyn_move_dir = 0.2f;
  rl->dyn_move = 0;
  rl->dyn_r1 = rl->dyn_g1 = rl->dyn_b1 = 1;
  WalkDataObject* rldata = new WalkDataObject (rl);
  light->QueryObject ()->ObjAdd (rldata);
  rldata->DecRef ();
}

//===========================================================================

static csPtr<iMeshWrapper> CreateMeshWrapper (const char* name)
{
  csRef<iMeshObjectType> ThingType = csLoadPluginCheck<iMeshObjectType> (
  	Sys->object_reg, "crystalspace.mesh.object.thing");
  if (!ThingType)
    return 0;

  csRef<iMeshObjectFactory> thing_fact = ThingType->NewFactory ();
  csRef<iMeshObject> mesh_obj = thing_fact->NewInstance ();

  csRef<iMeshWrapper> mesh_wrap =
  	Sys->Engine->CreateMeshWrapper (mesh_obj, name);
  return csPtr<iMeshWrapper> (mesh_wrap);
}

static csPtr<iMeshWrapper> CreatePortalThing (const char* name, iSector* room,
    	iMaterialWrapper* tm, int& portalPoly)
{
  csRef<iMeshWrapper> thing = CreateMeshWrapper (name);
  csRef<iThingFactoryState> thing_fact_state = 
    scfQueryInterface<iThingFactoryState> (
    thing->GetMeshObject ()->GetFactory());
  thing->GetMovable ()->SetSector (room);
  float dx = 1, dy = 3, dz = 0.3f;
  float border = 0.3f; // width of border around the portal

  // bottom
  thing_fact_state->AddQuad (
    csVector3 (-dx, 0, -dz),
    csVector3 (dx, 0, -dz),
    csVector3 (dx, 0, dz),
    csVector3 (-dx, 0, dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.25, 0.875),
      csVector2 (0.75, 0.875),
      csVector2 (0.75, 0.75));

  // top
  thing_fact_state->AddQuad (
    csVector3 (-dx, dy, dz),
    csVector3 (dx, dy, dz),
    csVector3 (dx, dy, -dz),
    csVector3 (-dx, dy, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.25, 0.25),
      csVector2 (0.75, 0.25),
      csVector2 (0.75, 0.125));

  // back
  thing_fact_state->AddQuad (
    csVector3 (-dx, 0, dz),
    csVector3 (dx, 0, dz),
    csVector3 (dx, dy, dz),
    csVector3 (-dx, dy, dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.25, 0.75),
      csVector2 (0.75, 0.75),
      csVector2 (0.75, 0.25));

  // right
  thing_fact_state->AddQuad (
    csVector3 (dx, 0, dz),
    csVector3 (dx, 0, -dz),
    csVector3 (dx, dy, -dz),
    csVector3 (dx, dy, dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.75, 0.75),
      csVector2 (0.875, 0.75),
      csVector2 (0.875, 0.25));

  // left
  thing_fact_state->AddQuad (
    csVector3 (-dx, 0, -dz),
    csVector3 (-dx, 0, dz),
    csVector3 (-dx, dy, dz),
    csVector3 (-dx, dy, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.125, 0.75),
      csVector2 (0.25, 0.75),
      csVector2 (0.25, 0.25));

  // front border
  // border top
  thing_fact_state->AddQuad (
    csVector3 (-dx+border, dy, -dz),
    csVector3 (dx-border, dy, -dz),
    csVector3 (dx-border, dy-border, -dz),
    csVector3 (-dx+border, dy-border, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.125, 0.125),
      csVector2 (0.875, 0.125),
      csVector2 (0.875, 0.0));
  // border right
  thing_fact_state->AddQuad (
    csVector3 (dx-border, dy-border, -dz),
    csVector3 (dx, dy-border, -dz),
    csVector3 (dx, border, -dz),
    csVector3 (dx-border, border, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (1.0, 0.125),
      csVector2 (0.875, 0.125),
      csVector2 (0.875, 0.875));
  // border bottom
  thing_fact_state->AddQuad (
    csVector3 (-dx+border, border, -dz),
    csVector3 (+dx-border, border, -dz),
    csVector3 (+dx-border, 0.0, -dz),
    csVector3 (-dx+border, 0.0, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.125, 1.0),
      csVector2 (0.875, 1.0),
      csVector2 (0.875, 0.875));
  // border left
  thing_fact_state->AddQuad (
    csVector3 (-dx, dy-border, -dz),
    csVector3 (-dx+border, dy-border, -dz),
    csVector3 (-dx+border, border, -dz),
    csVector3 (-dx, border, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.125, 0.125),
      csVector2 (0.0, 0.125),
      csVector2 (0.0, 0.875));
  // border topleft
  thing_fact_state->AddQuad (
    csVector3 (-dx, dy, -dz),
    csVector3 (-dx+border, dy, -dz),
    csVector3 (-dx+border, dy-border, -dz),
    csVector3 (-dx, dy-border, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.125, 0.125),
      csVector2 (0.0, 0.125),
      csVector2 (0.0, 0.0));
  // border topright
  thing_fact_state->AddQuad (
    csVector3 (dx-border, dy, -dz),
    csVector3 (dx, dy, -dz),
    csVector3 (dx, dy-border, -dz),
    csVector3 (dx-border, dy-border, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (1.0, 0.125),
      csVector2 (0.875, 0.125),
      csVector2 (0.875, 0.0));
  // border botright
  thing_fact_state->AddQuad (
    csVector3 (dx-border, border, -dz),
    csVector3 (dx, border, -dz),
    csVector3 (dx, 0.0, -dz),
    csVector3 (dx-border, 0.0, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (1.0, 1.0),
      csVector2 (0.875, 1.0),
      csVector2 (0.875, 0.875));
  // border botleft
  thing_fact_state->AddQuad (
    csVector3 (-dx, border, -dz),
    csVector3 (-dx+border, border, -dz),
    csVector3 (-dx+border, 0.0, -dz),
    csVector3 (-dx, 0.0, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0.125, 1.0),
      csVector2 (0.0, 1.0),
      csVector2 (0.0, 0.875));

  // front - the portal
  portalPoly = thing_fact_state->AddQuad (
    csVector3 (dx-border, border, -dz),
    csVector3 (-dx+border, border, -dz),
    csVector3 (-dx+border, dy-border, -dz),
    csVector3 (dx-border, dy-border, -dz));
  thing_fact_state->SetPolygonTextureMapping (CS_POLYRANGE_LAST,
      csVector2 (0, 0),
      csVector2 (1, 0),
      csVector2 (1, 1));

  thing_fact_state->SetPolygonMaterial (CS_POLYRANGE_ALL, tm);
  thing_fact_state->SetPolygonFlags (CS_POLYRANGE_ALL, CS_POLY_COLLDET);

  csRef<iLightingInfo> linfo (SCF_QUERY_INTERFACE (thing->GetMeshObject (),
    iLightingInfo));
  linfo->InitializeDefault (true);
  room->ShineLights (thing);
  linfo->PrepareLighting ();

  return csPtr<iMeshWrapper> (thing);
}

void OpenPortal (iLoader *LevelLoader, iView* view, char* lev)
{
  iSector* room = view->GetCamera ()->GetSector ();
  csVector3 pos = view->GetCamera ()->GetTransform ().This2Other (
  	csVector3 (0, 0, 1));
  iMaterialWrapper* tm = Sys->Engine->GetMaterialList ()->
  	FindByName ("portal");

  int portalPoly;
  csRef<iMeshWrapper> thing = CreatePortalThing ("portalTo", room, tm,
  	portalPoly);
  csRef<iThingFactoryState> thing_fact_state = 
    scfQueryInterface<iThingFactoryState> (
    thing->GetMeshObject ()->GetFactory());

  bool regionExists = (Sys->Engine->GetRegions ()->FindByName (lev) != 0);
  iRegion* cur_region = Sys->Engine->CreateRegion (lev);
  // If the region did not already exist then we load the level in it.
  if (!regionExists)
  {
    // @@@ No error checking!
    csString buf;
    buf.Format ("/lev/%s", lev);
    Sys->myVFS->ChDir (buf);
    LevelLoader->LoadMapFile ("world", false, cur_region, true);
    cur_region->Prepare ();
  }

  iMovable* tmov = thing->GetMovable ();
  tmov->SetPosition (pos + csVector3 (0, Sys->cfg_legs_offset, 0));
  tmov->Transform (view->GetCamera ()->GetTransform ().GetT2O ());
  tmov->UpdateMove ();

  // First make a portal to the new level.
  iCameraPosition* cp = cur_region->FindCameraPosition ("Start");
  const char* room_name;
  csVector3 topos;
  if (cp) { room_name = cp->GetSector (); topos = cp->GetPosition (); }
  else { room_name = "room"; topos.Set (0, 0, 0); }
  topos.y -= Sys->cfg_eye_offset;
  iSector* start_sector = cur_region->FindSector (room_name);
  if (start_sector)
  {
    csPoly3D poly;
    poly.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 0));
    poly.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 1));
    poly.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 2));
    poly.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 3));
    iPortal* portal;
    csRef<iMeshWrapper> portalMesh = Sys->Engine->CreatePortal (
    	"new_portal", tmov->GetSectors ()->Get (0), csVector3 (0),
	start_sector, poly.GetVertices (), (int)poly.GetVertexCount (),
	portal);
    //iPortal* portal = portalPoly->CreatePortal (start_sector);
    portal->GetFlags ().Set (CS_PORTAL_ZFILL);
    portal->GetFlags ().Set (CS_PORTAL_CLIPDEST);
    portal->SetWarp (view->GetCamera ()->GetTransform ().GetT2O (), topos, pos);

    if (!regionExists)
    {
      // Only if the region did not already exist do we create a portal
      // back. So even if multiple portals go to the region we only have
      // one portal back.
      int portalPolyBack;
      csRef<iMeshWrapper> thingBack = CreatePortalThing ("portalFrom",
	  	start_sector, tm, portalPolyBack);
      csRef<iThingFactoryState> thing_fact_state = 
	scfQueryInterface<iThingFactoryState> (
	thingBack->GetMeshObject ()->GetFactory());
      iMovable* tbmov = thingBack->GetMovable ();
      tbmov->SetPosition (topos + csVector3 (0, Sys->cfg_legs_offset, -0.1f));
      tbmov->Transform (csYRotMatrix3 (PI));//view->GetCamera ()->GetW2C ());
      tbmov->UpdateMove ();
      iPortal* portalBack;
      csPoly3D polyBack;
      polyBack.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 0));
      polyBack.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 1));
      polyBack.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 2));
      polyBack.AddVertex (thing_fact_state->GetPolygonVertex (portalPoly, 3));
      csRef<iMeshWrapper> portalMeshBack = Sys->Engine->CreatePortal (
    	  "new_portal_back", tbmov->GetSectors ()->Get (0), csVector3 (0),
	  tmov->GetSectors ()->Get (0), polyBack.GetVertices (),
	  (int)polyBack.GetVertexCount (),
	  portalBack);
      //iPortal* portalBack = portalPolyBack->CreatePortal (room);
      portalBack->GetFlags ().Set (CS_PORTAL_ZFILL);
      portalBack->GetFlags ().Set (CS_PORTAL_CLIPDEST);
      portalBack->SetWarp (view->GetCamera ()->GetTransform ().GetO2T (),
      	-pos, -topos);
    }
  }

  if (!regionExists)
    Sys->InitCollDet (Sys->Engine, cur_region);
}

