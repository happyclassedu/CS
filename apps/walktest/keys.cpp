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

#include "sysdef.h"
#include "qint.h"
#include "cssys/common/system.h"
#include "walktest/walktest.h"
#include "walktest/scon.h"
#include "walktest/bot.h"
#include "walktest/infmaze.h"
#include "support/command.h"
#include "csengine/camera.h"
#include "csengine/world.h"
#include "csengine/csview.h"
#include "csengine/wirefrm.h"
#include "csengine/objects/cssprite.h"
#include "csengine/polygon/polygon.h"
#include "csengine/light/light.h"
#include "csengine/sector.h"
#include "csengine/2d/csspr2d.h"
#include "csutil/scanstr.h"
#include "csobject/nameobj.h"
#include "csobject/dataobj.h"
#include "cssndldr/common/sndbuf.h"
#include "csparser/sndbufo.h"
#include "csscript/csscript.h"
#include "isndsrc.h"
#include "isndlstn.h"
#include "isndrdr.h"

#include "csgeom/math3d.h"

#include "igraph3d.h"
//#include "walktest/physics.h"

csKeyMap* mapping = NULL;

Bot* first_bot = NULL;
bool do_bots = false;

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
  csSprite3D* sprite;
  ISoundSource *snd;
};

struct ExplosionStruct
{
  int type;		// type == DYN_TYPE_EXPLOSION
  float radius;
  int dir;
  ISoundSource *snd;
};

struct RandomLight
{
  int type;		// type == DYN_TYPE_RANDOM
  float dyn_move_dir;
  float dyn_move;
  float dyn_r1, dyn_g1, dyn_b1;
};

void RandomColor (float& r, float& g, float& b)
{
  switch ((rand ()>>3) % 3)
  {
    case 0:
      r = (float)(900+(rand () % 100))/1000.;
      g = (float)(rand () % 1000)/1000.;
      b = (float)(rand () % 1000)/1000.;
      break;
    case 1:
      r = (float)(rand () % 1000)/1000.;
      g = (float)(900+(rand () % 100))/1000.;
      b = (float)(rand () % 1000)/1000.;
      break;
    case 2:
      r = (float)(rand () % 1000)/1000.;
      g = (float)(rand () % 1000)/1000.;
      b = (float)(900+(rand () % 100))/1000.;
      break;
  }
}

void move_sprite (csSprite3D* sprite, csSector* where, csVector3 const& pos)
{
  sprite->SetMove (pos);
  sprite->MoveToSector (where);
  csLight* lights[2];
  int num_lights = Sys->world->GetNearbyLights (where, pos, CS_NLIGHT_STATIC|CS_NLIGHT_DYNAMIC, lights, 2);
  sprite->UpdateLighting (lights, num_lights);
}

void add_sprite (char* name, csSector* where, csVector3 const& pos, float size)
{
  csSpriteTemplate* tmpl = Sys->view->GetWorld ()->GetSpriteTemplate (name, true);
  if (!tmpl)
  {
    Sys->Printf (MSG_CONSOLE, "Unknown sprite template '%s'!\n", name);
    return;
  }
  csSprite3D* spr = tmpl->NewSprite ();
  csNameObject::AddName (*spr, name);
  Sys->view->GetWorld ()->sprites.Push (spr);
  spr->MoveToSector (where);
  spr->SetMove (pos);
  csMatrix3 m; m.Identity (); m = m * size;
  spr->SetTransform (m);

  CHK (csDataObject* sprdata = new csDataObject ((void*)1));
  spr->ObjAdd (sprdata);

  move_sprite (spr, where, pos);
}

void add_bot (float size, csSector* where, csVector3 const& pos, float dyn_radius)
{
  csDynLight* dyn = NULL;
  if (dyn_radius)
  {
    float r, g, b;
    RandomColor (r, g, b);
    //@@@ MEMORY LEAK?
    CHK (dyn = new csDynLight (pos.x, pos.y, pos.z, dyn_radius, r, g, b));
    Sys->view->GetWorld ()->AddDynLight (dyn);
    dyn->SetSector (where);
    dyn->Setup ();
  }
  csSpriteTemplate* tmpl = Sys->view->GetWorld ()->GetSpriteTemplate ("bot", true);
  if (!tmpl) return;
  Bot* bot;
  CHK (bot = new Bot (tmpl));
  csNameObject::AddName (*bot, "bot");
  Sys->view->GetWorld ()->sprites.Push (bot);
  bot->MoveToSector (where);
  csMatrix3 m; m.Identity (); m = m * size;
  bot->SetTransform (m);
  bot->set_bot_move (pos);
  bot->set_bot_sector (where);
  bot->SetAction ("default");
  bot->InitSprite ();
  bot->next = first_bot;
  bot->light = dyn;
  first_bot = bot;
}

void HandleSprite (csSprite3D* spr)
{
  int data = (int)csDataObject::GetData (*spr);
  if (data)
  {
    move_sprite (spr, (csSector*)(spr->sectors[0]), spr->GetW2TTranslation ());
  }
}

