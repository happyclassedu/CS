/*
Copyright (C) 2002 by M�rten Svanfeldt
Anders Stenberg

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free
Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"

#include "csgeom/transfrm.h"

#include "csutil/objreg.h"
#include "csutil/ref.h"
#include "csutil/scf.h"
#include "csutil/strset.h"

#include "igeom/clip2d.h"

#include "iutil/cmdline.h"
#include "iutil/comp.h"
#include "iutil/plugin.h"
#include "iutil/eventq.h"

#include "ivaria/reporter.h"

#include "ivideo/lighting.h"
#include "ivideo/txtmgr.h"
#include "ivideo/render3d.h"
#include "ivideo/rndbuf.h"

#include "gl_render3d.h"
#include "gl_sysbufmgr.h"
#include "gl_txtcache.h"
#include "gl_txtmgr.h"
#include "glextmanager.h"

#include "ivideo/effects/efserver.h"
#include "ivideo/effects/efdef.h"
#include "ivideo/effects/eftech.h"
#include "ivideo/effects/efpass.h"
#include "ivideo/effects/eflayer.h"


csRef<iGLStateCache> csGLRender3D::statecache;


CS_IMPLEMENT_PLUGIN

SCF_IMPLEMENT_FACTORY (csGLRender3D)

SCF_EXPORT_CLASS_TABLE (glrender3d)
SCF_EXPORT_CLASS_DEP (csGLRender3D, "crystalspace.render3d.opengl",
                      "OpenGL Render3D graphics driver for Crystal Space",
                      "crystalspace.font.server.")
SCF_EXPORT_CLASS_TABLE_END


SCF_IMPLEMENT_IBASE(csGLRender3D)
SCF_IMPLEMENTS_INTERFACE(iRender3D)
SCF_IMPLEMENTS_EMBEDDED_INTERFACE(iComponent)
SCF_IMPLEMENTS_EMBEDDED_INTERFACE(iEffectClient)
SCF_IMPLEMENT_IBASE_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csGLRender3D::eiComponent)
SCF_IMPLEMENTS_INTERFACE (iComponent)
SCF_IMPLEMENT_EMBEDDED_IBASE_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csGLRender3D::eiEffectClient)
SCF_IMPLEMENTS_INTERFACE (iEffectClient)
SCF_IMPLEMENT_EMBEDDED_IBASE_END

SCF_IMPLEMENT_IBASE (csGLRender3D::EventHandler)
SCF_IMPLEMENTS_INTERFACE (iEventHandler)
SCF_IMPLEMENT_IBASE_END


csGLRender3D::csGLRender3D (iBase *parent)
{
  SCF_CONSTRUCT_IBASE (parent);
  SCF_CONSTRUCT_EMBEDDED_IBASE(scfiComponent);
  SCF_CONSTRUCT_EMBEDDED_IBASE(scfiEffectClient);

  scfiEventHandler = NULL;

  strings = new csStringSet ();

  frustum_valid = false;

  do_near_plane = false;

  stencilclipnum = 0;
  stencil_enabled = false;
  clip_planes_enabled = false;

  render_target = NULL;

  //@@@ Test default. Will have to be autodetected later.
  clip_optional[0] = CS_GL_CLIP_PLANES;
  clip_optional[1] = CS_GL_CLIP_STENCIL;
  clip_optional[2] = CS_GL_CLIP_NONE;
  clip_required[0] = CS_GL_CLIP_PLANES;
  clip_required[1] = CS_GL_CLIP_STENCIL;
  clip_required[2] = CS_GL_CLIP_NONE;
  clip_outer[0] = CS_GL_CLIP_PLANES;
  clip_outer[1] = CS_GL_CLIP_STENCIL;
  clip_outer[2] = CS_GL_CLIP_NONE;
}

csGLRender3D::~csGLRender3D()
{
  delete strings;

  delete buffermgr;
  delete txtcache;
  delete txtmgr;
}




////////////////////////////////////////////////////////////////////
// Private helpers
////////////////////////////////////////////////////////////////////




void csGLRender3D::Report (int severity, const char* msg, ...)
{
  va_list arg;
  va_start (arg, msg);
  csRef<iReporter> rep (CS_QUERY_REGISTRY (object_reg, iReporter));
  if (rep)
    rep->ReportV (severity, "crystalspace.render3d.opengl", msg, arg);
  else
  {
    csPrintfV (msg, arg);
    csPrintf ("\n");
  }
  va_end (arg);
}

int csGLRender3D::GetMaxTextureSize ()
{
  GLint max;
  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max);
  return max;
}

void csGLRender3D::SetGlOrtho (bool inverted)
{
  if (render_target)
  {
    if (inverted)
      glOrtho (0., (GLdouble) (viewwidth+1),
      (GLdouble) (viewheight+1), 0., -1.0, 10.0);
    else
      glOrtho (0., (GLdouble) (viewwidth+1), 0.,
      (GLdouble) (viewheight+1), -1.0, 10.0);
  }
  else
  {
    if (inverted)
      glOrtho (0., (GLdouble) viewwidth,
      (GLdouble) viewheight, 0., -1.0, 10.0);
    else
      glOrtho (0., (GLdouble) viewwidth, 0.,
      (GLdouble) viewheight, -1.0, 10.0);
  }
}

void csGLRender3D::SetZMode (csZBufMode mode)
{
  switch (mode)
  {
  case CS_ZBUF_NONE:
    current_zmode = mode;
    statecache->DisableState (GL_DEPTH_TEST);
    break;
  case CS_ZBUF_FILL:
  case CS_ZBUF_FILLONLY:
    current_zmode = mode;
    statecache->EnableState (GL_DEPTH_TEST);
    statecache->SetDepthFunc (GL_ALWAYS);
    statecache->SetDepthMask (GL_TRUE);
    break;
  case CS_ZBUF_EQUAL:
    current_zmode = mode;
    statecache->EnableState (GL_DEPTH_TEST);
    statecache->SetDepthFunc (GL_EQUAL);
    statecache->SetDepthMask (GL_FALSE);
    break;
  case CS_ZBUF_TEST:
    current_zmode = mode;
    statecache->EnableState (GL_DEPTH_TEST);
    statecache->SetDepthFunc (GL_GREATER);
    statecache->SetDepthMask (GL_FALSE);
    break;
  case CS_ZBUF_USE:
    current_zmode = mode;
    statecache->EnableState (GL_DEPTH_TEST);
    statecache->SetDepthFunc (GL_GREATER);
    statecache->SetDepthMask (GL_TRUE);
    break;
  default:
    break;
  }
}

csZBufMode csGLRender3D::GetZModePass2 (csZBufMode mode)
{
  switch (mode)
  {
  case CS_ZBUF_NONE:
  case CS_ZBUF_TEST:
  case CS_ZBUF_EQUAL:
    return mode;
    break;
  case CS_ZBUF_FILL:
  case CS_ZBUF_FILLONLY:
    return CS_ZBUF_EQUAL;
    break;
  case CS_ZBUF_USE:
    return CS_ZBUF_EQUAL;
    break;
  default:
    break;
  }
  return mode;
}

void csGLRender3D::CalculateFrustum ()
{
  if (frustum_valid) return;
  frustum_valid = true;
  if (clipper)
  {
    frustum.MakeEmpty ();
    int nv = clipper->GetVertexCount ();
    csVector3 v3;
    v3.z = 1;
    csVector2* v = clipper->GetClipPoly ();
    int i;
    for (i = 0 ; i < nv ; i++)
    {
      v3.x = (v[i].x - asp_center_x) * (1.0/aspect);
      v3.y = (v[i].y - asp_center_y) * (1.0/aspect);
      frustum.AddVertex (v3);
    }
  }
}

void csGLRender3D::SetupStencil ()
{
  if (clipper)
  {
    glMatrixMode (GL_PROJECTION);
    glPushMatrix ();
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glLoadIdentity ();
    // First set up the stencil area.
    statecache->EnableState (GL_STENCIL_TEST);
    if (stencilclipnum == 0)
    {
      glClearStencil (0);
      glClear (GL_STENCIL_BUFFER_BIT);
      stencilclipnum++;
    }
    statecache->SetStencilFunc (GL_ALWAYS, stencilclipnum, -1);
    statecache->SetStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE);
    int nv = clipper->GetVertexCount ();
    csVector2* v = clipper->GetClipPoly ();
    glColor4f (0, 0, 0, 0);
    statecache->SetShadeModel (GL_FLAT);
    SetZMode (CS_ZBUF_NONE);
    statecache->DisableState (GL_TEXTURE_2D);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glBegin (GL_TRIANGLE_FAN);
    int i;
    for (i = 0 ; i < nv ; i++)
      glVertex2f (2.0*v[i].x/(float)viewwidth-1.0,
                  2.0*v[i].y/(float)viewheight-1.0);
    glEnd ();
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    statecache->DisableState (GL_STENCIL_TEST);
    glPopMatrix ();
    glMatrixMode (GL_PROJECTION);
    glPopMatrix ();
  }
}

void csGLRender3D::SetupClipPlanes (bool add_near_clip,
                                    bool add_z_clip)
{
  // This routine assumes the hardware planes can handle the
  // required number of planes from the clipper.
  if (clipper)
  {
    CalculateFrustum ();
    csPlane3 pl;
    GLdouble plane_eq[4];
    int i, i1;
    i1 = frustum.GetVertexCount ()-1;

    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glLoadIdentity ();
    for (i = 0 ; i < frustum.GetVertexCount () ; i++)
    {
      pl.Set (csVector3 (0), frustum[i], frustum[i1]);
      plane_eq[0] = pl.A ();
      plane_eq[1] = pl.B ();
      plane_eq[2] = pl.C ();
      plane_eq[3] = pl.D ();
      glClipPlane ((GLenum)(GL_CLIP_PLANE0+i), plane_eq);
      i1 = i;
    }
    if (add_near_clip)
    {
      plane_eq[0] = -near_plane.A ();
      plane_eq[1] = -near_plane.B ();
      plane_eq[2] = -near_plane.C ();
      plane_eq[3] = -near_plane.D ();
      glClipPlane ((GLenum)(GL_CLIP_PLANE0+i), plane_eq);
      i++;
    }
    if (add_z_clip)
    {
      plane_eq[0] = 0;
      plane_eq[1] = 0;
      plane_eq[2] = 1;
      plane_eq[3] = -.001;
      glClipPlane ((GLenum)(GL_CLIP_PLANE0+i), plane_eq);
    }
    glPopMatrix ();
  }
}

void csGLRender3D::SetupClipper (int clip_portal,
                                 int clip_plane,
                                 int clip_z_plane)
{
  if (clipperinitialized)
    return;

  stencil_enabled = false;
  clip_planes_enabled = false;

  //===========
  // First we are going to find out what kind of clipping (if any)
  // we need. This depends on various factors including what the engine
  // says about the mesh (the clip_portal and clip_plane flags in the
  // mesh), what the current clipper is (the current cliptype), what
  // the current z-buf render mode is, and what the settings are to use
  // for the clipper on the current type of hardware (the clip_... arrays).
  //===========
  char how_clip = CS_GL_CLIP_NONE;
  bool use_lazy_clipping = false;
  bool do_plane_clipping = false;
  bool do_z_plane_clipping = false;

  // First we see how many additional planes we might need because of
  // z-plane clipping and/or near-plane clipping. These additional planes
  // will not be usable for portal clipping (if we're using OpenGL plane
  // clipping).
  int reserved_planes =
    int (do_near_plane && clip_plane != CS_CLIP_NOT) +
    int (clip_z_plane != CS_CLIP_NOT);

  if (clip_portal != CS_CLIP_NOT)
  {
    // Some clipping may be required.

    // In some z-buf modes we cannot use clipping modes that depend on
    // zbuffer ('n','N', 'z', or 'Z').
    bool no_zbuf_clipping = (
      current_zmode == CS_ZBUF_NONE || current_zmode == CS_ZBUF_FILL ||
      current_zmode == CS_ZBUF_FILLONLY);

    // Select the right clipping mode variable depending on the
    // type of clipper.
    int ct = cliptype;
    // If clip_portal in the mesh indicates that we might need toplevel
    // clipping then we do as if the current clipper type is toplevel.
    if (clip_portal == CS_CLIP_TOPLEVEL) ct = CS_CLIPPER_TOPLEVEL;
    char* clip_modes;
    switch (ct)
    {
    case CS_CLIPPER_OPTIONAL: clip_modes = clip_optional; break;
    case CS_CLIPPER_REQUIRED: clip_modes = clip_required; break;
    case CS_CLIPPER_TOPLEVEL: clip_modes = clip_outer; break;
    default: clip_modes = clip_optional;
    }

    // Go through all the modes and select the first one that is appropriate.
    for (int i = 0 ; i < 3 ; i++)
    {
      char c = clip_modes[i];
      // We cannot use n,N,z, or Z if no_zbuf_clipping is true.
      if ((c == 'n' || c == 'N' || c == 'z' || c == 'Z') && no_zbuf_clipping)
        continue;
      // We cannot use p or P if the clipper has more vertices than the
      // number of hardware planes minus one (for the view plane).
      if ((c == 'p' || c == 'P') &&
        clipper->GetVertexCount ()
        >= 6-reserved_planes)
        continue;
      how_clip = c;
      break;
    }
    if (how_clip != '0' && toupper (how_clip) == how_clip)
    {
      use_lazy_clipping = true;
      how_clip = tolower (how_clip);
    }
  }

  // Check for the near-plane.
  if (do_near_plane && clip_plane != CS_CLIP_NOT)
  {
    do_plane_clipping = true;
    // If we must do clipping to the near plane then we cannot use
    // lazy clipping.
    use_lazy_clipping = false;
    // If we are doing plane clipping already then we don't have
    // to do additional software plane clipping as the OpenGL plane
    // clipper can do this too.
    if (how_clip == 'p')
    {
      do_plane_clipping = false;
    }
  }

  // Check for the z-plane.
  if (clip_z_plane != CS_CLIP_NOT)
  {
    do_z_plane_clipping = true;
    // If hardware requires clipping to the z-plane (because it
    // crashes otherwise) we have to disable lazy clipping.
    // @@@
    if (true)
    {
      use_lazy_clipping = false;
    }
    else
    {
      // If we are doing plane clipping already then we don't have
      // to do additional software plane clipping as the OpenGL plane
      // clipper can do this too.
      if (how_clip == 'p')
      {
        do_z_plane_clipping = false;
      }
    }
  }

  //===========
  // First setup the clipper that we need.
  //===========
  if (how_clip == 's')
  {
    SetupStencil ();
    stencil_enabled = true;
    // Use the stencil area.
    statecache->EnableState (GL_STENCIL_TEST);
    statecache->SetStencilFunc (GL_EQUAL, stencilclipnum, stencilclipnum);
    statecache->SetStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
    stencilclipnum++;
    if (stencilclipnum>255)
      stencilclipnum = 0;
  }
  else if (how_clip == 'p')
  {
    clip_planes_enabled = true;
    CalculateFrustum ();
    SetupClipPlanes (do_near_plane && clip_plane != CS_CLIP_NOT,
      clip_z_plane != CS_CLIP_NOT);
    for (int i = 0; i < frustum.GetVertexCount ()+reserved_planes; i++)
      statecache->EnableState ((GLenum)(GL_CLIP_PLANE0+i));
  }
}




////////////////////////////////////////////////////////////////////
// iRender3D
////////////////////////////////////////////////////////////////////




bool csGLRender3D::Open ()
{
  csRef<iPluginManager> plugin_mgr (
    CS_QUERY_REGISTRY (object_reg, iPluginManager));

  csRef<iCommandLineParser> cmdline (CS_QUERY_REGISTRY (object_reg,
    iCommandLineParser));

  config.AddConfig(object_reg, "/config/r3dopengl.cfg");

  const char *driver = cmdline->GetOption ("canvas");
  if (!driver)
    driver = config->GetStr ("Video.OpenGL.Canvas", CS_OPENGL_2D_DRIVER);

  // @@@ Should check what canvas to load
  G2D = CS_LOAD_PLUGIN (plugin_mgr, driver, iGraphics2D);
  if (!G2D)
    return false;

  if (!G2D->Open ())
  {
    Report (CS_REPORTER_SEVERITY_ERROR, "Error opening Graphics2D context.");
    return false;
  }

  G2D->PerformExtension("getstatecache", &statecache);

  int w = G2D->GetWidth ();
  int h = G2D->GetHeight ();
  SetDimensions (w, h);
  asp_center_x = w/2;
  asp_center_y = h/2;

  /*effectserver = CS_QUERY_REGISTRY(object_reg, iEffectServer);
  if( !effectserver )
  {
  effectserver = CS_LOAD_PLUGIN (plugin_mgr,
  "crystalspace.video.effects.stdserver", iEffectServer);
  object_reg->Register (effectserver, "iEffectServer");
  }*/

  csRef<iOpenGLInterface> gl = SCF_QUERY_INTERFACE (G2D, iOpenGLInterface);
  ext.InitExtensions (gl);

  if( false && ext.CS_GL_NV_vertex_array_range && ext.CS_GL_NV_fence)
  {
    csVARRenderBufferManager * bm = new csVARRenderBufferManager();
    bm->Initialize(this);
    buffermgr = bm;
  } else
  {
    buffermgr = new csSysRenderBufferManager();
  }

  txtcache = new csGLTextureCache (1024*1024*32, this);
  txtmgr = new csGLTextureManager (object_reg, GetDriver2D (), config, this);

  glClearDepth (0.0);
  statecache->EnableState (GL_CULL_FACE);
  statecache->SetCullFace (GL_FRONT);

  return true;
}

