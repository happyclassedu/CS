/*
    Copyright (C) 1999,2000 by Andrew Zabolotny
    Crystal Space Shared Class Facility (SCF)

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
#include "cssys/csshlib.h"
#include "cssys/sysfunc.h"
#include "cssys/syspath.h"
#include "csutil/csstring.h"
#include "csutil/csvector.h"
#include "csutil/memfile.h"
#include "csutil/ref.h"
#include "csutil/scf.h"
#include "csutil/scfstrv.h"
#include "csutil/scopedmutexlock.h"
#include "csutil/strset.h"
#include "csutil/util.h"
#include "csutil/xmltiny.h"
#include "iutil/document.h"

/// This is the registry for all class factories
static class scfClassRegistry *ClassRegistry = 0;
/// If this bool is true, we should sort the registery
static bool SortClassRegistry = false;
/// This is our private instance of csSCF
static class csSCF *PrivateSCF = 0;

/**
 * This class manages all SCF functionality.
 * The SCF module loader routines (CreateInstance, RegisterClass, ...)
 * are all thread-safe.
 */
class csSCF : public iSCF
{
private:
  csRef<csMutex> mutex;

  csStringSet contexts;

  void RegisterClassesInt (char const* pluginPath, iDocumentNode* scfnode, 
    const char* context = 0);
public:
  SCF_DECLARE_IBASE;

  /// The global table of all known interface names
  csStringSet InterfaceRegistry;
  
  /// constructor
  csSCF ();
  /// destructor
  virtual ~csSCF ();

  virtual void RegisterClasses (iDocument*);
  virtual void RegisterClasses (char const*);
  virtual void RegisterClasses (char const* pluginPath, 
    iDocument* scfnode, const char* context = 0);
  virtual bool RegisterClass (const char *iClassID,
    const char *iLibraryName, const char *iFactoryClass,
    const char *Description, const char *Dependencies = 0, 
    const char* context = 0);
  virtual bool RegisterClass (scfFactoryFunc, const char *iClassID,
    const char *Description, const char *Dependencies = 0, 
    const char* context = 0);
  virtual bool RegisterFactoryFunc (scfFactoryFunc, const char *FactClass);
  virtual bool ClassRegistered (const char *iClassID);
  virtual void *CreateInstance (const char *iClassID,
    const char *iInterfaceID, int iVersion);
  virtual const char *GetClassDescription (const char *iClassID);
  virtual const char *GetClassDependencies (const char *iClassID);
  virtual bool UnregisterClass (const char *iClassID);
  virtual void UnloadUnusedModules ();
  virtual void ScanPluginsPath (const char* path, bool recursive = false,
    const char* context = 0);
  virtual scfInterfaceID GetInterfaceID (const char *iInterface);
  virtual void Finish ();
  virtual iStrVector* QueryClassList (char const* pattern);
};

#ifndef CS_STATIC_LINKED

class scfLibraryVector : public csVector
{
public:
  virtual bool FreeItem (void* Item);
  virtual int CompareKey (void* Item, const void* Key, int Mode) const;
};

// This is the registry for all shared libraries
static scfLibraryVector *LibraryRegistry = 0;

/// A object of this class represents a shared library
class scfSharedLibrary
{
  friend class scfLibraryVector;
  friend class scfFactory;
  // Shared library name
  char *LibraryName;
  // Handle of shared module (if RefCount > 0)
  csLibraryHandle LibraryHandle;
  // Number of references to this shared library
  int RefCount;
  // Base name used for composition of library intialize and shutdown
  // functions.  All exported factory classes will implement initialize and
  // shutdown functions, but we choose only a single pair to perform this work
  // on behalf of the library.
  char *FuncCoreName;

public:
  /// Create a shared library and load it
  scfSharedLibrary (const char *iLibraryName, const char *iCoreName);

  /// Destroy a shared library object
  virtual ~scfSharedLibrary ();

  /// Check if library object is okay
  bool ok () const
  { return (LibraryHandle != 0); }

  /// Increment reference count for the library
  void IncRef ()
  { RefCount++; }

  /// Decrement reference count for the library
  void DecRef ()
  { RefCount--; }

