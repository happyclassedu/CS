/*
    Copyright (C) 2004 by Jorrit Tyberghein
	      (C) 2004 by Frank Richter

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

#include "csutil/databuf.h"
#include "csutil/util.h"
#include "csutil/xmltiny.h"
#include "csgfx/shaderexp.h"

#include "imap/services.h"
#include "iutil/verbositymanager.h"
#include "iutil/vfs.h"
#include "ivaria/reporter.h"

#include "csplugincommon/shader/shaderprogram.h"

CS_LEAKGUARD_IMPLEMENT (csShaderProgram);


csShaderProgram::csShaderProgram (iObjectRegistry* objectReg)
  : scfImplementationType (this)
{
  InitCommonTokens (commonTokens);

  csShaderProgram::objectReg = objectReg;
  synsrv = CS_QUERY_REGISTRY (objectReg, iSyntaxService);
  strings = CS_QUERY_REGISTRY_TAG_INTERFACE (objectReg, 
    "crystalspace.shared.stringset", iStringSet);
  
  csRef<iVerbosityManager> verbosemgr (
    CS_QUERY_REGISTRY (objectReg, iVerbosityManager));
  if (verbosemgr) 
    doVerbose = verbosemgr->Enabled("renderer.shader");
  else
    doVerbose = false;
}

csShaderProgram::~csShaderProgram ()
{
}

bool csShaderProgram::ParseProgramParam (iDocumentNode* node,
  ProgramParam& param, uint types)
{
  const char* type = node->GetAttributeValue ("type");
  if (type == 0)
  {
    synsrv->Report ("crystalspace.graphics3d.shader.common",
      CS_REPORTER_SEVERITY_WARNING,
      node,
      "No 'type' attribute");
    return false;
  }

  // Var for static data
  csRef<csShaderVariable> var;
  var.AttachNew (new csShaderVariable (csInvalidStringID));

  ProgramParamType paramType = ParamInvalid;
  if (strcmp (type, "shadervar") == 0)
  {
    const char* value = node->GetContentsValue();
    if (!value)
    {
      synsrv->Report ("crystalspace.graphics3d.shader.common",
	CS_REPORTER_SEVERITY_WARNING,
	node,
	"Node has no contents");
      return false;
    }
    param.name = strings->Request (value);
    param.valid = true;
    return true;
  }
  else if (strcmp (type, "float") == 0)
  {
    paramType = ParamFloat;
  }
  else if (strcmp (type, "vector2") == 0)
  {
    paramType = ParamVector2;
  }
  else if (strcmp (type, "vector3") == 0)
  {
    paramType = ParamVector3;
  }
  else if (strcmp (type, "vector4") == 0)
  {
    paramType = ParamVector4;
  }
  else if (strcmp (type, "matrix") == 0)
  {
    paramType = ParamMatrix;
  }
  else if (strcmp (type, "transform") == 0)
  {
    paramType = ParamTransform;
  }
  else if ((strcmp (type, "expression") == 0) || (strcmp (type, "expr") == 0))
  {
    // Parse exp and save it
    csRef<iShaderVariableAccessor> acc = synsrv->ParseShaderVarExpr (node);
    var->SetType (csShaderVariable::VECTOR4);
    var->SetAccessor (acc);
    param.var = var;
    param.valid = true;
    return true;
  }
  else if (strcmp (type, "array") == 0)
  {
    csArray<ProgramParam> allParams;
    ProgramParam tmpParam;
    csRef<iDocumentNodeIterator> it = node->GetNodes ();
    while (it->HasNext ())
    {
      csRef<iDocumentNode> child = it->Next ();
      ParseProgramParam (child, tmpParam, types & 0x3F);
      allParams.Push (tmpParam);
    }

    //Save the params
    var->SetType (csShaderVariable::ARRAY);
    var->SetArraySize (allParams.Length ());

    for (uint i = 0; i < allParams.Length (); i++)
    {
      var->SetArrayElement (i, allParams[i].var);
    }
    paramType = ParamArray;
  }
  else 
  {
    synsrv->Report ("crystalspace.graphics3d.shader.common",
      CS_REPORTER_SEVERITY_WARNING,
      node,
      "Unknown type '%s'", type);
    return false;
  }

  if (!(types & paramType))
  {
    synsrv->Report ("crystalspace.graphics3d.shader.common",
      CS_REPORTER_SEVERITY_WARNING,
      node,
      "Type '%s' not supported by this parameter", type);
    return false;
  }

  switch ((paramType & 0x3F))
  {
    case ParamInvalid:
      return false;
      break;
    case ParamFloat:
      {
	float x = node->GetContentsValueAsFloat ();
	var->SetValue (x);
      }
      break;
    case ParamVector2:
      {
	float x, y;
	const char* value = node->GetContentsValue();
	if (!value)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING,
	    node,
	    "Node has no contents");
	  return false;
	}
	if (sscanf (value, "%f,%f", &x, &y) != 2)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING,
	    node,
	    "Couldn't parse vector2 '%s'", value);
	  return false;
	}
	var->SetValue (csVector2 (x,y));
      }
      break;
    case ParamVector3:
      {
	float x, y, z;
	const char* value = node->GetContentsValue();
	if (!value)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING,
	    node,
	    "Node has no contents");
	  return false;
	}
	if (sscanf (value, "%f,%f,%f", &x, &y, &z) != 3)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING,
	    node,
	    "Couldn't parse vector3 '%s'", value);
	  return false;
	}
	var->SetValue (csVector3 (x,y,z));
      }
      break;
    case ParamVector4:
      {
	float x, y, z, w;
	const char* value = node->GetContentsValue();
	if (!value)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING,
	    node,
	    "Node has no contents");
	  return false;
	}
	if (sscanf (value, "%f,%f,%f,%f", &x, &y, &z, &w) != 4)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING,
	    node,
	    "Couldn't parse vector4 '%s'", value);
	  return false;
	}
	var->SetValue (csVector4 (x,y,z,w));
      }
      break;
    case ParamMatrix:
      {
        csMatrix3 matrix;
	if (!synsrv->ParseMatrix (node, matrix))
	  return false;
        var->SetValue (matrix);
      }
      break;
    case ParamTransform:
      {
        csReversibleTransform t;
        csRef<iDocumentNode> matrix_node = node->GetNode ("matrix");
        if (matrix_node)
        {
          csMatrix3 m;
          if (!synsrv->ParseMatrix (matrix_node, m))
            return false;
          t.SetT2O (m);
        }
        csRef<iDocumentNode> vector_node = node->GetNode ("v");
        if (vector_node)
        {
          csVector3 v;
          if (!synsrv->ParseVector (vector_node, v))
            return false;
          t.SetOrigin (v);
        }
        var->SetValue (t);
      }
      break;
  }
  
  param.var = var;
  param.valid = true;
  return true;
}

bool csShaderProgram::ParseCommon (iDocumentNode* child)
{
  const char* value = child->GetValue ();
  csStringID id = commonTokens.Request (value);
  switch (id)
  {
    case XMLTOKEN_VARIABLEMAP:
      {
        //@@ REWRITE
	const char* destname = child->GetAttributeValue ("destination");
	if (!destname)
	{
	  synsrv->Report ("crystalspace.graphics3d.shader.common",
	    CS_REPORTER_SEVERITY_WARNING, child,
	    "<variablemap> has no 'destination' attribute");
	  return false;
	}

	const char* varname = child->GetAttributeValue ("variable");
	if (!varname)
	{
	  // "New style" variable mapping
	  VariableMapEntry vme (csInvalidStringID, destname);
	  if (!ParseProgramParam (child, vme.mappingParam,
	    ParamFloat | ParamVector2 | ParamVector3 | ParamVector4))
	    return false;
	  variablemap.Push (vme);
	}
	else
	{
	  // "Classic" variable mapping
	  variablemap.Push (VariableMapEntry (strings->Request (varname),
	    destname));
	}
      }
      break;
    case XMLTOKEN_PROGRAM:
      {
	const char* filename = child->GetAttributeValue ("file");
	if (filename != 0)
	{
	  programFileName = filename;

	  csRef<iVFS> vfs = CS_QUERY_REGISTRY (objectReg, iVFS);
	  csRef<iFile> file = vfs->Open (filename, VFS_FILE_READ);
	  if (!file.IsValid())
	  {
	    synsrv->Report ("crystalspace.graphics3d.shader.common",
	      CS_REPORTER_SEVERITY_WARNING, child,
	      "Could not open '%s'", filename);
	    return false;
	  }

	  programFile = file;
	}
	else
	  programNode = child;
      }
      break;

    case XMLTOKEN_DESCRIPTION:
      description = child->GetContentsValue();
      break;
    default:
      synsrv->ReportBadToken (child);
      return false;
  }
  return true;
}

iDocumentNode* csShaderProgram::GetProgramNode ()
{
  if (programNode.IsValid ())
    return programNode;

  if (programFile.IsValid ())
  {
    csRef<iDocumentSystem> docsys = CS_QUERY_REGISTRY (objectReg, 
      iDocumentSystem);
    if (!docsys)
      docsys.AttachNew (new csTinyDocumentSystem ());
    csRef<iDocument> doc (docsys->CreateDocument ());

    const char* err = doc->Parse (programFile, true);
    if (err != 0)
    {
      csReport (objectReg,
	CS_REPORTER_SEVERITY_WARNING, 
	"crystalspace.graphics3d.shader.common",
	"Error parsing %s: %s", programFileName.GetData(), err);
      return 0;
    }
    programNode = doc->GetRoot ();
    programFile = 0;
    return programNode;
  }

  return 0;
}

csPtr<iDataBuffer> csShaderProgram::GetProgramData ()
{
  if (programFile.IsValid())
  {
    return programFile->GetAllData ();
  }

  if (programNode.IsValid())
  {
    char* data = csStrNew (programNode->GetContentsValue ());

    csRef<iDataBuffer> newbuff;
    newbuff.AttachNew (new csDataBuffer (data, data ? strlen (data) : 0));
    return csPtr<iDataBuffer> (newbuff);
  }

  return 0;
}

void csShaderProgram::DumpProgramInfo (csString& output)
{
  output << "Program description: " << 
    (description.Length () ? description.GetData () : "<none>") << "\n";
  output << "Program file name: " << programFileName << "\n";
}

void csShaderProgram::DumpVariableMappings (csString& output)
{
  for (size_t v = 0; v < variablemap.Length(); v++)
  {
    const VariableMapEntry& vme = variablemap[v];

    output << strings->Request (vme.name);
    output << '(' << vme.name << ") -> ";
    output << vme.destination << ' ';
    output << vme.userVal << ' ';
    output << '\n'; 
  }
}
