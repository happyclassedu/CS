/*
    Copyright (C) 2001 by Norman Kr�mer
  
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
#include "avistrv.h"
#include "isystem.h"
#include "itexture.h"
#include "itxtmgr.h"

IMPLEMENT_IBASE (csAVIStreamVideo)
  IMPLEMENTS_INTERFACE (iVideoStream)
  IMPLEMENTS_INTERFACE (iStream)
IMPLEMENT_IBASE_END

csAVIStreamVideo::csAVIStreamVideo (iBase *pBase): memimage (1,1)
{
  CONSTRUCT_IBASE (pBase);
  pChunk = NULL;
  pAVI = (csAVIFormat*)pBase;
  pSystem = NULL;
  pG3D = NULL;
  pCodec = NULL;
  pMaterial = NULL;

  pIA = new csImageArea (1,1,1,1);
}

bool csAVIStreamVideo::Initialize (const csAVIFormat::AVIHeader *ph, 
				   const csAVIFormat::StreamHeader *psh, 
				   const csAVIFormat::VideoStreamFormat *pf, 
				   UShort nStreamNumber, iSystem *pTheSystem)
{

  strdesc.type = CS_STREAMTYPE_VIDEO;
  memcpy (strdesc.codec, psh->handler, 4);
  strdesc.codec[4] = '\0';
  strdesc.colordepth = pf->planes * pf->bitcount;
  strdesc.framecount = ph->framecount;
  strdesc.width = ph->width;
  strdesc.height = ph->height;
  strdesc.framerate = 1000./ph->msecperframe; // @@@, this is wrong, gonna reread it on the MS site when DDoS is over
  strdesc.duration = psh->length / psh->scale;

  delete pChunk;
  pChunk = new csAVIFormat::AVIDataChunk;
  pChunk->currentframe = -1;
  pChunk->currentframepos = NULL;
  sprintf (pChunk->id, "%02dd", nStreamNumber);
  pChunk->id[3] = '\0';

  nStream = nStreamNumber;
  if (pSystem) pSystem->DecRef ();
  (pSystem = pTheSystem)->IncRef ();
  if (pG3D) pG3D->DecRef ();
  pG3D = QUERY_PLUGIN (pSystem, iGraphics3D);
  if (pG2D) pG2D->DecRef ();
  pG2D = QUERY_PLUGIN (pSystem, iGraphics2D);

  pIA->w = 0;
  pIA->h = 0;
  pIA->x = 0;
  pIA->y = 0;
  delete pIA->data;
  pIA->data = NULL;

  SetRect(0, 0, strdesc.width, strdesc.height);

  bTimeSynced = false;
  fxmode = CS_FX_COPY;
  polyfx.num = 4;
  polyfx.use_fog = false;
  polyfx.vertices[0].u = 0;
  polyfx.vertices[0].v = 0;
  polyfx.vertices[1].u = 1;
  polyfx.vertices[1].v = 0;
  polyfx.vertices[2].u = 1;
  polyfx.vertices[2].v = 1;
  polyfx.vertices[3].u = 0;
  polyfx.vertices[3].v = 1;
  for (int i=0; i<4; i++)
  {
    polyfx.vertices[i].r = 1;
    polyfx.vertices[i].g = 1;
    polyfx.vertices[i].b = 1;
    polyfx.vertices[i].z = 1;
  }

  if (pMaterial) pMaterial->DecRef ();
  pMaterial = NULL;
  return LoadCodec ();
}

csAVIStreamVideo::~csAVIStreamVideo ()
{
  delete pChunk;
  delete pIA->data;
  delete pIA;
  if (pMaterial) pMaterial->DecRef ();
  if (pCodec) pCodec->DecRef ();
  if (pG2D) pG2D->DecRef ();
  if (pG3D) pG3D->DecRef ();
  if (pSystem) pSystem->DecRef ();
}

void csAVIStreamVideo::GetStreamDescription (csStreamDescription &desc)
{
  memcpy (&desc, (csStreamDescription*)&strdesc, sizeof (csStreamDescription));
}

bool csAVIStreamVideo::GotoFrame (ULong frameindex)
{
  return pAVI->GetChunk (frameindex, pChunk);
}

bool csAVIStreamVideo::GotoTime (ULong timeindex)
{
  (void)timeindex;
  // not yet implemented
  return false;
}

bool csAVIStreamVideo::SetPlayMethod (bool bTimeSynced)
{
  // timesynced isnt yet implemented, so return false if its requested.
  return bTimeSynced == false;
}

void csAVIStreamVideo::GetStreamDescription (csVideoStreamDescription &desc)
{
  memcpy (&desc, &strdesc, sizeof (csVideoStreamDescription));
}

bool csAVIStreamVideo::SetRect (int x, int y, int w, int h)
{
  int height = pG3D->GetHeight ()-1;

  rc.Set (x, y, x+w, y+h);
  polyfx.vertices[0].sx = x;
  polyfx.vertices[0].sy = height - y;
  polyfx.vertices[1].sx = x+w;
  polyfx.vertices[1].sy = height - y;
  polyfx.vertices[2].sx = x+w;
  polyfx.vertices[2].sy = height - (y+h);
  polyfx.vertices[3].sx = x;
  polyfx.vertices[3].sy = height - (y+h);

  memimage.Rescale (w, h);
  return true;
}

bool csAVIStreamVideo::SetFXMode (UInt mode)
{
  fxmode = mode;
  return true;
}

iMaterialHandle* csAVIStreamVideo::NextFrameGetMaterial ()
{
  if (NextFrameGetData ())
  {
    makeMaterial ();
    return pMaterial;
  }
  return NULL;
}

void csAVIStreamVideo::PrepImageArea ()
{
  int nPB = pG2D->GetPixelBytes ();
  if (rc.Height () != pIA->h || rc.Width () != pIA->w)
  {
    delete pIA->data;
    pIA->data = new char [nPB * rc.Width () * rc.Height ()];
    pIA->x = rc.xmin;
    pIA->y = rc.ymin;
    pIA->w = rc.Width ();
    pIA->h = rc.Height ();
  }

  csRGBpixel *pixel = (csRGBpixel *)memimage.GetImageData ();
  char *p = pIA->data;
  iTextureManager *pTexMgr = pG3D->GetTextureManager();

  for (int row=0; row<rc.Height (); row++)
    for (int col=0; col<rc.Width (); col++)
    {
      int color = pTexMgr->FindRGB (pixel->red, pixel->green, pixel->blue);
      memcpy (p, ((char*)&color), nPB);
      p += nPB;
      pixel++;
    }
}

bool csAVIStreamVideo::NextFrameGetData ()
{
  void *outdata;

  if (pAVI->GetChunk (pChunk->currentframe+1, pChunk))
  {
    pCodec->Decode ((char*)pChunk->data, pChunk->length, outdata);
    if (cdesc.decodeoutput == CS_CODECFORMAT_YUV_CHANNEL)
      //      rgb_channel_2_rgba_interleave ((char **)outdata);
      yuv_channel_2_rgba_interleave ((char **)outdata);
    else
    if (cdesc.decodeoutput == CS_CODECFORMAT_RGB_CHANNEL)
      rgb_channel_2_rgba_interleave ((char **)outdata);
    else
    if (cdesc.decodeoutput == CS_CODECFORMAT_RGBA_CHANNEL)
      rgba_channel_2_rgba_interleave ((char **)outdata);
    else
    if (cdesc.decodeoutput != CS_CODECFORMAT_RGBA_INTERLEAVED)
      return false;
    return true;

  }
  return false;
}

void csAVIStreamVideo::NextFrame ()
{
  if (NextFrameGetData ())
  {
    /*
    unsigned int zbuf = pG3D->GetRenderState( G3DRENDERSTATE_ZBUFFERMODE );
    bool btex = pG3D->GetRenderState( G3DRENDERSTATE_TEXTUREMAPPINGENABLE ) != 0;
    bool bgour = pG3D->GetRenderState( G3DRENDERSTATE_GOURAUDENABLE ) != 0;

    pG3D->SetRenderState( G3DRENDERSTATE_ZBUFFERMODE, CS_ZBUF_NONE );
    pG3D->SetRenderState( G3DRENDERSTATE_TEXTUREMAPPINGENABLE, true );
    pG3D->SetRenderState( G3DRENDERSTATE_GOURAUDENABLE, false );

    polyfx.mat_handle = pMaterial;
    pG3D->StartPolygonFX (polyfx.mat_handle, fxmode);
    pG3D->DrawPolygonFX (polyfx);
    pG3D->FinishPolygonFX ();

    pG3D->SetRenderState( G3DRENDERSTATE_ZBUFFERMODE, zbuf );
    pG3D->SetRenderState( G3DRENDERSTATE_TEXTUREMAPPINGENABLE, btex );
    pG3D->SetRenderState( G3DRENDERSTATE_GOURAUDENABLE, bgour );
    */
    PrepImageArea ();
    pG2D->RestoreArea (pIA, false);
  }
}