void HandleDynLight (csDynLight* dyn)
{
  LightStruct* ls = (LightStruct*)(csDataObject::GetData(*dyn));
  switch (ls->type)
  {
    case DYN_TYPE_MISSILE:
    {
      MissileStruct* ms = (MissileStruct*)(csDataObject::GetData(*dyn));
      csVector3 v (0, 0, 2.5);
      csVector3 old = dyn->GetCenter ();
      v = old + ms->dir.GetT2O () * v;
      csSector* s = dyn->GetSector ();
      bool mirror = false;
      csVector3 old_v = v;
      s = s->FollowSegment (ms->dir, v, mirror);
      if (ABS (v.x-old_v.x) > SMALL_EPSILON || ABS (v.y-old_v.y) > SMALL_EPSILON && ABS (v.z-old_v.z) > SMALL_EPSILON)
      {
        v = old;
        if (ms->sprite)
      	{
          if ((rand () & 0x3) == 1)
	  {
	    int i;
	    if (do_bots)
	      for (i = 0 ; i < 40 ; i++)
            add_bot (1, dyn->GetSector (), dyn->GetCenter (), 0);
	  }
	  ms->sprite->RemoveFromSectors ();
	  Sys->view->GetWorld ()->RemoveSprite (ms->sprite);
	}
        dyn->ObjRemove(dyn->GetObj(csDataObject::Type()));
        CHK (delete ms);
	CHK (ExplosionStruct* es = new ExplosionStruct);
	Sys->piSound->CreateSource (&es->snd, Sys->wMissile_boom);
	if (es->snd)
	{
	  es->snd->SetPosition (v.x, v.y, v.z);
	  es->snd->PlaySource ();
	}
	es->type = DYN_TYPE_EXPLOSION;
	es->radius = 2;
	es->dir = 1;
        CHK(csDataObject* esdata = new csDataObject(es));
	dyn->ObjAdd(esdata);
        return;
      }
      dyn->Move (s, v.x, v.y, v.z);
      dyn->Setup ();
      if (ms->sprite) move_sprite (ms->sprite, s, v);
      if (ms->snd) ms->snd->SetPosition (v.x, v.y, v.z);
      break;
    }
    case DYN_TYPE_EXPLOSION:
    {
      ExplosionStruct* es = (ExplosionStruct*)(csDataObject::GetData(*dyn));
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
          if (es->snd)
	  {
	    es->snd->StopSource ();
            FINAL_RELEASE (es->snd);
	  }
	  CHK (delete es);
          Sys->view->GetWorld ()->RemoveDynLight (dyn);
          CHK (delete dyn);
	  return;
	}
      }
      dyn->Resize (es->radius);
      dyn->Setup ();
      break;
    }
    case DYN_TYPE_RANDOM:
    {
      RandomLight* rl = (RandomLight*)(csDataObject::GetData(*dyn));
      rl->dyn_move += rl->dyn_move_dir;
      if (rl->dyn_move < 0 || rl->dyn_move > 2) rl->dyn_move_dir = -rl->dyn_move_dir;
      if (ABS (rl->dyn_r1-dyn->GetColor ().red) < .01 && ABS (rl->dyn_g1-dyn->GetColor ().green) < .01 && ABS (rl->dyn_b1-dyn->GetColor ().blue) < .01)
        RandomColor (rl->dyn_r1, rl->dyn_g1, rl->dyn_b1);
      else
        dyn->SetColor (csColor ((rl->dyn_r1+7.*dyn->GetColor ().red)/8., (rl->dyn_g1+7.*dyn->GetColor ().green)/8., (rl->dyn_b1+7.*dyn->GetColor ().blue)/8.));
      dyn->Move (dyn->GetSector (), dyn->GetCenter ().x, dyn->GetCenter ().y+rl->dyn_move_dir, dyn->GetCenter ().z);
      dyn->Setup ();
      break;
    }
  }
}

void map_key (char* keyname, csKeyMap* map)
{
  map->shift = 0;
  map->alt = 0;
  map->ctrl = 0;
  map->need_status = 0;
  char* dash = strchr (keyname, '-');
  while (dash)
  {
    *dash = 0;
    if (!strcmp (keyname, "shift")) map->shift = 1;
    else if (!strcmp (keyname, "alt")) map->alt = 1;
    else if (!strcmp (keyname, "ctrl")) map->ctrl = 1;
    else if (!strcmp (keyname, "status")) map->need_status = 1;
    else Sys->Printf (MSG_CONSOLE, "Bad modifier '%s'!\n", keyname);

    *dash = '-';
    keyname = dash+1;
    dash = strchr (dash+1, '-');
  }

  if (!strcmp (keyname, "tab")) map->key = CSKEY_TAB;
  else if (!strcmp (keyname, "space")) map->key = ' ';
  else if (!strcmp (keyname, "esc")) map->key = CSKEY_ESC;
  else if (!strcmp (keyname, "enter")) map->key = CSKEY_ENTER;
  else if (!strcmp (keyname, "bs")) map->key = CSKEY_BACKSPACE;
  else if (!strcmp (keyname, "up")) map->key = CSKEY_UP;
  else if (!strcmp (keyname, "down")) map->key = CSKEY_DOWN;
  else if (!strcmp (keyname, "right")) map->key = CSKEY_RIGHT;
  else if (!strcmp (keyname, "left")) map->key = CSKEY_LEFT;
  else if (!strcmp (keyname, "pgup")) map->key = CSKEY_PGUP;
  else if (!strcmp (keyname, "pgdn")) map->key = CSKEY_PGDN;
  else if (!strcmp (keyname, "home")) map->key = CSKEY_HOME;
  else if (!strcmp (keyname, "end")) map->key = CSKEY_END;
  else if (!strcmp (keyname, "ins")) map->key = CSKEY_INS;
  else if (!strcmp (keyname, "del")) map->key = CSKEY_DEL;
  else if (!strcmp (keyname, "f1")) map->key = CSKEY_F1;
  else if (!strcmp (keyname, "f2")) map->key = CSKEY_F2;
  else if (!strcmp (keyname, "f3")) map->key = CSKEY_F3;
  else if (!strcmp (keyname, "f4")) map->key = CSKEY_F4;
  else if (!strcmp (keyname, "f5")) map->key = CSKEY_F5;
  else if (!strcmp (keyname, "f6")) map->key = CSKEY_F6;
  else if (!strcmp (keyname, "f7")) map->key = CSKEY_F7;
  else if (!strcmp (keyname, "f8")) map->key = CSKEY_F8;
  else if (!strcmp (keyname, "f9")) map->key = CSKEY_F9;
  else if (!strcmp (keyname, "f10")) map->key = CSKEY_F10;
  else if (!strcmp (keyname, "f11")) map->key = CSKEY_F11;
  else if (!strcmp (keyname, "f12")) map->key = CSKEY_F12;
  else if (*(keyname+1) != 0) Sys->Printf (MSG_CONSOLE, "Bad key '%s'!\n", keyname);
  else if ((*keyname >= 'A' && *keyname <= 'Z') || strchr ("!@#$%^&*()_+", *keyname))
  {
    map->shift = 1;
    map->key = *keyname;
  }
  else if (*keyname >= 'a' && *keyname <= 'z')
  {
    if (map->shift) map->key = (*keyname)+'A'-'a';
    else map->key = *keyname;
  }
  else map->key = *keyname;
}