void csGLRender3D::Close ()
{
  if (G2D)
    G2D->Close ();
}

bool csGLRender3D::BeginDraw (int drawflags)
{
  current_drawflags = drawflags;

  if (render_target)
  {
    int txt_w, txt_h;
    render_target->GetMipMapDimensions (0, txt_w, txt_h);
    if (!rt_cliprectset)
    {
      G2D->GetClipRect (rt_old_minx, rt_old_miny, rt_old_maxx, rt_old_maxy);
      G2D->SetClipRect (-1, -1, txt_w+1, txt_h+1);
      rt_cliprectset = true;

      glMatrixMode (GL_PROJECTION);
      glLoadIdentity ();
      SetGlOrtho (false);
      glViewport (1, -1, viewwidth+1, viewheight+1);
    }

    if (!rt_onscreen)
    {
      txtcache->Cache (render_target);
      GLuint handle = ((csTxtCacheData *)render_target->GetCacheData ())
        ->Handle;
      statecache->SetShadeModel (GL_FLAT);
      statecache->EnableState (GL_TEXTURE_2D);
      glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
      statecache->SetTexture (GL_TEXTURE_2D, handle);
      statecache->DisableState (GL_BLEND);
      SetZMode (CS_ZBUF_NONE);

      glBegin (GL_QUADS);
      glTexCoord2f (0, 0); glVertex2i (0, viewheight-txt_h+1);
      glTexCoord2f (0, 1); glVertex2i (0, viewheight-0+1);
      glTexCoord2f (1, 1); glVertex2i (txt_w, viewheight-0+1);
      glTexCoord2f (1, 0); glVertex2i (txt_w, viewheight-txt_h+1);
      glEnd ();
      rt_onscreen = true;
    }
  }

  if (drawflags & CSDRAW_CLEARZBUFFER)
  {
    statecache->SetDepthMask (GL_TRUE);
    if (drawflags & CSDRAW_CLEARSCREEN)
      glClear (GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    else
      glClear (GL_DEPTH_BUFFER_BIT);
  }
  else if (drawflags & CSDRAW_CLEARSCREEN)
    G2D->Clear (0);

  if (!render_target && (drawflags & CSDRAW_3DGRAPHICS))
  {
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    SetGlOrtho (false);
    glViewport (1, -1, viewwidth+1, viewheight+1);
    glTranslatef (viewwidth/2, viewheight/2, 0);

    GLfloat matrixholder[16];
    for (int i = 0 ; i < 16 ; i++) matrixholder[i] = 0.0;
    matrixholder[0] = matrixholder[5] = 1.0;
    matrixholder[11] = 1.0/aspect;
    matrixholder[14] = -matrixholder[11];
    glMultMatrixf (matrixholder);
    return true;
  }
  if (drawflags & CSDRAW_2DGRAPHICS)
  {
    SetZMode(CS_ZBUF_NONE);
    return G2D->BeginDraw ();
  }

  current_drawflags = 0;
  return false;
}

void csGLRender3D::FinishDraw ()
{
  if (current_drawflags & (CSDRAW_2DGRAPHICS | CSDRAW_3DGRAPHICS))
    G2D->FinishDraw ();
  current_drawflags = 0;

  if (render_target)
  {
    if (rt_cliprectset)
    {
      rt_cliprectset = false;
      G2D->SetClipRect (rt_old_minx, rt_old_miny, rt_old_maxx, rt_old_maxy);
      glMatrixMode (GL_PROJECTION);
      glLoadIdentity ();
      glOrtho (0., viewwidth, 0., viewheight, -1.0, 10.0);
      glViewport (0, 0, viewwidth, viewheight);
    }

    if (rt_onscreen)
    {
      rt_onscreen = false;
      statecache->EnableState (GL_TEXTURE_2D);
      SetZMode (CS_ZBUF_NONE);
      statecache->DisableState (GL_BLEND);
      statecache->DisableState (GL_ALPHA_TEST);
      int txt_w, txt_h;
      render_target->GetMipMapDimensions (0, txt_w, txt_h);
      csGLTextureHandle* tex_mm = (csGLTextureHandle *)
        render_target->GetPrivateObject ();
      csGLTexture *tex_0 = tex_mm->vTex[0];
      csTxtCacheData *tex_data = (csTxtCacheData*)render_target->GetCacheData();
      if (tex_data)
      {
        // Texture is in tha cache, update texture directly.
        statecache->SetTexture (GL_TEXTURE_2D, tex_data->Handle);
        // Texture was not used as a render target before.
        // Make some necessary adjustments.
        if (!tex_mm->was_render_target)
        {
          if (!(tex_mm->GetFlags() & CS_TEXTURE_NOMIPMAPS))
          {
            if (ext.CS_GL_SGIS_generate_mipmap)
            {
              glTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
            }
            else
            {
              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                txtcache->GetBilinearMapping() ? GL_LINEAR : GL_NEAREST);
            }
          }
          tex_mm->was_render_target = true;
        }
        glCopyTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 1, viewheight-txt_h,
          txt_w, txt_h, 0);
      }
      else
      {
        // Not in cache.
#ifdef GL_VERSION_1_2x
        if (pfmt.PixelBytes == 2)
        {
          char* buffer = new char[2*txt_w*txt_h]; // @@@ Alloc elsewhere!!!
          glReadPixels (0, height-txt_h, txt_w, txt_h,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer);

          csRGBpixel *dst = tex_0->get_image_data ();
          uint16 bb = 8 - pfmt.BlueBits;
          uint16 gb = 8 - pfmt.GreenBits;
          uint16 rb = 8 - pfmt.RedBits;
          uint16 *src = (uint16*) buffer;
          int i;
          for (i = 0 ; i < width*height ; i++, src++, dst++)
          {
            dst->red = ((*src & pfmt.RedMask) >> pfmt.RedShift) << rb;
            dst->green = ((*src & pfmt.GreenMask) >> pfmt.GreenShift) << gb;
            dst->blue = ((*src & pfmt.BlueMask) >> pfmt.BlueShift) << bb;
          }
          delete[] buffer;
        }
        else
          glReadPixels (1, height-txt_h, txt_w, txt_h,
          GL_RGBA, GL_UNSIGNED_BYTE, tex_0->get_image_data());
#else
        glReadPixels (1, viewheight-txt_h, txt_w, txt_h, tex_mm->SourceFormat (),
          tex_mm->SourceType (), tex_0->get_image_data ());
#endif
      }
    }
  }
  render_target = NULL;
}

