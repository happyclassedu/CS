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

#ifndef __CS_SHADERPLUGINS_COMMON_SHADERPROGRAM_H__
#define __CS_SHADERPLUGINS_COMMON_SHADERPROGRAM_H__

#include "csutil/array.h"
#include "csutil/csstring.h"
#include "csutil/ref.h"
#include "csutil/strhash.h"
#include "csgfx/shadervar.h"
#include "csgfx/shadervarcontext.h"

#include "iutil/databuff.h"
#include "iutil/document.h"
#include "iutil/objreg.h"
#include "iutil/strset.h"
#include "iutil/vfs.h"
#include "imap/services.h"

#include "../../common/shaderplugin.h"

struct iDataBuffer;

class csShaderProgram : public iShaderProgram
{
protected:
  csStringHash commonTokens;
private:
#define CS_INIT_TOKEN_TABLE_NAME InitCommonTokens
#define CS_TOKEN_ITEM_FILE \
  "video/render3d/shader/shaderplugins/common/shaderprogram.tok"
#include "cstool/tokenlist.h"

protected:
  csRef<iObjectRegistry> objectReg;
  csRef<iSyntaxService> synsrv;
  csRef<iStringSet> strings;

  struct VariableMapEntry : public csShaderVarMapping
  {
    // Variables that can be resolved statically at shader load
    // or compilation is put in "statlink"
    csRef<csShaderVariable> statlink;
    int userInt;
    void* userPtr;

    VariableMapEntry (csStringID s, const char* d) : 
      csShaderVarMapping (s, d)
    { 
      userInt = 0;
      userPtr = 0;
    }
  };

  csString description;
  csArray<VariableMapEntry> variablemap;
  csRef<iDocumentNode> programNode;
  csRef<iFile> programFile;
  csString programFileName;

  csShaderVariableContext svcontext;

  bool ParseCommon (iDocumentNode* child);
  iDocumentNode* GetProgramNode ();
  csPtr<iDataBuffer> GetProgramData ();
  void ResolveStaticVars (csArray<iShaderVariableContext*> &staticContexts);
public:
  SCF_DECLARE_IBASE;

  csShaderProgram (iObjectRegistry* objectReg);
  virtual ~csShaderProgram ();
};

#endif // __CS_SHADERPLUGINS_COMMON_SHADERPROGRAM_H__