  /// Get the reference count.
  int GetRefCount () const
  { return RefCount; }

  /// Try to free the library, if refcount is zero
  bool TryUnload ()
  {
    if (RefCount <= 0)
    {
      LibraryRegistry->Delete (LibraryRegistry->Find (this));
      return true;
    }
    return false;
  }
};

scfSharedLibrary::scfSharedLibrary (const char *lib, const char *core)
{
  LibraryRegistry->Push (this);

  RefCount = 0;
  LibraryName = csStrNew (lib);
  FuncCoreName = csStrNew (core);
  LibraryHandle = csLoadLibrary (LibraryName);

  if (LibraryHandle != 0)
  {
    typedef void (*scfInitFunc)(iSCF*);
    csString sym;
    sym << FuncCoreName << "_scfInitialize";
    scfInitFunc initfunc = (scfInitFunc)csGetLibrarySymbol(LibraryHandle, sym);
    if (initfunc)
      initfunc(PrivateSCF);
    else
      csPrintLibraryError(sym);
  }
}

scfSharedLibrary::~scfSharedLibrary ()
{
  if (LibraryHandle)
  {
    typedef void (*scfFinisFunc)();
    csString sym;
    sym << FuncCoreName << "_scfFinalize";
    scfFinisFunc func = (scfFinisFunc)csGetLibrarySymbol(LibraryHandle, sym);
    if (func)
      func();
    else
      csPrintLibraryError(sym);
    csUnloadLibrary (LibraryHandle);
  }
  delete [] FuncCoreName;
  delete [] LibraryName;
}

bool scfLibraryVector::FreeItem (void* Item)
{
  delete (scfSharedLibrary *)Item;
  return true;
}

int scfLibraryVector::CompareKey (void* Item, const void* Key, int) const
{
  return (strcmp (((scfSharedLibrary *)Item)->LibraryName, (char *)Key));
}

#endif // CS_STATIC_LINKED

/// This structure contains everything we need to know about a particular class
class scfFactory : public iFactory
{
public:
  SCF_DECLARE_IBASE;

  // Class identifier
  char *ClassID;
  // Class description
  char *Description;
  // The dependency list
  char *Dependencies;
  // Name of factory implementation class in shared library.
  char *FactoryClass;
  // Function which actually creates an instance
  scfFactoryFunc CreateFunc;
  /*
    "Context" of this class (used to decide whether a class conflict will 
    be reported)
  */
  csStringID classContext;
#ifndef CS_STATIC_LINKED
  // Shared module that implements this class or 0 for local classes
  char *LibraryName;
  // A pointer to shared library object (0 for local classes)
  scfSharedLibrary *Library;
#endif

  // Create the factory for a class located in a shared library
  scfFactory (const char *iClassID, const char *iLibraryName,
    const char *iFactoryClass, scfFactoryFunc iCreateFunc, const char *iDesc,
    const char *iDepend, csStringID context = csInvalidStringID);
  // Free the factory object (but not objects created by this factory)
  virtual ~scfFactory ();
  // Create a insance of class this factory represents
  virtual void *CreateInstance ();
  // Try to unload class module (i.e. shared module)
  virtual void TryUnload ();
  /// Query class description string
  virtual const char *QueryDescription ();
  /// Query class dependency strings
  virtual const char *QueryDependencies ();
  /// Query class ID
  virtual const char *QueryClassID();
};

/// This class holds a number of scfFactory structures
class scfClassRegistry : public csVector
{
public:
  scfClassRegistry () : csVector (16, 16) {}
  virtual ~scfClassRegistry () { DeleteAll (); }
  virtual bool FreeItem (void* Item)
  { delete (scfFactory *)Item; return true; }
  virtual int CompareKey (void* Item, const void* Key, int) const
  { return strcmp (((scfFactory *)Item)->ClassID, (char *)Key); }
  virtual int Compare (void* Item1, void* Item2, int) const
  { return strcmp (((scfFactory *)Item1)->ClassID,
                   ((scfFactory *)Item2)->ClassID); }
};

//----------------------------------------- Class factory implementation ----//

