/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   $Id$

    This file is part of Crystal Space Virtual Object System Abstract
    3D Layer plugin (csvosa3dl).

    Copyright (C) 2004 Peter Amstutz

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _CSVOSOBJECT3D_H_
#define _CSVOSOBJECT3D_H_

#include <vos/metaobjects/a3dl/object3d.hh>
#include <vos/metaobjects/property/propertylistener.hh>

#include "inetwork/vosa3dl.h"
#include "iengine/mesh.h"

#include "csvosa3dl.h"
#include "vossector.h"

class csVosObject3D : public iVosObject3D
{
private:
  csRef<iMeshWrapper> meshwrapper;

public:
  SCF_DECLARE_IBASE;

  csVosObject3D();
  virtual ~csVosObject3D();

  virtual csRef<iMeshWrapper> GetMeshWrapper();

  void SetMeshWrapper(iMeshWrapper* mw);
};

class csMetaObject3D : public virtual A3DL::Object3D,
                       public VOS::PropertyListener
{
protected:
  csRef<csVosObject3D> csvobj3d;
  csVosA3DL *vosa3dl;

public:
  csMetaObject3D(VOS::VobjectBase* superobject);
  virtual ~csMetaObject3D();

  static VOS::MetaObject* new_csMetaObject3D(VOS::VobjectBase* superobject, 
                                             const std::string& type);

  // Set up the object
  virtual void Setup(csVosA3DL* vosa3dl, csVosSector* sect);
  csRef<csVosObject3D> GetCSinterface();

  // Property listener and callbacks for object property events
  virtual void notifyPropertyChange(const VOS::PropertyEvent &event);

  // Call these from CS run loop.  Derived objects can override if they
  // do not correctly observe movable interface
  virtual void changePosition(const csVector3 &pos);
  virtual void changeTransform(const csMatrix3 &trans);
};

#endif
