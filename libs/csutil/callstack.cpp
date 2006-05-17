/*
  Copyright (C) 2005 by Jorrit Tyberghein
            (C) 2005 by Frank Richter

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
#include "csutil/callstack.h"
#include "callstack.h"

#ifdef CS_HAVE_BACKTRACE
  #include "generic/callstack-backtrace.h"
  CS_IMPLEMENT_STATIC_VAR(cscBacktrace, 
			  CS::Debug::CallStackCreatorBacktrace, ())
  CS_IMPLEMENT_STATIC_VAR(csnrBacktrace, 
			  CS::Debug::CallStackNameResolverBacktrace, ())
#endif

#ifdef CS_PLATFORM_WIN32
  #include "win32/callstack-dbghelp.h"
  CS_IMPLEMENT_STATIC_VAR(cscDbgHelp, 
			  CS::Debug::CallStackCreatorDbgHelp, ())
  CS_IMPLEMENT_STATIC_VAR(csnrDbgHelp, 
			  CS::Debug::CallStackNameResolverDbgHelp, ())

  #ifdef CS_HAVE_LIBBFD
    #include "win32/callstack-bfd.h"
    CS_IMPLEMENT_STATIC_VAR(csnrBfd, 
			    CS::Debug::CallStackNameResolverBfd, ())
  #endif
#endif

namespace CS
{
namespace Debug
{

static const void* const callStackCreators[] =
{
#ifdef CS_PLATFORM_WIN32
  (void*)&cscDbgHelp,
#endif
#ifdef CS_HAVE_BACKTRACE
  (void*)&cscBacktrace,
#endif
  0
};

static const void* const callStackNameResolvers[] =
{
#ifdef CS_PLATFORM_WIN32
  #ifdef CS_HAVE_LIBBFD
    (void*)&csnrBfd,
  #endif
  (void*)&csnrDbgHelp,
#endif
#ifdef CS_HAVE_BACKTRACE
  (void*)&csnrBacktrace,
#endif
  0
};

void CallStackImpl::Free() { delete this; }

size_t CallStackImpl::GetEntryCount ()
{
  return entries.Length();
}

typedef iCallStackNameResolver* (*NameResolveGetter)();

bool CallStackImpl::GetFunctionName (size_t num, csString& str)
{
  csString sym, mod;
  const void* const* resGetter = callStackNameResolvers;
  while (*resGetter != 0)
  {
    iCallStackNameResolver* res = ((NameResolveGetter)*resGetter)();
    if (res->GetAddressSymbol (entries[num].address, str))
    {
      return true;
    }
    resGetter++;
  }
  
  // Return at least the address...
  str.Format ("[%p]", entries[num].address);
  return true;
}

bool CallStackImpl::GetLineNumber (size_t num, csString& str)
{
  const void* const* resGetter = callStackNameResolvers;
  while (*resGetter != 0)
  {
    iCallStackNameResolver* res = ((NameResolveGetter)*resGetter)();
    if (res->GetLineNumber (entries[num].address, str))
      return true;
    resGetter++;
  }
  return false;
}

bool CallStackImpl::GetParameters (size_t num, csString& str)
{
  if (entries[num].paramNum == csParamUnknown) return false;
  const void* const* resGetter = callStackNameResolvers;
  while (*resGetter != 0)
  {
    void* h;
    iCallStackNameResolver* res = ((NameResolveGetter)*resGetter)();
    if ((h = res->OpenParamSymbols (entries[num].address)))
    {
      str.Clear();
      csString parm;
      for (size_t i = 0; i < entries[num].paramNum; i++)
      {
        parm.Clear();
        if (!res->GetParamName (h, i, parm)) 
          parm.Format ("unk%zu", i);
        if (i > 0) str << ", ";
        str << parm << " = ";
        str.AppendFmt ("%" PRIdPTR "(0x%" PRIxPTR ")",
          params[entries[num].paramOffs + i],
          params[entries[num].paramOffs + i]);
      }
      res->FreeParamSymbols (h);
      return true;
    }
    resGetter++;
  }
  return false;
}

} // namespace Debug
} // namespace CS

using namespace CS::Debug;

typedef iCallStackCreator* (*CreatorGetter)();

csCallStack* csCallStackHelper::CreateCallStack (int skip, bool fast)
{
  skip += 1; /* Adjust for this function */
  CallStackImpl* stack = new CallStackImpl();
  const void* const* cscGetter = callStackCreators;
  while (*cscGetter != 0)
  {
    iCallStackCreator* csc = ((CreatorGetter)*cscGetter)();
    if (csc->CreateCallStack (stack->entries, stack->params, fast))
    {
      // Remove one more so CreateCallstack() itself does not appear
      stack->entries.DeleteRange (0, (size_t)(skip));
      stack->entries.ShrinkBestFit();
      stack->params.ShrinkBestFit();
      return stack;
    }
    cscGetter++;
  }
  delete stack;
  return 0;
}