char* keyname (csKeyMap* map)
{
  static char buf[100];
  buf[0] = 0;
  if (map->need_status) strcat (buf, "status-");
  if (map->shift) strcat (buf, "shift-");
  if (map->ctrl) strcat (buf, "ctrl-");
  if (map->alt) strcat (buf, "alt-");
  switch (map->key)
  {
    case CSKEY_TAB: strcat (buf, "tab"); break;
    case ' ': strcat (buf, "space"); break;
    case CSKEY_ESC: strcat (buf, "esc"); break;
    case CSKEY_ENTER: strcat (buf, "enter"); break;
    case CSKEY_BACKSPACE: strcat (buf, "bs"); break;
    case CSKEY_UP: strcat (buf, "up"); break;
    case CSKEY_DOWN: strcat (buf, "down"); break;
    case CSKEY_RIGHT: strcat (buf, "right"); break;
    case CSKEY_LEFT: strcat (buf, "left"); break;
    case CSKEY_PGUP: strcat (buf, "pgup"); break;
    case CSKEY_PGDN: strcat (buf, "pgdn"); break;
    case CSKEY_HOME: strcat (buf, "home"); break;
    case CSKEY_END: strcat (buf, "end"); break;
    case CSKEY_INS: strcat (buf, "ins"); break;
    case CSKEY_DEL: strcat (buf, "del"); break;
    case CSKEY_F1: strcat (buf, "f1"); break;
    case CSKEY_F2: strcat (buf, "f2"); break;
    case CSKEY_F3: strcat (buf, "f3"); break;
    case CSKEY_F4: strcat (buf, "f4"); break;
    case CSKEY_F5: strcat (buf, "f5"); break;
    case CSKEY_F6: strcat (buf, "f6"); break;
    case CSKEY_F7: strcat (buf, "f7"); break;
    case CSKEY_F8: strcat (buf, "f8"); break;
    case CSKEY_F9: strcat (buf, "f9"); break;
    case CSKEY_F10: strcat (buf, "f10"); break;
    case CSKEY_F11: strcat (buf, "f11"); break;
    case CSKEY_F12: strcat (buf, "f12"); break;
    default:
    {
      char* s = strchr (buf, 0);
      *s++ = map->key;
      *s = 0;
    }
  }
  return buf;
}

csKeyMap* find_mapping (char* keyname)
{
  csKeyMap map;
  map_key (keyname, &map);

  csKeyMap* m = mapping;
  while (m)
  {
    if (map.key == m->key && map.shift == m->shift && map.ctrl == m->ctrl && map.alt == m->alt
    	&& map.need_status == m->need_status)
      return m;
    m = m->next;
  }
  return NULL;
}

void bind_key (char* arg)
{
  if (!arg)
  {
    csKeyMap* map = mapping;
    while (map)
    {
      Sys->Printf (MSG_CONSOLE, "Key '%s' bound to '%s'.\n", keyname (map), map->cmd);
      map = map->next;
    }
    return;
  }
  char* space = strchr (arg, ' ');
  if (space)
  {
    *space = 0;
    csKeyMap* map = find_mapping (arg);
    if (map)
    {
      CHK (delete [] map->cmd);
    }
    else
    {
      CHK (map = new csKeyMap ());
      map->next = mapping;
      map->prev = NULL;
      if (mapping) mapping->prev = map;
      mapping = map;
      map_key (arg, map);
    }
    CHK (map->cmd = new char [strlen (space+1)+1]);
    strcpy (map->cmd, space+1);
    *space = ' ';
  }
  else
  {
    csKeyMap* map = find_mapping (arg);
    if (map) Sys->Printf (MSG_CONSOLE, "Key bound to '%s'!\n", map->cmd);
    else Sys->Printf (MSG_CONSOLE, "Key not bound!\n");
  }
}

void free_keymap ()
{
  csKeyMap *prev, *cur = mapping;
  while (cur)
  {
    prev = cur;
    cur = cur->next;
    CHK (delete [] prev->cmd);
    CHK (delete prev);
  }
  mapping = NULL;
}

float safe_atof (char* arg)
{
  if (arg) return atof (arg);
  else return 1;
}