void csGLRender3D::Print (csRect* area)
{
  G2D->Print (area);
}

csReversibleTransform* csGLRender3D::GetWVMatrix()
{
  return NULL;
}

void csGLRender3D::SetObjectToCamera(csReversibleTransform* wvmatrix)
{
  GLfloat matrixholder[16];
  const csMatrix3 &orientation = wvmatrix->GetO2T();

  matrixholder[0] = orientation.m11;
  matrixholder[1] = orientation.m21;
  matrixholder[2] = orientation.m31;

  matrixholder[4] = orientation.m12;
  matrixholder[5] = orientation.m22;
  matrixholder[6] = orientation.m32;

  matrixholder[8] = orientation.m13;
  matrixholder[9] = orientation.m23;
  matrixholder[10] = orientation.m33;

  matrixholder[3] = matrixholder[7] = matrixholder[11] =
    matrixholder[12] = matrixholder[13] = matrixholder[14] = 0.0;
  matrixholder[15] = 1.0;

  const csVector3 &translation = wvmatrix->GetO2TTranslation();

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glMultMatrixf (matrixholder);
  glTranslatef (-translation.x, -translation.y, -translation.z);
}

void csGLRender3D::DrawMesh(csRenderMesh* mymesh,
                            csZBufMode z_buf_mode,
                            int clip_portal,
                            int clip_plane,
                            int clip_z_plane)
{
  csRef<iStreamSource> source = mymesh->GetStreamSource ();
  csRef<iRenderBuffer> vertexbuf =
    source->GetBuffer (strings->Request ("vertices"));
  csRef<iRenderBuffer> texcoordbuf =
    source->GetBuffer (strings->Request ("texture coordinates"));
  csRef<iRenderBuffer> indexbuf =
    source->GetBuffer (strings->Request ("indices"));

  if (!vertexbuf)
    return;

  SetupClipper (clip_portal, clip_plane, clip_z_plane);
  
  SetZMode (z_buf_mode);

  csRef<iMaterialHandle> mathandle;
  if (mymesh->GetMaterialWrapper ())
    mathandle = mymesh->GetMaterialWrapper ()->GetMaterialHandle ();

  csRef<iTextureHandle> txthandle;
  if (mathandle)
    txthandle = mathandle->GetTexture ();

  if (txthandle)
  {
    txtcache->Cache (txthandle);
    csGLTextureHandle *gltxthandle = (csGLTextureHandle *)
      txthandle->GetPrivateObject ();
    csTxtCacheData *cachedata =
      (csTxtCacheData *)gltxthandle->GetCacheData ();

    statecache->SetTexture (GL_TEXTURE_2D, cachedata->Handle);
    statecache->EnableState (GL_TEXTURE_2D);
  } else statecache->DisableState (GL_TEXTURE_2D);


  glColor3f (1,1,1);
  glVertexPointer (3, GL_FLOAT, 0,
    (float*)vertexbuf->Lock(iRenderBuffer::CS_BUF_LOCK_RENDER));
  glEnableClientState (GL_VERTEX_ARRAY);

  if (texcoordbuf)
  {
    glTexCoordPointer (2, GL_FLOAT, 0, (float*)
      texcoordbuf->Lock(iRenderBuffer::CS_BUF_LOCK_RENDER));
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
  }
  /*glIndexPointer (GL_INT, 0, vertexbuf->GetUIntBuffer ());
  glEnableClientState (GL_INDEX_ARRAY);
  glDrawArrays (GL_TRIANGLES, 0, indexbuf->GetUIntLength ());*/
  glDrawElements (
    GL_TRIANGLES,
    mymesh->GetIndexEnd ()-mymesh->GetIndexStart (),
    GL_UNSIGNED_INT,
    ((unsigned int*)indexbuf->Lock(iRenderBuffer::CS_BUF_LOCK_RENDER))
    +mymesh->GetIndexStart ());

  vertexbuf->Release();
  indexbuf->Release();
  texcoordbuf->Release();

  if (clip_planes_enabled)
  {
    clip_planes_enabled = false;
    for (int i = 0; i < 6; i++)
      statecache->DisableState ((GLenum)(GL_CLIP_PLANE0+i));
  }
  if (stencil_enabled)
  {
    stencil_enabled = false;
    statecache->DisableState (GL_STENCIL_TEST);
  }

}