void csAVIStreamVideo::yuv_channel_2_rgba_interleave (char *data[3])
{
#define FIX_RANGE(x) (unsigned char)((x)>255.f ? 255 : (x) < 0.f ? 0 : (x))

  char *ydata = data[0];
  char *udata = data[1];
  char *vdata = data[2];
  int idx=0, uvidx=0, tidx=0;
  int sw = strdesc.width;
  int sh = strdesc.height;
  int tw = rc.Width ();
  int th = rc.Height ();
  int ytic=th, xtic;
  int ls = 0;
  float y,u,v, uf1, uf2, vf1, vf2;

  csRGBpixel *pixel = (csRGBpixel *)memimage.GetImageData ();
  for (int row=0; row < th; row++)
  {
    xtic = 0;
    for (int col=0; col < tw; col++)
    {
      if (uvidx != (idx>>2)) // this YUV is a 1:4:4 scheme
      {
	uvidx = idx >> 2;
	v=((float)(unsigned char)udata[uvidx]) - 128.f;
	u=((float)(unsigned char)vdata[uvidx]) - 128.f;
	uf1 = 2.018f * u;
	uf2 = 0.813f * u;
	vf1 = 0.391f * v;
	vf2 = 1.596f * v;
      }
      y=1.164f*(((float)(unsigned char)ydata[idx]) - 16.f);
      pixel[tidx].blue = FIX_RANGE(y + uf1);
      pixel[tidx].green= FIX_RANGE(y - uf2 - vf1);
      pixel[tidx].red  = FIX_RANGE(y + vf2);

      while (xtic < sw)
      {
	idx++;
	xtic += tw;
      }
      xtic -=sw;
      tidx++;
    }
    while (ytic < sh)
    {
      ls += sw;
      ytic += th;
    }
    ytic -=sh;
    idx = ls;
  }

#undef FIX_RANGE
}