scfFactory::scfFactory (const char *iClassID, const char *iLibraryName,
  const char *iFactoryClass, scfFactoryFunc iCreate, const char *iDescription,
  const char *iDepend, csStringID context)
{
  // Don't use SCF_CONSTRUCT_IBASE (0) since it will call IncRef()
  scfRefCount = 0; scfParent = 0;
  ClassID = csStrNew (iClassID);
  Description = csStrNew (iDescription);
  Dependencies = csStrNew (iDepend);
  FactoryClass = csStrNew (iFactoryClass);
  CreateFunc = iCreate;
  classContext = context;
#ifndef CS_STATIC_LINKED
  LibraryName = csStrNew (iLibraryName);
  Library = 0;
#else
  (void)iLibraryName;
#endif
}

scfFactory::~scfFactory ()
{
#ifdef CS_DEBUG
  // Warn user about unreleased instances of this class
  if (scfRefCount)
    fprintf (stderr, "SCF WARNING: %d unreleased instances of class %s!\n",
      scfRefCount, ClassID);
#endif

#ifndef CS_STATIC_LINKED
  if (Library)
    Library->DecRef ();
  delete [] LibraryName;
#endif
  delete [] FactoryClass;
  delete [] Dependencies;
  delete [] Description;
  delete [] ClassID;
}

void scfFactory::IncRef ()
{
#ifndef CS_STATIC_LINKED
  if (!Library && LibraryName)
  {
    int libidx = LibraryRegistry->FindKey (LibraryName);
    if (libidx >= 0)
      Library = (scfSharedLibrary *)LibraryRegistry->Get (libidx);
    else
      Library = new scfSharedLibrary (LibraryName, FactoryClass);

    if (Library->ok ())
    {
      if (CreateFunc == 0)
      {
        csString sym;
	sym << FactoryClass << "_Create";
	CreateFunc =
	  (scfFactoryFunc)csGetLibrarySymbol(Library->LibraryHandle, sym);
	if (CreateFunc == 0)
	  csPrintLibraryError(sym);
      }
    }

    if (!Library->ok () || CreateFunc == 0)
    {
      Library = 0;
      return; // Signify that IncRef() failed by _not_ incrementing count.
    }
    
    Library->IncRef ();
  }
#endif
  scfRefCount++;
}

void scfFactory::DecRef ()
{
#ifdef CS_DEBUG
  if (scfRefCount == 0)
  {
    fprintf (stderr, "SCF WARNING: Extra calls to scfFactory::DecRef () for "
      "class %s\n", ClassID);
    return;
  }
#endif
  scfRefCount--;
#ifndef CS_STATIC_LINKED
  if (scfRefCount == 0)
  {
    // now we no longer need the library either
    if (Library)
    {
      Library->DecRef ();
      Library = 0;
    }
  }
#endif
}

int scfFactory::GetRefCount ()
{
  return scfRefCount;
}

void *scfFactory::QueryInterface (scfInterfaceID iInterfaceID, int iVersion)
{
  SCF_IMPLEMENTS_INTERFACE (iFactory);
  return 0;
}

void *scfFactory::CreateInstance ()
{
  IncRef ();
  // If IncRef won't succeed, we'll have a zero reference counter
  if (!scfRefCount)
    return 0;

  void *instance = CreateFunc(this);

  // No matter whenever we succeeded or not, decrement the refcount
  DecRef ();

  return instance;
}

void scfFactory::TryUnload ()
{
}

const char *scfFactory::QueryDescription ()
{
  if (scfRefCount)
    return Description;
  else
    return 0;
}

const char *scfFactory::QueryDependencies ()
{
  return Dependencies;
}

const char *scfFactory::QueryClassID ()
{
  return ClassID;
}

//------------------------------------ Implementation of csSCF functions ----//

SCF_IMPLEMENT_IBASE (csSCF);
  SCF_IMPLEMENTS_INTERFACE (iSCF);
SCF_IMPLEMENT_IBASE_END;