void csGLRender3D::SetClipper (iClipper2D* clipper, int cliptype)
{
  csGLRender3D::clipper = clipper;
  if (!clipper) cliptype = CS_CLIPPER_NONE;
  csGLRender3D::cliptype = cliptype;
  clipperinitialized = false;
}




////////////////////////////////////////////////////////////////////
// iEffectClient
////////////////////////////////////////////////////////////////////




bool csGLRender3D::Validate (iEffectDefinition* effect, iEffectTechnique* technique)
{
  return false;
}




////////////////////////////////////////////////////////////////////
// iComponent
////////////////////////////////////////////////////////////////////




bool csGLRender3D::Initialize (iObjectRegistry* p)
{
  object_reg = p;

  if (!scfiEventHandler)
    scfiEventHandler = new EventHandler (this);

  csRef<iEventQueue> q (CS_QUERY_REGISTRY(object_reg, iEventQueue));
  if (q)
    q->RegisterListener (scfiEventHandler, CSMASK_Broadcast);


  return true;
}




////////////////////////////////////////////////////////////////////
// iEventHandler
////////////////////////////////////////////////////////////////////




bool csGLRender3D::HandleEvent (iEvent& Event)
{
  if (Event.Type == csevBroadcast)
    switch (Event.Command.Code)
  {
    case cscmdSystemOpen:
      {
        Open ();
        return true;
      }
    case cscmdSystemClose:
      {
        Close ();
        return true;
      }
  }
  return false;
}
