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

#ifndef RVIEW_H
#define RVIEW_H

#include "csgeom/math3d.h"
#include "csgeom/frustrum.h"
#include "csengine/camera.h"

class csClipper;
class csMatrix3;
class csVector3;
class csLight;
interface IGraphics3D;
interface IGraphics2D;

/**
 * This structure represents all information needed for drawing
 * a scene. It is closely related to the csCamera class and modified
 * while rendering according to portals/warping portals and such.
 */
class csRenderView : public csCamera
{
public:
  /// The 2D polygon describing how everything drawn inside should be clipped.
  csClipper* view;

  /// The 3D graphics subsystem used for drawing.
  IGraphics3D* g3d;
  /// The 2D graphics subsystem used for drawing.
  IGraphics2D* g2d;

  /**
   * This variable holds the plane of the portal through which the camera
   * is looking.<p>
   */
  csPlane clip_plane;

  /**
   * If true then we clip all objects to 'clip_plane'. In principle
   * one should always clip to 'clip_plane'. However, in many cases
   * this is not required because portals mostly arrive in at the
   * boundaries of a sector so there can actually be no objects
   * after the portal plane. But it is possible that portals arive
   * somewhere in the middle of a sector (for example with BSP sectors
   * or with Things containing portals). In that case this variable
   * will be set to true and clipping to 'clip_plane' is required.
   */
  bool do_clip_plane;

  ///
  csRenderView () : csCamera (), view (NULL), g3d (NULL), g2d (NULL), do_clip_plane (false) {}
  ///
  csRenderView (const csCamera& c) : csCamera (c), view (NULL), g3d (NULL), g2d (NULL), do_clip_plane (false) {}
  ///
  csRenderView (const csCamera& c, csClipper* v, IGraphics3D* ig3d, IGraphics2D* ig2d) :
   csCamera (c), view (v), g3d (ig3d), g2d (ig2d), do_clip_plane (false) {}

  ///
  void SetView (csClipper* v) { view = v; }
  ///
  void SetClipPlane (csPlane& p) { clip_plane = p; }
};

/**
 * A shadow polygon. Objects of this type are collected by the
 * lighting routines. Every polygon of every thing generates a
 * shadow polygon which follows the csLightView structure below.
 */
class csShadowPatch
{
public:
  ///
  csShadowPatch* next;

  /**
   * The shadow frustrum.
   */
  csVector3* frustrum;
  ///
  int num_frustrum;

  csShadowPatch () : next (NULL), frustrum (NULL), num_frustrum (0) { }

  ~csShadowPatch ()
  {
    CHK (delete [] frustrum);
  }
};

/**
 * This structure represents all information needed for static lighting.
 * It is the basic information block that is passed between the various
 * static lighting routines.
 */
class csLightView
{
public:
  /// The light that we're processing.
  csLight* l;

  /**
   * The center of the light. This may differ from the actual center
   * of 'l' because space warping may have relocated it. Routines should
   * always use this center.
   */
  csVector3 center;

  /**
   * The current color of the light. Initially this is the same as the
   * light in csStatLight but portals may change this.
   */
  float r, g, b;

  /// If space is mirrored.
  bool mirror;

  /**
   * If this structure is used for dynamic light frustrum calculation
   * then this flag is true.
   */
  bool dynamic;

  /**
   * If only gouroud shading should be updated then this flag is true.
   */
  bool gouroud_only;

  /**
   * If 'true' then the gouroud vertices need to be initialized (set to
   * black) first. Only the parent PolygonSet of a polygon can know this
   * because it is calculated using the current_light_frame_number.
   */
  bool gouroud_color_reset;

  /**
   * The frustrum for the light. Everthing that falls in this frustrum
   * is lit unless it also is in a shadow frustrum.
   */
  csFrustrum* light_frustrum;

  /**
  @@@ OBSOLETE
   * The current view frustrum as seen from the light (relative
   * to the center of the light). If frustrum == NULL then everything is
   * visible (the full frustrum).
   */
  csVector3* frustrum;
  ///
  int num_frustrum;

  /**
  @@@ OBSOLETE
   * All shadows collected upto now.
   * @@@ Currently not used but reserved for later when implementing
   * a better lighting system.
   */
  csShadowPatch* shadows;

  /**
  @@@ OBSOLETE
   * Pointer to the first shadow in the list which belongs to
   * the previous recursion level (and which should not be cleaned
   * by this one).
   */
  csShadowPatch* prev_shadows;

  /**
  @@@ OBSOLETE?
   * A tranform from the space of the light beam to the space of the light
   * source.
   */
  csReversibleTransform beam2source;

public:
  ///
  csLightView () : light_frustrum (NULL), frustrum (NULL), num_frustrum (0), shadows (NULL),
  	prev_shadows (NULL), beam2source () { }
 
  ///
  ~csLightView ()
  {
    CHK (delete [] frustrum);
    //while (shadows && shadows != prev_shadows)
    //{
      //csShadowPatch* s = shadows;
      //shadows = s->next;
      //CHK (delete s);
    //}
  }

  ///
  void SetNumFrustrum (int n)
  {
    num_frustrum = n;
    CHK (delete [] frustrum);
    CHK (frustrum = new csVector3 [n]);
  }

  ///
  int GetNumFrustrum () { return num_frustrum; }

  ///
  csVector3* GetFrustrum () { return frustrum; }

  ///
  csLightView& Copy (csLightView& other)
  {
    if (&other == this) return *this;
    *this = other;
    //prev_shadows = shadows;
    if (other.frustrum)
    {
      CHK (frustrum = new csVector3 [num_frustrum]);
      memcpy (frustrum, other.frustrum, num_frustrum*sizeof (csVector3));
    }
    if (other.light_frustrum)
    {
      CHK (light_frustrum = new csFrustrum (*other.light_frustrum));
    }
    return *this;
  }

  ///
  csLightView& CopyNoFrustrum (csLightView& other)
  {
    if (&other == this) return *this;
    *this = other;
    //prev_shadows = shadows;
    frustrum = NULL,
    num_frustrum = 0;
    light_frustrum = NULL;
    return *this;
  }
};


#endif /*RVIEW_H*/