static void scfScanPlugins (csPluginPaths* pluginPaths, const char* context)
{
  if (!PrivateSCF)
    PrivateSCF = new csSCF ();

#ifndef CS_STATIC_LINKED
  if (pluginPaths)
  {
    // Search plugins in pluginpaths
    csRef<iStrVector> plugins;
    csRefArray<iDocument> metadata;

    int i, j;
    for (i = 0; i < pluginPaths->GetCount(); i++)
    {
      if (plugins) plugins->DeleteAll();
      metadata.DeleteAll();

      csRef<iStrVector> messages = 
	csScanPluginDir ((*pluginPaths)[i].path, plugins, metadata,
	  (*pluginPaths)[i].scanRecursive);

      if ((messages != 0) && (messages->Length() > 0))
      {
	fprintf (stderr,
	  "The following issue(s) came up while scanning '%s':",
	  (*pluginPaths)[i].path);

	for (j = 0; j < messages->Length(); j++)
	{
	  fprintf(stderr,
	    " %s\n", messages->Get (j));
	}
      }
      for (j = 0; j < metadata.Length(); j++)
      {
	PrivateSCF->RegisterClasses (plugins->Get (j), metadata[j], 
	  context ? context : (*pluginPaths)[i].path);
      }
    }
  #endif
  }
}

void scfInitialize (csPluginPaths* pluginPaths)
{
  if (!PrivateSCF)
    PrivateSCF = new csSCF ();

  scfScanPlugins (pluginPaths, 0);
}

void scfInitialize (int argc, const char* const argv[])
{
  csPluginPaths* pluginPaths = csGetPluginPaths (argv[0]);

  scfInitialize (pluginPaths);

  delete pluginPaths;
}


csSCF::csSCF ()
{
  SCF = PrivateSCF = this;
#ifdef CS_DEBUG
  object_reg = 0;
#endif

  if (!ClassRegistry)
    ClassRegistry = new scfClassRegistry ();

#ifndef CS_STATIC_LINKED
  if (!LibraryRegistry)
    LibraryRegistry = new scfLibraryVector ();
#endif

  // We need a recursive mutex.
  mutex = csMutex::Create (true);
}

csSCF::~csSCF ()
{
  delete ClassRegistry;
  ClassRegistry = 0;
#ifndef CS_STATIC_LINKED
  UnloadUnusedModules ();
  delete LibraryRegistry;
  LibraryRegistry = 0;
#endif

  SCF = PrivateSCF = 0;
}

void csSCF::RegisterClasses (char const* xml)
{
  csMemFile file(xml, strlen(xml));
  csTinyDocumentSystem docsys;
  csRef<iDocument> doc = docsys.CreateDocument();
  if (doc->Parse(&file) == 0)
    RegisterClasses(doc);
}

void csSCF::RegisterClasses (iDocument* doc)
{
  RegisterClasses (0, doc);
}

void csSCF::RegisterClasses (char const* pluginPath, 
    iDocument* doc, const char* context)
{
  if (doc)
  {
    csRef<iDocumentNode> rootnode = doc->GetRoot();
    if (rootnode != 0)
    {
      csRef<iDocumentNode> pluginnode = rootnode->GetNode("plugin");
      if (pluginnode)
      {
	csRef<iDocumentNode> scfnode = pluginnode->GetNode("scf");
	if (scfnode.IsValid())
	  RegisterClassesInt (pluginPath, scfnode, context);
	else
	  fprintf(stderr, "csSCF::RegisterClasses: Missing <scf> node.\n");
      }
      else
        fprintf(stderr,
	  "csSCF::RegisterClasses: Missing root <plugin> node.\n");
    }
  }
}

static char const* get_node_value(csRef<iDocumentNode> parent, char const* key)
{
  csRef<iDocumentNode> node = parent->GetNode(key);
  return node.IsValid() ? node->GetContentsValue() : "";
}