void csAVIStreamVideo::rgb_channel_2_rgba_interleave (char *data[3])
{
  char *rdata = data[0];
  char *gdata = data[1];
  char *bdata = data[2];
  csRGBpixel *pixel = (csRGBpixel *)memimage.GetImageData ();
  for (int idx=0, y=0; y < memimage.GetHeight (); y++)
    for (int x=0; x < memimage.GetWidth (); x++)
    {
      pixel[idx].red   = rdata[idx];
      pixel[idx].green = gdata[idx];
      pixel[idx].blue  = bdata[idx];
      idx++;
    }
}

void csAVIStreamVideo::rgba_channel_2_rgba_interleave (char *data[4])
{
  char *rdata = data[0];
  char *gdata = data[1];
  char *bdata = data[2];
  char *adata = data[3];
  csRGBpixel *pixel = (csRGBpixel *)memimage.GetImageData ();
  for (int idx=0, y=0; y < memimage.GetHeight (); y++)
    for (int x=0; x < memimage.GetWidth (); x++)
    {
      pixel[idx].red   = rdata[idx];
      pixel[idx].green = gdata[idx];
      pixel[idx].blue  = bdata[idx];
      pixel[idx].alpha = adata[idx];
      idx++;
    }
}

void csAVIStreamVideo::makeMaterial ()
{
  if (pMaterial)
    pMaterial->DecRef ();

  iTextureManager *txtmgr = pG3D->GetTextureManager();
  iTextureHandle *pFrameTex = txtmgr->RegisterTexture (&memimage, CS_TEXTURE_NOMIPMAPS);
  pFrameTex->Prepare ();
  pMaterial = txtmgr->RegisterMaterial (pFrameTex);
  pMaterial->Prepare ();
}

bool csAVIStreamVideo::LoadCodec ()
{
  // based on the codec id we try to load the apropriate codec
  iSCF *pSCF = QUERY_INTERFACE (pSystem, iSCF);
  if (pSCF)
  {
    // create a classname from the coec id
    char cn[128];
    sprintf (cn, "crystalspace.video.codec.%s", strdesc.codec);
    // try open this class
    pCodec = (iCodec*)pSCF->scfCreateInstance (cn, "iCodec", 0);
    pSCF->DecRef ();
    if (pCodec)
    {
      // codec exists, now try to initialize it
      if (pCodec->Initialize (&strdesc))
      {
	pCodec->GetCodecDescription (cdesc);
       	return true;
      }
      else
      {
	pSystem->Printf (MSG_WARNING, "CODEC class \"%s\" could not be initialized !", cn);
	pCodec->DecRef ();
	pCodec = NULL;
      }
    }
    else
      pSystem->Printf (MSG_WARNING, "CODEC class \"%s\" could not be loaded !", cn);
  }
  else
    pSystem->Printf (MSG_DEBUG_0, "Could not get an SCF interface from the systemdriver !");
  return false;
}