//--//--//--//--//--//--//--//--//--//--//-- Handle our additional commands --//
static bool CommandHandler (char *cmd, char *arg)
{
  if (!strcasecmp (cmd, "help"))
  {
    Command::perform (cmd, arg);
    Sys->Printf (MSG_CONSOLE, "-*- Additional commands -*-\n");
    Sys->Printf (MSG_CONSOLE, " bind, fclear, addlight, dellight, dellights\n");
    Sys->Printf (MSG_CONSOLE, " picklight, droplight, colldet, stats, hi, frustrum\n");
    Sys->Printf (MSG_CONSOLE, " fps, perftest, capture, coordshow, zbuf, freelook\n");
    Sys->Printf (MSG_CONSOLE, " map, fire, debug0, debug1, debug2, edges, p_alpha\n");
    Sys->Printf (MSG_CONSOLE, " addbot, delbot, addsprite, snd_play, snd_volume, s_fog, do_gravity\n");
    Sys->Printf (MSG_CONSOLE, " step_forward, step_backward, strafe_left, strafe_right\n");
    Sys->Printf (MSG_CONSOLE, " look_up, look_down, rotate_left, rotate_right, jump, move3d\n");
    Sys->Printf (MSG_CONSOLE, " i_forward, i_backward, i_left, i_right, i_up, i_down\n");
    Sys->Printf (MSG_CONSOLE, " i_rotleftc, i_rotleftw, i_rotrightc, i_rotrightw\n");
    Sys->Printf (MSG_CONSOLE, " i_rotleftx, i_rotleftz, i_rotrightx, i_rotrightz\n");
  }
  else if (!strcasecmp (cmd, "bind"))
    bind_key (arg);
  else if (!strcasecmp (cmd, "fclear"))
    Command::change_boolean (arg, &Sys->do_clear, "fclear");
  else if (!strcasecmp (cmd, "fps"))
    Command::change_boolean (arg, &Sys->do_fps, "fps");
  else if (!strcasecmp (cmd, "edges"))
    Command::change_boolean (arg, &Sys->do_edges, "do_edges");
  else if (!strcasecmp (cmd, "do_gravity"))
    Command::change_boolean (arg, &Sys->do_gravity, "do_gravity");
  else if (!strcasecmp (cmd, "inverse_mouse"))
    Command::change_boolean (arg, &Sys->inverse_mouse, "inverse_mouse");
  else if (!strcasecmp (cmd, "colldet"))
    Command::change_boolean (arg, &Sys->do_cd, "colldet");
  else if (!strcasecmp (cmd, "frustrum"))
    Command::change_boolean (arg, &Sys->do_light_frust, "frustrum");
  else if (!strcasecmp (cmd, "zbuf"))
    Command::change_boolean (arg, &Sys->do_show_z, "zbuf");
  else if (!strcasecmp (cmd, "move3d"))
    Command::change_boolean (arg, &Sys->move_3d, "move3d");
  else if (!strcasecmp (cmd, "freelook"))
  {
    Command::change_boolean (arg, &Sys->do_freelook, "freelook");
    if (Sys->do_freelook)
      System->piG2D->SetMousePosition (FRAME_WIDTH / 2, FRAME_HEIGHT / 2);
  }
  else if (!strcasecmp (cmd, "stats"))
  {
    Command::change_boolean (arg, &Sys->do_stats, "stats");
    if (Sys->do_stats) Sys->do_show_coord = false;
  }
  else if (!strcasecmp (cmd, "coordshow"))
  {
    Command::change_boolean (arg, &Sys->do_show_coord, "coordshow");
    if (Sys->do_show_coord) Sys->do_stats = false;
  }
  else if (!strcasecmp (cmd, "hi"))
  {
    csPolygon3D* hi = arg ? Sys->view->GetCamera ()->GetSector ()->GetPolygon (arg) : (csPolygon3D*)NULL;
    if (hi) Sys->Printf (MSG_CONSOLE, "Hilighting polygon: '%s'\n", arg);
    else Sys->Printf (MSG_CONSOLE, "Disabled hilighting.\n");
    Sys->selected_polygon = hi;
  }
  else if (!strcasecmp (cmd, "p_alpha"))
  {
    csPolygon3D* hi = Sys->selected_polygon;
    if (hi)
    {
      if (hi->GetPortal ())
      {
        int a = hi->GetAlpha ();
        Command::change_int (arg, &a, "portal alpha", 0, 100);
	hi->SetAlpha (a);
      }
      else Sys->Printf (MSG_CONSOLE, "Only for portals!\n");
    }
    else Sys->Printf (MSG_CONSOLE, "No polygon selected!\n");
  }
  else if (!strcasecmp (cmd, "s_fog"))
  {
    csFog& f = Sys->view->GetCamera ()->GetSector ()->GetFog ();
    if (!arg)
    {
      Sys->Printf (MSG_CONSOLE, "Fog in current sector (%f,%f,%f) density=%f\n",
      	f.red, f.green, f.blue, f.density);
    }
    else
    {
      float r, g, b, dens;
      if (ScanStr (arg, "%f,%f,%f,%f", &r, &g, &b, &dens) != 4)
      {
        Sys->Printf (MSG_CONSOLE, "Expected r,g,b,density. Got something else!\n");
        return false;
      }
      f.enabled = true;
      f.density = dens;
      f.red = r;
      f.green = g;
      f.blue = b;
    }
  }
  else if (!strcasecmp (cmd, "capture"))
    CaptureScreen ();
  else if (!strcasecmp (cmd, "perftest"))
    perf_test ();
  else if (!strcasecmp (cmd, "debug0"))
  {
    Sys->Printf (MSG_CONSOLE, "No debug0 implementation in this version.\n");
  }
  else if (!strcasecmp (cmd, "debug1"))
  {
    Sys->Printf (MSG_CONSOLE, "No debug1 implementation in this version.\n");
  }
  else if (!strcasecmp (cmd, "debug2"))
  {
    Sys->Printf (MSG_CONSOLE, "No debug2 implementation in this version.\n");
  }
  else if (!strcasecmp (cmd, "strafe_left"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_left (.1, false, false); }
    else Sys->strafe (-1*f,0);
  }
  else if (!strcasecmp (cmd, "strafe_right"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_right (.1, false, false); }
    else Sys->strafe (1*f,0);
  }
  else if (!strcasecmp (cmd, "step_forward"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_forward (.1, false, false); }
    else Sys->step (1*f,0);
  }
  else if (!strcasecmp (cmd, "step_backward"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_backward (.1, false, false); }
    else Sys->step (-1*f,0);
  }
  else if (!strcasecmp (cmd, "rotate_left"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_rot_left_camera (.1, false, false); }
    else Sys->rotate (-1*f,0);
  }
  else if (!strcasecmp (cmd, "rotate_right"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_rot_right_camera (.1, false, false); }
    else Sys->rotate (1*f,0);
  }
  else if (!strcasecmp (cmd, "look_up"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_rot_right_xaxis (.1, false, false); }
    else Sys->look (-1*f,0);
  }
  else if (!strcasecmp (cmd, "look_down"))
  {
    float f = safe_atof (arg);
    if (Sys->move_3d || Sys->map_mode) { if (f) Sys->imm_rot_left_xaxis (.1, false, false); }
    else Sys->look (1*f,0);
  }
  else if (!strcasecmp (cmd, "jump"))
  {
    if (Sys->do_gravity && Sys->on_ground)
      Sys->velocity.y=0.08;
  }
  else if (!strcasecmp (cmd, "i_forward"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_forward (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_backward"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_backward (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_left"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_left (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_right"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_right (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_up"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_up (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_down"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_down (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotleftc"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_left_camera (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotleftw"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_left_world (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotrightc"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_right_camera (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotrightw"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_right_world (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotleftx"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_left_xaxis (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotleftz"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_left_zaxis (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotrightx"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_right_xaxis (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "i_rotrightz"))
  {
    int slow = 0, fast = 0; if (arg) ScanStr (arg, "%d,%d", &slow, &fast);
    Sys->imm_rot_right_zaxis (.1, (bool)slow, (bool)fast);
  }
  else if (!strcasecmp (cmd, "fire"))
  {
    csVector3 dir (0, 0, 0);
    csVector3 pos = Sys->view->GetCamera ()->Camera2World (dir);
    float r, g, b;
    RandomColor (r, g, b);
    CHK (csDynLight* dyn = new csDynLight (pos.x, pos.y, pos.z, 4, r, g, b));
    Sys->view->GetWorld ()->AddDynLight (dyn);
    dyn->SetSector (Sys->view->GetCamera ()->GetSector ());
    dyn->Setup ();
    CHK (MissileStruct* ms = new MissileStruct);
    Sys->piSound->CreateSource (&ms->snd, Sys->wMissile_whoosh);
    if (ms->snd)
    {
      ms->snd->SetPosition (pos.x, pos.y, pos.z);
      ms->snd->PlaySource ();
    }
    ms->type = DYN_TYPE_MISSILE;
    ms->dir = (csOrthoTransform)*(Sys->view->GetCamera ());
    ms->sprite = NULL;
    CHK(csDataObject* msdata = new csDataObject(ms));
    dyn->ObjAdd(msdata);

    char misname[10];
    sprintf (misname, "missile%d", ((rand () >> 3) & 1)+1);

    csSpriteTemplate* tmpl = Sys->view->GetWorld ()->GetSpriteTemplate (misname, true);
    if (!tmpl)
      Sys->Printf (MSG_CONSOLE, "Could not find '%s' sprite template!\n", misname);
    else
    {
      csSprite3D* sp = tmpl->NewSprite ();
      csNameObject::AddName (*sp, "missile");
      Sys->view->GetWorld ()->sprites.Push (sp);
      sp->MoveToSector (Sys->view->GetCamera ()->GetSector ());
      ms->sprite = sp;
      sp->SetMove (pos);
      csMatrix3 m = ms->dir.GetT2O ();
      sp->SetTransform (m);
      move_sprite (sp, Sys->view->GetCamera ()->GetSector (), pos);
    }
  }
  else if (!strcasecmp (cmd, "addsprite"))
  {
    char name[100];
    float size;
    if (arg) ScanStr (arg, "%s,%f", name, &size);
    else { *name = 0; size = 1; }
    add_sprite (name, Sys->view->GetCamera ()->GetSector (), Sys->view->GetCamera ()->GetOrigin (), size);
  }
  else if (!strcasecmp (cmd, "addbot"))
  {
    float radius = 0;
    if (arg) ScanStr (arg, "%f", &radius);
    add_bot (2, Sys->view->GetCamera ()->GetSector (), Sys->view->GetCamera ()->GetOrigin (), radius);
  }
  else if (!strcasecmp (cmd, "delbot"))
  {
    if (first_bot)
    {
      Bot* bot = first_bot;
      first_bot = bot->next;
      Sys->view->GetWorld ()->RemoveSprite (bot);
    }
  }
  else if (!strcasecmp (cmd, "addlight"))
  {
    csVector3 dir (0,0,0);
    csVector3 pos = Sys->view->GetCamera ()->Camera2World (dir);
    csDynLight* dyn;

    bool rnd;
    float r, g, b, radius, thing_shadows;
    if (arg && ScanStr (arg, "%f,%f,%f,%f,%d", &r, &g, &b, &radius, &thing_shadows) == 5)
    {
      CHK (dyn = new csDynLight (pos.x, pos.y, pos.z, radius, r, g, b));
      if (thing_shadows) dyn->SetFlags (CS_LIGHT_THINGSHADOWS, CS_LIGHT_THINGSHADOWS);
      rnd = false;
    }
    else
    {
      CHK (dyn = new csDynLight (pos.x, pos.y, pos.z, 6, 1, 1, 1));
      rnd = true;
    }
    Sys->view->GetWorld ()->AddDynLight (dyn);
    dyn->SetSector (Sys->view->GetCamera ()->GetSector ());
    dyn->Setup ();
    if (rnd)
    {
      CHK (RandomLight* rl = new RandomLight);
      rl->type = DYN_TYPE_RANDOM;
      rl->dyn_move_dir = .2;
      rl->dyn_move = 0;
      rl->dyn_r1 = rl->dyn_g1 = rl->dyn_b1 = 1;
      CHK(csDataObject* rldata = new csDataObject(rl));
      dyn->ObjAdd(rldata);
    }
    Sys->Printf (MSG_CONSOLE, "Dynamic light added.\n");
  }
  else if (!strcasecmp (cmd, "dellight"))
  {
    csDynLight* dyn;
    if ((dyn = Sys->view->GetWorld ()->GetFirstDynLight ()) != NULL)
    {
      Sys->view->GetWorld ()->RemoveDynLight (dyn);
      CHK (delete dyn);
      Sys->Printf (MSG_CONSOLE, "Dynamic light deleted.\n");
    }
  }
  else if (!strcasecmp (cmd, "dellights"))
  {
    csDynLight* dyn;
    while ((dyn = Sys->view->GetWorld ()->GetFirstDynLight ()) != NULL)
    {
      Sys->view->GetWorld ()->RemoveDynLight (dyn);
      CHK (delete dyn);
    }
    Sys->Printf (MSG_CONSOLE, "All dynamic lights deleted.\n");
  }
  else if (!strcasecmp (cmd, "picklight"))
  {
#   if 0
    pickup_light = Sys->view->GetWorld ()->GetFirstFltLight ();
    if (pickup_light) Sys->Printf (MSG_CONSOLE, "Floating light taken.\n");
    else Sys->Printf (MSG_CONSOLE, "No floating light to take.\n");
#   endif
  }
  else if (!strcasecmp (cmd, "droplight"))
  {
#   if 0
    if (pickup_light) Sys->Printf (MSG_CONSOLE, "Floating light dropped.\n");
    else Sys->Printf (MSG_CONSOLE, "No floating light to drop.\n");
    pickup_light = NULL;
#   endif
  }
  else if (!strcasecmp (cmd, "map"))
  {
    char* choices[4] = { "off", "overlay", "on", NULL };
    Command::change_choice (arg, &Sys->map_mode, "map", choices, 3);
  }
  else if (!strcasecmp (cmd, "snd_play"))
  {
    csSoundBuffer *sb =
         csSoundBufferObject::GetSound(*(Sys->view->GetWorld()), arg);
    if (sb)
      Sys->piSound->PlayEphemeral(sb);
    else Sys->Printf (MSG_CONSOLE, "Sound '%s' not found!\n", arg);
  }
  else if (!strcasecmp (cmd, "snd_volume"))
  {
    float vol;
    Sys->piSound->GetVolume (&vol);
    Command::change_float (arg, &vol, "snd_volume", 0.0, 1.0);
    Sys->piSound->SetVolume (vol);
  }
  else
    return false;
  return true;
}

//-----------------------------------------------------------------------------

char WalkTest::world_file[100];
bool WalkTest::move_3d = false;

WalkTest::WalkTest () : SysSystemDriver (), pos (0, 0, 0), velocity (0, 0, 0)
{
  Command::ExtraHandler = CommandHandler;
  auto_script = NULL;
  layer = NULL;
  view = NULL;
  infinite_maze = NULL;
  wMissile_boom = NULL;
  wMissile_whoosh = NULL;
  cslogo = NULL;

  wf = NULL;
  map_mode = MAP_OFF;
  do_fps = true;
  do_stats = false;
  do_clear = false;
  do_edges = false;
  do_light_frust = false;
  do_show_coord = false;
  busy_perf_test = false;
  do_show_z = false;
  do_infinite = false;
  do_cd = true;
  do_freelook = false;
  player_spawned = false;
  do_gravity = true;
  inverse_mouse = false;
  selected_light = NULL;
  selected_polygon = NULL;

  velocity.Set (0, 0, 0);
  angle.Set (0, 0, 0);
  angle_velocity.Set (0, 0, 0);

//  pl=new PhysicsLibrary;

  timeFPS = 0.0;
}

WalkTest::~WalkTest ()
{
  CHK (delete wf);
  CHK (delete [] auto_script);
  CHK (delete layer);
  CHK (delete view);
  CHK (delete infinite_maze);
  CHK (delete cslogo);
  CHK (delete world);
//  CHK (delete pl);
}

bool WalkTest::ParseArg (int argc, char* argv[], int& i)
{
  if (argv[i][0] != '-')
  {
    strcpy (world_file, argv[i]);
  }
  if (strcasecmp ("-clear", argv[i]) == 0)
  {
    do_clear = true;
    Sys->Printf (MSG_INITIALIZATION, "Screen will be cleared every frame.\n");
  }
  else if (strcasecmp ("-noclear", argv[i]) == 0)
  {
    do_clear = false;
    Sys->Printf (MSG_INITIALIZATION, "Screen will not be cleared every frame.\n");
  }
  else if (strcasecmp ("-stats", argv[i]) == 0)
  {
    do_stats = true;
    Sys->Printf (MSG_INITIALIZATION, "Statistics enabled.\n");
  }
  else if (strcasecmp ("-nostats", argv[i]) == 0)
  {
    do_stats = false;
    Sys->Printf (MSG_INITIALIZATION, "Statistics disabled.\n");
  }
  else if (strcasecmp ("-fps", argv[i]) == 0)
  {
    do_fps = true;
    Sys->Printf (MSG_INITIALIZATION, "Frame Per Second enabled.\n");
  }
  else if (strcasecmp ("-nofps", argv[i]) == 0)
  {
    do_fps = false;
    Sys->Printf (MSG_INITIALIZATION, "Frame Per Second disabled.\n");
  }
  else if (strcasecmp ("-infinite", argv[i]) == 0)
  {
    do_infinite = true;
  }
  else if (strcasecmp ("-bots", argv[i]) == 0)
  {
    do_bots = true;
  }
  else if (strcasecmp ("-colldet", argv[i]) == 0)
  {
    do_cd = true;
    Sys->Printf (MSG_INITIALIZATION, "Enabled collision detection system.\n");
  }
  else if (strcasecmp ("-nocolldet", argv[i]) == 0)
  {
    do_cd = false;
    Sys->Printf (MSG_INITIALIZATION, "Disabled collision detection system.\n");
  }
  else if (strcasecmp ("-exec", argv[i]) == 0)
  {
    i++;
    if (i < argc)
    {
      CHK (delete [] auto_script);
      CHK (auto_script = new char [strlen (argv[i])+1]);
      strcpy (auto_script, argv[i]);
    }
  }
  else
    return SysSystemDriver::ParseArg (argc, argv, i);
  return true;
}

void WalkTest::Help ()
{
  SysSystemDriver::Help ();
  Sys->Printf (MSG_STDOUT, "  -exec <filename>   execute given script at startup\n");
  Sys->Printf (MSG_STDOUT, "  -clear/noclear     clear display every frame (default '%sclear')\n", do_clear ? "" : "no");
  Sys->Printf (MSG_STDOUT, "  -stats/nostats     statistics (default '%sstats')\n", do_stats ? "" : "no");
  Sys->Printf (MSG_STDOUT, "  -fps/nofps         frame rate printing (default '%sfps')\n", do_fps ? "" : "no");
  Sys->Printf (MSG_STDOUT, "  -colldet/nocolldet collision detection system (default '%scolldet')\n", do_cd ? "" : "no");
  Sys->Printf (MSG_STDOUT, "  -infinite          special infinite level generation (ignores world file!)\n");
  Sys->Printf (MSG_STDOUT, "  -bots              allow random generation of bots\n");
  Sys->Printf (MSG_STDOUT, "  <filename>         load world file from <filename> (default '%s')\n", world_file);
}

/*------------------------------------------------------------------
 * The following handle_key_... routines are general movement
 * routines that are called by do_update() for the new movement
 * system and by do_keypress() for the old movement system (see
 * system.h for an explanation of the difference between the two
 * systems).
 *------------------------------------------------------------------*/

extern csCamera c;
extern WalkTest* Sys;

void WalkTest::strafe (float speed,int keep_old)
{
  if (move_3d || map_mode) return;
  static bool pressed = false;
  static float strafe_speed = 0;
  static long start_time = Time ();

  long cur_time = Time ();
  if (!keep_old)
  {
    bool new_pressed = fabs (speed) > 0.001;
    if (new_pressed != pressed)
    {
      pressed = new_pressed;
      strafe_speed = speed * 0.007;
      start_time = cur_time - 100;
    }
  }

  while ((cur_time - start_time) >= 100)
  {
    if (pressed)
    {
      // accelerate
      if (fabs (velocity.x) < 0.1)
        velocity.x += strafe_speed;
    }
    else
    {
      // brake!
      if (velocity.x > 0.014)
        velocity.x -= 0.014;
      else if (velocity.x < -0.014)
        velocity.x += 0.014;
      else
        velocity.x = 0;
    }
    start_time += 100;
  }
}

void WalkTest::step (float speed,int keep_old)
{
  if (move_3d || map_mode) return;

  static bool pressed = false;
  static float step_speed = 0;
  static long start_time = Time ();

  long cur_time = Time ();
  if (!keep_old)
  {
    bool new_pressed = fabs (speed) > 0.001;
    if (new_pressed != pressed)
    {
      pressed = new_pressed;
      step_speed = speed * 0.007;
      start_time = cur_time - 100;
    }
  }

  while ((cur_time - start_time) >= 100)
  {
    if (pressed)
    {
      // accelerate
      if (fabs (velocity.z) < 0.1)
        velocity.z += step_speed;
    }
    else
    {
      // brake!
      if (velocity.z > 0.014)
        velocity.z -= 0.014;
      else if (velocity.z < -0.014)
        velocity.z += 0.014;
      else
        velocity.z = 0;
    }
    start_time += 100;
  }
}

void WalkTest::rotate (float speed,int keep_old)
{
  if (move_3d || map_mode) return;

  static bool pressed = false;
  static float angle_accel = 0;
  static long start_time = Time ();

  long cur_time = Time ();
  if (!keep_old)
  {
    bool new_pressed = fabs (speed) > 0.001;
    if (new_pressed != pressed)
    {
      pressed = new_pressed;
      angle_accel = speed * 0.005;
      start_time = cur_time - 100;
    }
  }

  while ((cur_time - start_time) >= 100)
  {
    if (pressed)
    {
      // accelerate rotation
      if (fabs (angle_velocity.y) < 0.03)
        angle_velocity.y += angle_accel;
    }
    else
    {
      // brake!
      if (angle_velocity.y > 0.015)
        angle_velocity.y -= 0.015;
      else if (angle_velocity.y < -0.015)
        angle_velocity.y += 0.015;
      else
        angle_velocity.y = 0;
    }
    start_time += 100;
  }
}

void WalkTest::look (float speed,int keep_old)
{
  if (move_3d || map_mode) return;
  static float step_speed = 0;
  if (!keep_old)
    step_speed = speed*0.028;
  if (fabs (angle.x+step_speed) <= (355.0/113.0/4))
    angle.x += step_speed;
}

void WalkTest::imm_forward (float speed, bool slow, bool fast)
{
  if (map_mode) { wf->KeyUp (speed, slow, fast); return; }
  if (slow) Sys->view->GetCamera ()->Move (speed*.01*VEC_FORWARD);
  else if (fast) Sys->view->GetCamera ()->Move (speed*1.2*VEC_FORWARD);
  else Sys->view->GetCamera ()->Move (speed*.6*VEC_FORWARD);
}

void WalkTest::imm_backward (float speed, bool slow, bool fast)
{
  if (map_mode) { wf->KeyDown (speed, slow, fast); return; }
  if (slow) Sys->view->GetCamera ()->Move (speed*.01*VEC_BACKWARD);
  else if (fast) Sys->view->GetCamera ()->Move (speed*1.2*VEC_BACKWARD);
  else Sys->view->GetCamera ()->Move (speed*.6*VEC_BACKWARD);
}

void WalkTest::imm_left (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->Move (speed*.01*VEC_LEFT);
  else if (fast) Sys->view->GetCamera ()->Move (speed*1.2*VEC_LEFT);
  else Sys->view->GetCamera ()->Move (speed*.6*VEC_LEFT);
}

void WalkTest::imm_right (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->Move (speed*.01*VEC_RIGHT);
  else if (fast) Sys->view->GetCamera ()->Move (speed*1.2*VEC_RIGHT);
  else Sys->view->GetCamera ()->Move (speed*.6*VEC_RIGHT);
}

void WalkTest::imm_up (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->Move (speed*.01*VEC_UP);
  else if (fast) Sys->view->GetCamera ()->Move (speed*1.2*VEC_UP);
  else Sys->view->GetCamera ()->Move (speed*.6*VEC_UP);
}

void WalkTest::imm_down (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->Move (speed*.01*VEC_DOWN);
  else if (fast) Sys->view->GetCamera ()->Move (speed*1.2*VEC_DOWN);
  else Sys->view->GetCamera ()->Move (speed*.6*VEC_DOWN);
}

void WalkTest::imm_rot_left_camera (float speed, bool slow, bool fast)
{
  if (map_mode) { wf->KeyLeft (speed, slow, fast); return; }
  if (slow) Sys->view->GetCamera ()->Rotate (VEC_ROT_LEFT, speed*.005);
  else if (fast) Sys->view->GetCamera ()->Rotate (VEC_ROT_LEFT, speed*.2);
  else Sys->view->GetCamera ()->Rotate (VEC_ROT_LEFT, speed*.1);
}

void WalkTest::imm_rot_left_world (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->RotateWorld (VEC_ROT_LEFT, speed*.005);
  else if (fast) Sys->view->GetCamera ()->RotateWorld (VEC_ROT_LEFT, speed*.2);
  else Sys->view->GetCamera ()->RotateWorld (VEC_ROT_LEFT, speed*.1);
}

void WalkTest::imm_rot_right_camera (float speed, bool slow, bool fast)
{
  if (map_mode) { wf->KeyRight (speed, slow, fast); return; }
  if (slow) Sys->view->GetCamera ()->Rotate (VEC_ROT_RIGHT, speed*.005);
  else if (fast) Sys->view->GetCamera ()->Rotate (VEC_ROT_RIGHT, speed*.2);
  else Sys->view->GetCamera ()->Rotate (VEC_ROT_RIGHT, speed*.1);
}

void WalkTest::imm_rot_right_world (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->RotateWorld (VEC_ROT_RIGHT, speed*.005);
  else if (fast) Sys->view->GetCamera ()->RotateWorld (VEC_ROT_RIGHT, speed*.2);
  else Sys->view->GetCamera ()->RotateWorld (VEC_ROT_RIGHT, speed*.1);
}

void WalkTest::imm_rot_left_xaxis (float speed, bool slow, bool fast)
{
  if (map_mode) { wf->KeyPgDn (speed, slow, fast); return; }
  if (slow) Sys->view->GetCamera ()->Rotate (VEC_TILT_DOWN, speed*.005);
  else if (fast) Sys->view->GetCamera ()->Rotate (VEC_TILT_DOWN, speed*.2);
  else Sys->view->GetCamera ()->Rotate (VEC_TILT_DOWN, speed*.1);
}

void WalkTest::imm_rot_right_xaxis (float speed, bool slow, bool fast)
{
  if (map_mode) { wf->KeyPgUp (speed, slow, fast); return; }
  if (slow) Sys->view->GetCamera ()->Rotate (VEC_TILT_UP, speed*.005);
  else if (fast) Sys->view->GetCamera ()->Rotate (VEC_TILT_UP, speed*.2);
  else Sys->view->GetCamera ()->Rotate (VEC_TILT_UP, speed*.1);
}

void WalkTest::imm_rot_left_zaxis (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->Rotate (VEC_TILT_LEFT, speed*.005);
  else if (fast) Sys->view->GetCamera ()->Rotate (VEC_TILT_LEFT, speed*.2);
  else Sys->view->GetCamera ()->Rotate (VEC_TILT_LEFT, speed*.1);
}

void WalkTest::imm_rot_right_zaxis (float speed, bool slow, bool fast)
{
  if (map_mode) return;
  if (slow) Sys->view->GetCamera ()->Rotate (VEC_TILT_RIGHT, speed*.005);
  else if (fast) Sys->view->GetCamera ()->Rotate (VEC_TILT_RIGHT, speed*.2);
  else Sys->view->GetCamera ()->Rotate (VEC_TILT_RIGHT, speed*.1);
}

void WalkTest::eatkeypress (int status,int key, bool shift, bool alt, bool ctrl)
{
  if (System->Console->IsActive ()&&status)
    ((SimpleConsole *)System->Console)->AddChar (key);
  else switch (key)
  {
    case CSKEY_TAB:
      if(status)
        System->Console->Show ();
      break;

    default:
      {
	csKeyMap *m = mapping;
        while (m)
        {
          if (key == m->key && shift == m->shift
           && alt == m->alt && ctrl == m->ctrl)
          {
            char buf[256];
            if(m->need_status)
            {
              m->is_on=status;
              sprintf(buf,"%s %d",m->cmd,status);
            }
            else
            {
              if(!status)
                break;
              sprintf(buf,"%s",m->cmd);
            }
            Command::perform_line(buf);
          }
          else
          if(!status&&m->is_on&&m->need_status)
          {
            if(key==m->key||(shift!=m->shift||alt!=m->alt||ctrl!=m->ctrl))
            {
              char buf[256];
              sprintf(buf,"%s 0",m->cmd);
              Command::perform_line(buf);
              m->is_on=0;
            }
          }
	  m = m->next;
        }
      }
      break;
  }
}

void WalkTest::NextFrame (long elapsed_time, long current_time)
{
  SysSystemDriver::NextFrame (elapsed_time, current_time);

  // Record the first time this routine is called.
  if(do_bots)
  {
    static long first_time = -1;
    static long next_bot_at;
    if (first_time == -1) { first_time = current_time; next_bot_at = current_time+1000*10; }
    if (current_time > next_bot_at)
    {
      add_bot (2, view->GetCamera ()->GetSector (), view->GetCamera ()->GetOrigin (), 0);
      next_bot_at = current_time+1000*10;
    }
  }
#if 0 // don't like it. Yet ;)
  {
    static long first_time = -1;
    static long next_bot_at;
    if (first_time == -1) { first_time = current_time; next_bot_at = current_time+1000*3; }
    if (current_time > next_bot_at)
    {
      csSpriteTemplate* tmpl = Sys->view->GetWorld ()->GetSpriteTemplate ("bot", true);
      if (tmpl)
      {
        csSprite3D *sp = tmpl->NewSprite ();
        sp->MoveToSector (view->GetCamera()->GetSector());
/*      csMatrix3 m; m.Identity ();
        sp->SetTransform (m);
        sp->SetMove (view->GetCamera()->GetOrigin());*/
        csVector3 forward=view->GetCamera()->GetW2C().Col3();
        pl->AddObject (sp,view->GetCamera ()->GetOrigin ()+forward*3); //(2, view->GetCamera ()->GetSector (), view->GetCamera ()->GetOrigin (), 0);
      }
      next_bot_at = current_time+1000*3;
    }
  }

  pl->MakeStep ();
#endif
  if (!System->Console->IsActive ())
  {
    int alt,shift,ctrl;
    float speed = 1;

    alt = Keyboard->Key.alt;
    ctrl = Keyboard->Key.ctrl;
    shift = Keyboard->Key.shift;
    if (ctrl) speed = .5;
    if (shift) speed = 2;

    /// Act as usual...
    strafe (0,1); look (0,1); step (0,1); rotate (0,1);

    ISoundListener *sndListener;
    Sys->piSound->GetListener(&sndListener);
    if(sndListener)
    {
      // take position/direction from view->GetCamera ()
      csVector3 v = view->GetCamera ()->GetOrigin ();
      csMatrix3 m = view->GetCamera ()->GetC2W();
      csVector3 f = m.Col3();
      csVector3 t = m.Col2();
      sndListener->SetPosition(v.x, v.y, v.z);
      sndListener->SetDirection(f.x,f.y,f.z,t.x,t.y,t.z);
      //sndListener->SetDirection(...);
    }
  } /* endif */

  static bool move_forward = false;
  if (move_forward) step (1, 0);

  csEvent *Event;
  while ((Event = Sys->EventQueue->Get ()) != NULL)
  {
    switch (Event->Type)
    {
      case csevKeyDown:
        eatkeypress (1,Event->Key.Code,
        		(Event->Key.ShiftKeys & CSMASK_SHIFT) != 0,
          	(Event->Key.ShiftKeys & CSMASK_ALT) != 0,
        		(Event->Key.ShiftKeys & CSMASK_CTRL) != 0);
        break;
      case csevKeyUp:
        eatkeypress (0,Event->Key.Code,
        		(Event->Key.ShiftKeys & CSMASK_SHIFT) != 0,
          	(Event->Key.ShiftKeys & CSMASK_ALT) != 0,
        		(Event->Key.ShiftKeys & CSMASK_CTRL) != 0);
        break;
      case csevBroadcast:
        if ((Event->Command.Code == cscmdFocusChanged)
         && (Event->Command.Info == NULL))
          memset (&Keyboard->Key, 0, sizeof (Keyboard->Key));
        break;
      case csevMouseDown:
        if (Event->Mouse.Button == 1)
	        move_forward = true;
        else
        if (Event->Mouse.Button == 2)
        {
	  unsigned long real_zb;
          unsigned long* zb = &real_zb;
	  System->piG3D->GetZBufPoint(Event->Mouse.x, Event->Mouse.y, &zb);

          if (zb)
          {
	    csVector3 v;
	    v.z = 1. / (((float)*zb)/(256.*65536.));
	    v.x = (Event->Mouse.x-FRAME_WIDTH/2) * v.z / csCamera::aspect;
	    v.y = (FRAME_HEIGHT-1-Event->Mouse.y-FRAME_HEIGHT/2) * v.z / csCamera::aspect;
	    csVector3 vw = view->GetCamera ()->Camera2World (v);

            Sys->Printf (MSG_CONSOLE, "LMB down : z_buf=%ld cam:(%f,%f,%f) world:(%f,%f,%f)\n", zb, v.x, v.y, v.z, vw.x, vw.y, vw.z);
            Sys->Printf (MSG_DEBUG_0, "LMB down : z_buf=%ld cam:(%f,%f,%f) world:(%f,%f,%f)\n", zb, v.x, v.y, v.z, vw.x, vw.y, vw.z);
	  }

	  extern csVector2 coord_check_vector;
          coord_check_vector.x = Event->Mouse.x;
          coord_check_vector.y = FRAME_HEIGHT-Event->Mouse.y;
	  extern bool check_poly, check_light;
	  extern void select_object (csRenderView* rview, int type, void* entity);
	  check_poly = check_light = true;
  	  view->GetWorld ()->DrawFunc (System->piG3D, view->GetCamera (), view->GetClipper (), select_object);
        }
        break;
      case csevMouseMove:
	// additional command by Leslie Saputra -> freelook mode.
	{
	  static bool first_time = true;
	  if (do_freelook)
	  {
	    int last_x, last_y;
	    last_x = Event->Mouse.x;
	    last_y = Event->Mouse.y;

            System->piG2D->SetMousePosition (FRAME_WIDTH / 2, FRAME_HEIGHT / 2);
	    if (!first_time)
	    {
            /*
	      if(move_3d)
	        view->GetCamera ()->Rotate (VEC_ROT_RIGHT, ((float)( last_x - (FRAME_WIDTH / 2) )) / (FRAME_WIDTH*2) );
	      else
	        view->GetCamera ()->RotateWorld (VEC_ROT_RIGHT, ((float)( last_x - (FRAME_WIDTH / 2) )) / (FRAME_WIDTH*2) );
	      view->GetCamera ()->Rotate (VEC_TILT_UP, -((float)( last_y - (FRAME_HEIGHT / 2) )) / (FRAME_HEIGHT*2) );
            */

              this->angle.y+=((float)(last_x - (FRAME_WIDTH / 2) )) / (FRAME_WIDTH*2);
              this->angle.x+=((float)(last_y - (FRAME_HEIGHT / 2) )) / (FRAME_HEIGHT*2)*(1-2*(int)inverse_mouse);
	    }
	    else
              first_time = false;
	  }
	  else
            first_time = true;
	}
      break;
      case csevMouseUp:
        if (Event->Mouse.Button == 1)
	        move_forward = false;
        break;
    }
    CHK(delete Event);
  }

  if(first_bot)
  {
    Bot* bot = first_bot;
    while (bot)
    {
      bot->move (elapsed_time);
      bot = bot->next;
    }
  }

  PrepareFrame (elapsed_time, current_time);
  DrawFrame (elapsed_time, current_time);

  // Execute one line from the script.
  if (!busy_perf_test)
  {
    char buf[256];
    if (Command::get_script_line (buf, 255)) Command::perform_line (buf);
  }
}