void csSCF::RegisterClassesInt (char const* pluginPath, iDocumentNode* scfnode, 
				const char* context)
{
  csRef<iDocumentNode> classesnode = scfnode->GetNode("classes");
  if (classesnode)
  {
    csRef<iDocumentNodeIterator> classiter = classesnode->GetNodes("class");
    csRef<iDocumentNode> classnode;
    while ((classnode = classiter->Next()))
    {
      csString classname = get_node_value(classnode, "name");
      csString imp = get_node_value(classnode, "implementation");
      csString desc = get_node_value(classnode, "description");

      // For backward compatibility, we build a comma-delimited dependency
      // string from the individual dependency nodes.  In the future,
      // iSCF::GetClassDependencies() should be updated to return an
      // iStrVector, rather than a simple comma-delimited string.
      csString depend;
      csRef<iDocumentNode> depnode = classnode->GetNode("requires");
      if (depnode)
      {
	csRef<iDocumentNodeIterator> depiter = depnode->GetNodes("class");
	csRef<iDocumentNode> depclassnode;
	while ((depclassnode = depiter->Next()))
	{
	  if (!depend.IsEmpty()) depend << ", ";
	  depend << depclassnode->GetContentsValue();
	}
      }

      char const* pdepend = (depend.IsEmpty() ? 0 : depend.GetData());
      RegisterClass(classname, pluginPath, imp, desc, pdepend, context);
    }
  }
}

void csSCF::Finish ()
{
  delete this;
}

void *csSCF::CreateInstance (const char *iClassID, const char *iInterface,
  int iVersion)
{
  csScopedMutexLock lock (mutex);

  // Pre-sort class registry for doing binary searches
  if (SortClassRegistry)
  {
    ClassRegistry->QuickSort ();
    SortClassRegistry = false;
  }

  int idx = ClassRegistry->FindSortedKey (iClassID);
  void *instance = 0;

  if (idx >= 0)
  {
    iFactory *cf = (iFactory *)ClassRegistry->Get (idx);
    iBase *object = (iBase *)cf->CreateInstance ();
    if (object)
    {
      instance = object->QueryInterface (GetInterfaceID (iInterface), iVersion);
      object->DecRef ();

      if (!instance)
        fprintf (stderr,
	  "SCF_WARNING: factory returned a null instance for %s\n"
	  "\tif error messages are not self explanatory, recompile CS with "
	  "CS_DEBUG\n", iClassID);
    }
  } /* endif */

  UnloadUnusedModules ();

  return instance;
}

void csSCF::UnloadUnusedModules ()
{
#ifndef CS_STATIC_LINKED
  csScopedMutexLock lock (mutex);
  for (int i = LibraryRegistry->Length () - 1; i >= 0; i--)
  {
    scfSharedLibrary *sl = (scfSharedLibrary *)LibraryRegistry->Get (i);
    sl->TryUnload ();
  }
#endif
}

inline static bool SameContext (csStringID contextA, csStringID contextB)
{
  return ((contextA != csInvalidStringID) && 
    (contextB != csInvalidStringID) && (contextA == contextB));
}

bool csSCF::RegisterClass (const char *iClassID, const char *iLibraryName,
  const char *iFactoryClass, const char *iDesc, const char *Dependencies, 
  const char* context)
{
  csScopedMutexLock lock (mutex);
  int idx;
  csStringID contextID = 
    context ? contexts.Request (context) : csInvalidStringID;
  if ((idx = ClassRegistry->FindKey (iClassID)) >= 0)
  {
    scfFactory *cf = (scfFactory *)ClassRegistry->Get (idx);
    if (SameContext (cf->classContext, contextID))
    {
      fprintf (stderr,
	"SCF_WARNING: class %s has already been registered in the same context\n", 
	iClassID);
    }
    else
    {
      /*
        The user may want to override a standard CS plugin by putting
	a plugin exhibiting the same class ID into the e.g. app directory.
	In this case a warning is probably not desired. But for debugging
	purposes we emit something.
       */
    #ifdef CS_DEBUG
      // @@@ some way to have this warning in non-debug builds would be nice.
      fprintf (stderr,
	"SCF_NOTIFY: class %s has already been registered in a different context "
	"(this message appears only in debug builds)\n", 
	iClassID);
    #endif
    }
    return false;
  }
  scfFactory* factory = new scfFactory (iClassID, iLibraryName, iFactoryClass,
    0, iDesc, Dependencies, contextID);
  ClassRegistry->Push (factory);
  SortClassRegistry = true;
  return true;
}

