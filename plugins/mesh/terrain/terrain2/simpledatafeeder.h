/*
  Copyright (C) 2006 by Kapoulkine Arseny
                2007 by Marten Svanfeldt

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

#ifndef __CS_TERRAIN_SIMPLEDATAFEEDER_H__
#define __CS_TERRAIN_SIMPLEDATAFEEDER_H__

#include "csutil/scf_implementation.h"
#include "csutil/csstring.h"

#include "imesh/terrain2.h"
#include "iutil/comp.h"

CS_PLUGIN_NAMESPACE_BEGIN(Terrain2)
{
struct csTerrainSimpleDataFeederProperties
  : public scfImplementation1<csTerrainSimpleDataFeederProperties,
                              iTerrainCellFeederProperties>
{
  csTerrainSimpleDataFeederProperties ();
  csTerrainSimpleDataFeederProperties (csTerrainSimpleDataFeederProperties& other);

  // ---- iTerrainCellFeederProperties ----
  virtual void SetHeightmapSource (const char* source);
  virtual void SetParameter (const char* param, const char* value);
  virtual csPtr<iTerrainCellFeederProperties> Clone ();

  csString heightmapSource, heightmapFormat, materialmapSource;
  float offset;
};

class csTerrainSimpleDataFeeder :
  public scfImplementation2<csTerrainSimpleDataFeeder,
                            iTerrainDataFeeder,
                            iComponent>
{
public:
  csTerrainSimpleDataFeeder (iBase* parent);

  virtual ~csTerrainSimpleDataFeeder ();

  // ------------ iTerrainDataFeeder implementation ------------
  virtual csPtr<iTerrainCellFeederProperties> CreateProperties ();

  virtual bool PreLoad (iTerrainCell* cell);

  virtual bool Load (iTerrainCell* cell);

  virtual void SetParameter (const char* param, const char* value);

  // ------------ iComponent implementation ------------
  virtual bool Initialize (iObjectRegistry* object_reg);

protected:
  iObjectRegistry* objectReg;
  csRef<iLoader> loader;
};

}
CS_PLUGIN_NAMESPACE_END(Terrain2)

#endif // __CS_TERRAIN_SIMPLEDATAFEEDER_H__
