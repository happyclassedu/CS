/*
    Copyright (C) 2002 by M�rten Svanfeldt
                          Anders Stenberg

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
#include <limits.h>
#include "cstypes.h"
#include "csutil/ref.h"
#include "csutil/scf.h"
#include "csutil/scfstrset.h"
#include "csgeom/vector3.h"
#include "csutil/objreg.h"
#include "csutil/scfstr.h"
#include "csgeom/vector4.h"
#include "csutil/hashmap.h"
#include "csutil/xmltiny.h"

#include "iengine/engine.h"
#include "iengine/texture.h"
#include "iengine/material.h"
#include "igeom/clip2d.h"
#include "imap/services.h"
#include "iutil/document.h"
#include "iutil/vfs.h"
#include "iutil/eventq.h"
#include "iutil/virtclk.h"
#include "iutil/comp.h"
#include "iutil/plugin.h"
#include "ivaria/reporter.h"
#include "ivideo/texture.h"

#include "ivideo/shader/shader.h"
#include "ivideo/graph3d.h"
#include "ivideo/rndbuf.h"
#include "ivideo/rendermesh.h"
#include "shadermgr.h"

// Pluginstuff
CS_IMPLEMENT_PLUGIN

SCF_IMPLEMENT_IBASE (csShaderManager)  
  SCF_IMPLEMENTS_INTERFACE (iShaderManager)
  SCF_IMPLEMENTS_EMBEDDED_INTERFACE (iComponent)
SCF_IMPLEMENT_IBASE_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csShaderManager::Component)
  SCF_IMPLEMENTS_INTERFACE (iComponent)
SCF_IMPLEMENT_EMBEDDED_IBASE_END

SCF_IMPLEMENT_IBASE (csShaderManager::EventHandler)
  SCF_IMPLEMENTS_INTERFACE (iEventHandler)
SCF_IMPLEMENT_IBASE_END

SCF_IMPLEMENT_FACTORY (csShaderManager)


//=================== csShaderManager ================//

// General stuff
csShaderManager::csShaderManager(iBase* parent)
{
  SCF_CONSTRUCT_IBASE(parent);
  SCF_CONSTRUCT_EMBEDDED_IBASE(scfiComponent);
  scfiEventHandler = 0;

  seqnumber = 0;
}

csShaderManager::~csShaderManager()
{
  //clear all shaders
  shaders.DeleteAll ();
  if (scfiEventHandler) scfiEventHandler->DecRef();
}

void csShaderManager::Report (int severity, const char* msg, ...)
{
  va_list args;
  va_start (args, msg);
  csReportV (objectreg, severity, "crystalspace.graphics3d.shadermgr", 
    msg, args);
  va_end (args);
}

bool csShaderManager::Initialize(iObjectRegistry *objreg)
{
  objectreg = objreg;
  vc = CS_QUERY_REGISTRY (objectreg, iVirtualClock);
  txtmgr = CS_QUERY_REGISTRY (objectreg, iTextureManager);

  if (!scfiEventHandler)
    scfiEventHandler = new EventHandler (this);

  csRef<iEventQueue> q = CS_QUERY_REGISTRY (objectreg, iEventQueue);
  if (q)
    q->RegisterListener (scfiEventHandler,
	CSMASK_Broadcast);

  csRef<iPluginManager> plugin_mgr = CS_QUERY_REGISTRY  (objectreg,
	iPluginManager);

  csRef<iStringArray> classlist = csPtr<iStringArray> 
    (iSCF::SCF->QueryClassList("crystalspace.graphics3d.shadercompiler."));
  int const nmatches = classlist->Length();
  if (nmatches != 0)
  {
    int i;
    for (i = 0; i < nmatches; ++i)
    {
      const char* classname = classlist->Get(i);
      csRef<iShaderCompiler> plugin = 
	CS_LOAD_PLUGIN (plugin_mgr, classname, iShaderCompiler);
      if (plugin)
      {
        Report (CS_REPORTER_SEVERITY_NOTIFY, "Loaded compilerplugin %s, compiler: %s", 
          classname,plugin->GetName());
        RegisterCompiler(plugin);
      }
    }
  }
  else
  {
    Report (CS_REPORTER_SEVERITY_WARNING, "No shader plugins found!");
  }

  return true;
}

void csShaderManager::Open ()
{
}

void csShaderManager::Close ()
{
}

bool csShaderManager::HandleEvent(iEvent& event)
{
  if (event.Type == csevBroadcast)
  {
    switch(event.Command.Code)
    {
      case cscmdPreProcess:
  //      UpdateStandardVariables();
	return false;
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
  }
  return false;
}


void csShaderManager::RegisterShader(iShader* shader)
{
  shaders.Push(shader);
}

iShader* csShaderManager::GetShader(const char* name)
{
  int i;
  for (i = 0; i < shaders.Length(); ++i)
  {
    iShader* shader = shaders.Get(i);
    if (strcasecmp(shader->GetName(), name) == 0)
      return shader;
  }
  return 0;
}

void csShaderManager::RegisterCompiler(iShaderCompiler* compiler)
{
  compilers.Push(compiler);
}

iShaderCompiler* csShaderManager::GetCompiler(const char* name)
{
  int i;
  for (i = 0; i < compilers.Length(); ++i)
  {
    iShaderCompiler* compiler = compilers.Get(i);
    if (strcasecmp(compiler->GetName(), name) == 0)
      return compiler;
  }
  return 0;
}