bool csSCF::RegisterClass (scfFactoryFunc Func, const char *iClassID,
  const char *Desc, const char *Dependencies, const char* context)
{
  csScopedMutexLock lock (mutex);
  int idx;
  csStringID contextID = 
    context ? contexts.Request (context) : csInvalidStringID;
  if ((idx = ClassRegistry->FindKey (iClassID)) >= 0)
  {
    scfFactory *cf = (scfFactory *)ClassRegistry->Get (idx);
    if (SameContext (cf->classContext, contextID))
    {
      fprintf (stderr,
	"SCF_WARNING: class %s has already been registered in the same context\n", 
	iClassID);
    }
    else
    {
      // See notice in other RegisterClass()
    #ifdef CS_DEBUG
      // @@@ some way to have this warning in non-debug builds would be nice.
      fprintf (stderr,
	"SCF_NOTIFY: class %s has already been registered in a different context "
	"(this message appears only in debug builds)\n", 
	iClassID);
    #endif
    }
    return false;
  }
  scfFactory* factory =
    new scfFactory (iClassID, 0, 0, Func, Desc, Dependencies, contextID);
  ClassRegistry->Push (factory);
  SortClassRegistry = true;
  return true;
}

bool csSCF::RegisterFactoryFunc (scfFactoryFunc Func, const char *FactClass)
{
  bool ok = false;
  csScopedMutexLock lock (mutex);
  for (int i = 0, n = ClassRegistry->Length(); i < n; i++)
  {
    scfFactory* fact = (scfFactory*)ClassRegistry->Get(i);
    if (fact->FactoryClass != 0 && strcmp(fact->FactoryClass, FactClass) == 0)
    {
      if (fact->CreateFunc == 0)
      {
        fact->CreateFunc = Func;
	ok = true;
      }
      //break;
      /*
        Don't break!
	Some factory functions are used for more than 1 class,
	so EVERY factory class has to be checked...
	Horribly inefficient!
       */
    }
  }
  return ok;
}

bool csSCF::UnregisterClass (const char *iClassID)
{
  csScopedMutexLock lock (mutex);

  // If we have no class registry, we aren't initialized (or were finalized)
  if (!ClassRegistry)
    return false;

  int idx = ClassRegistry->FindKey (iClassID);

  if (idx < 0)
    return false;

  ClassRegistry->Delete (idx);
  SortClassRegistry = true;
  return true;
}

void csSCF::ScanPluginsPath (const char* path, bool recursive,
  const char* context)
{
  csPluginPaths paths;
  paths.AddOnce (path, recursive);
  scfScanPlugins (&paths, context);
}

const char *csSCF::GetClassDescription (const char *iClassID)
{
  csScopedMutexLock lock (mutex);

  int idx = ClassRegistry->FindKey (iClassID);
  if (idx >= 0)
  {
    iFactory *cf = (iFactory *)ClassRegistry->Get (idx);
    return cf->QueryDescription ();
  }

  return 0;
}

const char *csSCF::GetClassDependencies (const char *iClassID)
{
  csScopedMutexLock lock (mutex);

  int idx = ClassRegistry->FindKey (iClassID);
  if (idx >= 0)
  {
    iFactory *cf = (iFactory *)ClassRegistry->Get (idx);
    return cf->QueryDependencies ();
  }

  return 0;
}

bool csSCF::ClassRegistered (const char *iClassID)
{
  csScopedMutexLock lock (mutex);
  return (ClassRegistry->FindKey (iClassID) >= 0);
}

scfInterfaceID csSCF::GetInterfaceID (const char *iInterface)
{
  csScopedMutexLock lock (mutex);
  return (scfInterfaceID)InterfaceRegistry.Request (iInterface);
}

iStrVector* csSCF::QueryClassList (char const* pattern)
{
  scfStrVector* v = new scfStrVector();

  csScopedMutexLock lock (mutex);
  int const rlen = ClassRegistry->Length();
  if (rlen != 0)
  {
    int const plen = (pattern ? strlen(pattern) : 0);
    for (int i = 0; i < rlen; i++)
    {
      char const* s = ((iFactory*)ClassRegistry->Get(i))->QueryClassID();
      if (plen == 0 || strncasecmp(pattern, s, plen) == 0)
        v->Push(csStrNew(s));
    }
  }
  csRef<iStrVector> iv (SCF_QUERY_INTERFACE(v, iStrVector));
  return iv;	// Will do DecRef() but that's ok in this case.
}
