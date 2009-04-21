/*
    Copyright (C) 1999 by Brandon Ehle <azverkan@yahoo.com>
    Copyright (C) 2007 by Pablo Martin <caedes@grupoikusnet.com>

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

#ifdef _MSC_VER
#include <io.h>
#include <stdarg.h>
#if defined(_DEBUG) && !defined(DEBUG_PYTHON)
#undef _DEBUG
#define RESTORE__DEBUG
#endif
#endif
#include <Python.h>
#ifdef RESTORE__DEBUG
#define _DEBUG
#undef RESTORE__DEBUG
#endif

#include <stdio.h>

#include "cssysdef.h"
#include "csutil/cfgmgr.h"
#include "csutil/csstring.h"
#include "csutil/util.h"
#include "csutil/scfstr.h"
#include "csutil/setenv.h"
#include "csutil/stringarray.h"
#include "csutil/sysfunc.h"
#include "csutil/syspath.h"
#include "csutil/stringarray.h"
#include "iutil/cmdline.h"
#include "iutil/eventq.h"
#include "csutil/event.h"
#include "csutil/eventhandlers.h"
#include "iutil/objreg.h"
#include "iutil/systemopenmanager.h"
#include "iutil/vfs.h"
#include "ivaria/reporter.h"

#include "cspython.h"
#include "pytocs.h"

extern "C"
{
#if defined(CS_COMPILER_MSVC)
  // MSVC will always use the shipped copy.
  #include "swigpyruntime.h"
#else
  /* *Must* be pointy include. The right file (generated when swig is present,
     shipped copy otherwise) is determined by include paths specified via
     the compiler command line. */
  #include <swigpyruntime.h>
#endif
}

CS_IMPLEMENT_PLUGIN

CS_PLUGIN_NAMESPACE_BEGIN(cspython)
{

SCF_IMPLEMENT_FACTORY(csPython)

csPython* csPython::shared_instance = 0;

/////////////////////////////////////////////////////////////////
// csPython Functions

csPython::csPython(iBase *iParent) : scfImplementationType (this, iParent),
  object_reg(0), Mode(CS_REPORTER_SEVERITY_NOTIFY), use_debugger(false)
{
  shared_instance = this;
}

csPython::~csPython()
{
  csRef<iEventQueue> queue = csQueryRegistry<iEventQueue> (object_reg);
  if (queue.IsValid())
    queue->RemoveListener (this);
  csRef<iSystemOpenManager> sysOpen =
    csQueryRegistry<iSystemOpenManager> (object_reg);
  if (sysOpen.IsValid())
    sysOpen->RemoveWeakListener (weakeh_open);
  Mode = CS_REPORTER_SEVERITY_BUG;
  Py_Finalize();
  object_reg = 0;
}

PyObject* csWrapTypedObject(void* objectptr, const char *typetag,
  int own)
{
  swig_type_info *ti = SWIG_TypeQuery (typetag);
  PyObject *obj = SWIG_NewPointerObj (objectptr, ti, own);
  return obj;
}

static csString const& path_append(csString& path, char const* component)
{
  char c;
  size_t n = path.Length();
  if (n > 0 && (c = path[n - 1]) != '/' && c != '\\')
    path << '/';
  path << component;
  return path;
}

bool csPython::Initialize(iObjectRegistry* object_reg)
{
  csPython::object_reg = object_reg;
  reporter = csQueryRegistry<iReporter> (object_reg);

  // get command line options
  csRef<iCommandLineParser> cmdline =
    csQueryRegistry<iCommandLineParser> (object_reg);
  bool const use_reporter = cmdline->GetOption("python-enable-reporter") != 0;
  use_debugger = cmdline->GetOption("python-enable-debugger") != 0;
 
  // Add compile location to pythonpath 
#ifdef DEFAULT_PYTHMOD_PATH
  csString pythonpath (getenv ("PYTHONPATH"));
  pythonpath.Insert (0, CS_PATH_DELIMITER);
  pythonpath.Insert (0, DEFAULT_PYTHMOD_PATH);
  CS::Utility::setenv ("PYTHONPATH", pythonpath, true);
#endif

  Py_SetProgramName(const_cast<char*>("Crystal Space -- Python"));
  Py_Initialize();

  py_main = PyImport_AddModule("__main__");
  Py_INCREF(py_main);

  InitPytocs();

  // some parts of python api require sys.argv to be filled.
  // so strange errors will appear if we dont do the following
  char *(argv[2]) = {const_cast<char*>(""), NULL};
  PySys_SetArgv(1, argv);

  // add cs scripts paths to pythonpath
  if (!AugmentPythonPath()) return false;

  if (use_reporter && !LoadModule ("cshelper")) return false;
  if (use_debugger && !LoadModule ("pdb")) return false;
  if (!LoadModule ("cspace")) return false;

  Mode = CS_REPORTER_SEVERITY_NOTIFY;

  // Inject iSCF::SCF
  csRefArray<iScriptValue> args;
  args.Push(csRef<iScriptValue>(RValue(iSCF::SCF, "iSCF *")));
  csRef<iScriptValue> ret = Call("cspace.SetSCFPointer",args);

  // Inject Object Registry
  Store("cspace.object_reg",RValue(object_reg, "iObjectRegistry *"));
  
  // uncomment this if you start refactoring :))
  // Test();

  // Register for command line help event
  csRef<iEventQueue> queue = csQueryRegistry<iEventQueue> (object_reg);
  if (queue.IsValid())
    queue->RegisterListener(this, csevCommandLineHelp(object_reg));
  // load further python modules from config file keys.
  LoadConfig();
  
  csRef<iSystemOpenManager> sysOpen =
    csQueryRegistry<iSystemOpenManager> (object_reg);
  if (sysOpen.IsValid())
    sysOpen->RegisterWeak (this, weakeh_open);
  return true;
}

void csPython::LoadConfig()
{
  csRef<iConfigManager> config;
  config = csQueryRegistry<iConfigManager> (object_reg);

  // Parse Module Paths in config
  csRef<iConfigIterator>  it = config->Enumerate("CsPython.Path");
  while (it->Next())
  {
    if (!AddVfsPythonPath(it->GetStr()))
    {
      Print(true,it->GetStr()+csString(" could not be added to pythonpath."));
    }
  }
}

void csPython::LoadComponents()
{
  csRef<iConfigManager> config;
  config = csQueryRegistry<iConfigManager> (object_reg);

  // Parse Modules in config
  csRef<iConfigIterator> it = config->Enumerate("CsPython.Module");
  while (it->Next())
  {
    if (!LoadModule(it->GetStr()))
    {
      Print(true,it->GetStr()+csString(" could not be imported."));
    }
  }
}

bool csPython::AddRealPythonPath(csString &path,const char *subpath)
{
  csString cmd;
  if (subpath)
    cmd << "sys.path.append('" << path_append(path, subpath) << "')\n";
  else
    cmd << "sys.path.append('" << path << "')\n";
  if (cmd.IsEmpty() || !RunText (cmd)) return false;
  return true;
}

bool csPython::AddVfsPythonPath(const char *vfspath,const char *subpath)
{
  csString cmd;
  csRef<iVFS> vfs(csQueryRegistry<iVFS> (object_reg));
  if (vfs.IsValid())
  {
    if (!vfs->Exists(vfspath))
      return false;
    csRef<iStringArray> paths(vfs->GetRealMountPaths(vfspath));
    if (!paths->GetSize())
    {
      csRef<iDataBuffer> databuf = vfs->GetRealPath(vfspath);
      csString path = databuf->GetData();
      return AddRealPythonPath(path, subpath);
    }
    for (size_t i = 0, n = paths->GetSize(); i < n; i++)
    {
      csString path = paths->Get(i);
      return AddRealPythonPath(path, subpath);
    }
  }
  return true;
}

bool csPython::AugmentPythonPath()
{
  if (!LoadModule ("sys")) return false;

  // Add /scripts vfs path to pythonpath
  AddVfsPythonPath("/scripts","python");

  // Add cs python scripts folder to pythonpath
  csString cfg(csGetConfigPath());
  if (!AddRealPythonPath(cfg,"scripts/python"))
  {
    Print(true,"cs scripts folder could not be added to pythonpath.");
    return false;
  }
  return true;
}

bool csPython::HandleEvent(iEvent& e)
{
  bool handled = false;
  if (e.Name == csevCommandLineHelp(object_reg))
  {
#undef indent
#define indent "                     "
    csPrintf("Options for csPython plugin:\n"
	   "  -python-enable-reporter\n"
	   indent "Redirect sys.stdout and sys.stderr to iReporter\n"
	   "  -python-enable-debugger\n"
	   indent "When Python exception is thrown, launch Python debugger\n");
#undef indent
    handled = true;
  }
  else if (e.Name == csevSystemOpen (object_reg))
  {
    LoadComponents();
  }
  return handled;
}

void csPython::ShowError()
{
  if(PyErr_Occurred())
  {
    PyObject *ptype;
    PyObject *pvalue;
    PyObject *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_Display(ptype,pvalue,ptraceback);
    Print(true, PyString_AsString(pvalue));
  }
}

bool csPython::RunText(const char* Text)
{
  // Apparently, some Python installations have bad prototype, so const_cast<>.
  bool ok = !PyRun_SimpleString(const_cast<char*> (Text));
  if(!ok && use_debugger)
    PyRun_SimpleString(const_cast<char*> ("pdb.pm()"));
  ShowError();
  return ok;
}

bool csPython::LoadModule(const char* name)
{
  csString s;
  s << "import " << name;
  return RunText(s);
}

bool csPython::LoadModule (const char *path, const char *name)
{
  csRef<iVFS> vfs(csQueryRegistry<iVFS> (object_reg));
  if (!vfs.IsValid())
    return false;
  csRef<iDataBuffer> rpath = vfs->GetRealPath (path);

  return LoadModuleNative (rpath->GetData (), name);
}

bool csPython::LoadModuleNative (const char *path, const char *name)
{
  // Alternative from `embedding' in py c api docs:
  //   Must provide custom implementation of
  //  Py_GetPath(), Py_GetPrefix(), Py_GetExecPrefix(),
  //  and Py_GetProgramFullPath()
  csString import;
  import << "import sys\n"
         << "paths = sys.path\n"
         << "sys.path = ['" << path << "']\n"
         << "import " << name << "\n"
         << "sys.path = paths\n";
  return RunText(import);
}

void csPython::Print(bool Error, const char *msg)
{
  csReport (object_reg, Error ? CS_REPORTER_SEVERITY_ERROR : Mode,
    "crystalspace.script.python", "%s", msg);
}

csPython::Object* csPython::Query (iScriptObject *obj) const
{
  csRef<csPythonObject> priv = scfQueryInterface<csPythonObject> (obj);
  if (priv.IsValid())
    return static_cast<csPython::Object*> ((csPythonObject*)priv);
  reporter->Report (CS_REPORTER_SEVERITY_ERROR,
   "crystalspace.script.python","This iScriptObject is not from Python");
  return 0;
}

csPython::Value* csPython::Query (iScriptValue *val) const
{
  csRef<csPythonValue> priv = scfQueryInterface<csPythonValue> (val);
  if (priv.IsValid())
    return static_cast<csPython::Value*> ((csPythonValue*)priv);
  reporter->Report (CS_REPORTER_SEVERITY_ERROR,
   "crystalspace.script.python","This iScriptValue is not from Python");
  return 0;
}

void csPython::Print(PyObject *py_obj)
{
  PyObject_Print(py_obj,stdout,Py_PRINT_RAW);
  printf(" ref: %ld", (long)py_obj->ob_refcnt);
  printf("\n");
}

void csPython::Print(csRef<iScriptValue> value)
{
  Print(Query(value)->self);
}

void csPython::Print(csRef<iScriptObject> obj)
{
  Print(Query(obj)->self);
}

PyObject *csPython::FindObject(const char *url,bool do_errors)
{
  csStringArray tokens;
  tokens.SplitString(url,".");
  csStringArray::Iterator iter(tokens.GetIterator());
  PyObject *py_curr = py_main;
  while (iter.HasNext())
  {
    PyObject *py_next = PyObject_GetAttrString(py_curr,const_cast<char*>(iter.Next()));
    if (!py_next)
    {
      if (do_errors)
        ShowError();
      else
        PyErr_Clear();
      return NULL;
    }
    Py_DECREF(py_next);
    py_curr = py_next;
    
  }
  return py_curr;
}

PyObject *csPython::FindFunction(const char *url,bool do_errors)
{
  csStringArray tokens;
  tokens.SplitString(url,".");
  csStringArray::Iterator iter(tokens.GetIterator());
  PyObject *py_curr = py_main;
  Py_INCREF(py_main);
  while (iter.HasNext())
  {
    PyObject *py_next = PyObject_GetAttrString(py_curr,
		    		const_cast<char*>(iter.Next()));
    if (!py_next)
    {
      if (do_errors)
        ShowError();
      else
        PyErr_Clear();
      return NULL;
    }
    //Print(py_next);
    Py_DECREF(py_curr);
    py_curr = py_next;
    
  }
  return py_curr;
}

bool csPython::Store (const char *name, iScriptValue *value)
{
  int result;
  const char *last_dot = strrchr(name,'.');
  if (last_dot)
  {
    // variable is inside some hierachy
    const csString url(name,last_dot-name);
    PyObject *py_curr = FindObject(url);
    if (!py_curr) return false;
    result = PyObject_SetAttrString(py_curr,const_cast<char*>(last_dot+1),Query(value)->self);
  }
  else
  {
    result = PyObject_SetAttrString(py_main,const_cast<char*>(name),Query(value)->self);
  }
  
  if (result == -1) 
  { 
    ShowError();
    return false; 
  }
  return true;
}

csPtr<iScriptValue> csPython::Retrieve (const char *name) 
{
  PyObject *py_curr = FindObject(name);
  if (!py_curr)
  {
    ShowError();
    return false;
  }
  return new Value (this, py_curr, true);
}

bool csPython::Remove (const char *name) 
{
  int result;
  const char *last_dot = strrchr(name,'.');
  if (last_dot)
  {
    // variable is inside some hierachy
    const csString url(name,last_dot-name);
    PyObject *py_curr = FindObject(url);
    if (!py_curr) return false;
    result = PyObject_DelAttrString(py_curr,const_cast<char*>(last_dot+1));
  }
  else
  {
    result = PyObject_DelAttrString(py_main,const_cast<char*>(name));
  }

  if (result == -1) 
  {
    ShowError(); 
    return false;
  }
  return true;
}

csPtr<iScriptValue> csPython::Call (const char *name,
	const csRefArray<iScriptValue> &args) 
{
  PyObject *py_func = FindFunction(name);
  return CallBody(py_func,args,true);
}

csPtr<iScriptObject> csPython::New (const char *type,
	const csRefArray<iScriptValue> &args)
{
  PyObject *py_type = FindObject(type,false);
  csRef<iScriptValue> ret (CallBody(py_type,args));
  if (!ret.IsValid())
  {
    csString cstype;
    cstype << "cspace." << type;
    py_type = FindObject(cstype);
    ret.AttachNew (CallBody(py_type,args));
  }
  if (ret.IsValid()) return new Object (this, Query(ret)->self,true);
  return 0;
}

csPtr<iScriptValue> csPython::CallBody (PyObject *py_func,
    const csRefArray<iScriptValue> &args, bool bound_func)
{
  if (!PyCallable_Check(py_func))
    return 0;
  // initialize arguments
  PyObject *py_args = NULL;
  if (args.GetSize())
    py_args = PyTuple_New(args.GetSize());
  Py_ssize_t i=0;
  csRefArray<iScriptValue>::ConstIterator iter(args.GetIterator());
  while (iter.HasNext())
  {
    PyObject *next = Query(iter.Next())->self;
    Py_INCREF(next);
    PyTuple_SetItem(py_args,i,next);
    i++;
  }
  // call function
  PyObject *py_result = PyObject_CallObject(py_func,py_args);
  if (bound_func)
    Py_DECREF(py_func);
  if (py_args)
    Py_DECREF(py_args);
  // check result and return
  if (py_result)
    return csPtr<iScriptValue>(new Value (this, py_result, false));
  else
  {
    ShowError();
    return 0;
  }
}

/////////////////////////////////////////////////////////////////
// Object Functions

const csRef<iString> csPython::Object::GetClass () const
{
  PyObject *type = PyObject_Type(self);
  PyObject *name = PyObject_GetAttrString(type,"__name__");
  csPtr<iString> c = new scfString(PyString_AsString(name));
  Py_DECREF(type);
  Py_DECREF(name);
  return c;
}
bool csPython::Object::IsA (const char *type) const
{
  PyObject *py_type = parent->FindObject(type);
  if (py_type && PyObject_IsInstance(self,py_type))
      return true;
  csString cstype;
  cstype << "cspace." << type;
  py_type = parent->FindObject(cstype);
  if (type && PyObject_IsInstance(self,py_type))
    return true;
  return false;
}
void* csPython::Object::GetPointer ()
{
  PySwigObject *sobj = SWIG_Python_GetSwigThis(self);
  if (sobj)
    return sobj->ptr;
  else
    return 0;
}

csPtr<iScriptValue> csPython::Object::Call (const char *name, const csRefArray<iScriptValue> &args)
{
  PyObject *py_func = PyObject_GetAttrString(self,const_cast<char*>(name));
  if (!py_func)
  {
    parent->ShowError();
    return 0;
  }
  return parent->CallBody(py_func,args,true);
}
bool csPython::Object::Set (const char *name, iScriptValue *value)
{
  if (PyObject_SetAttrString(self,const_cast<char*>(name),
			  	parent->Query(value)->self) == -1)
  {
    parent->ShowError();
    return false;
  }
  else
    return true;
}
csPtr<iScriptValue> csPython::Object::Get (const char *name)
{
  PyObject *res = PyObject_GetAttrString(self,const_cast<char*>(name));
  if (res)
    return new Value(parent,res,false);
  else
  {
    parent->ShowError();
    return 0;
  }
}

/////////////////////////////////////////////////////////////////
// Value Functions

unsigned csPython::Value::GetTypes () const
{
  //PyObjects can always be treated as an iScriptObject, but there's not much point if it's primarily
  //an int, string, etc. Unsure if this is the best method of handling this.
  unsigned result = tObject;
  if (self == Py_None)
    return 0;
  if (PyInt_Check(self)) {result += tInt;}
  if (PyFloat_Check(self)) {result += (tFloat + tDouble);}
  if (PyString_Check(self)) {result += tString;}
  if (PyBool_Check(self)) {result += tBool;}
  return result;

}
int csPython::Value::GetInt () const 
{
  return (int)PyInt_AsLong(self);
}
double csPython::Value::GetDouble () const 
{
  return PyFloat_AsDouble(self);
}
float csPython::Value::GetFloat () const 
{
  return (float)PyFloat_AsDouble(self);
}
bool csPython::Value::GetBool () const 
{
  return PyObject_IsTrue(self);
}
const csRef<iString> csPython::Value::GetString () const 
{
  const char* strval = PyString_AsString(self);
  if (strval)
    return csPtr<iString>(new scfString(PyString_AsString(self)));
  else
  {
    parent->ShowError();
    return 0;
  }
}
csRef<iScriptObject> csPython::Value::GetObject () const 
{
  if (self == Py_None)
    return 0;
  else
    return csPtr<iScriptObject>(new csPython::Object(parent,self,true));
}

/////////////////////////////////////////////////////////////////
// Test Function

void csPython::Test()
{
  printf("-- INIT TESTS --\n\n");
  printf("- store cspace.foo\n");
  if (Store("cspace.foo",csRef<iScriptValue>(RValue(10))))
    printf("- OK\n");
  else
    printf("- FAIL\n");

  printf("- store cspace.bar\n");
  if (Store("cspace.bar",csRef<iScriptValue>(RValue(10))))
    printf("- OK\n");
  else
    printf("- FAIL\n");


  printf("- retrieve cspace.foo\n");
  csRef<iScriptValue> val = Retrieve("cspace.foo");
  if (val)
  {
    iScriptValue *val2 = val;
    printf("- OK: ");
    PyObject_Print(Query(val2)->self,stdout,Py_PRINT_RAW);
    printf("\n");
  }
  else
    printf("- FAIL\n");


  printf("- remove cspace.foo\n");
  if (Remove("cspace.foo"))
    printf("- OK\n");
  else
    printf("- FAIL\n");

  printf("- store cspace.foo2\n");
  if (Store("cspace.foo2",csRef<iScriptValue>(RValue(10))))
    printf("- OK\n");
  else
    printf("- FAIL\n");

  printf("- store i\n");
  if (Store("i",csRef<iScriptValue>(RValue(10))))
    printf("- OK\n");
  else
    printf("- FAIL\n");


  printf("- new csVector3\n"); 
  csRef<iScriptObject> test1 = New("csVector3");
  if (test1)
  {
    Print(test1);
    printf("- OK\n");
  }
  else
    printf("- FAIL\n");

  csRefArray<iScriptValue> args;
  /*printf("- store cspace.myvec\n");
  if (Store("cspace.myvec",test1))
    printf("- OK\n");
  else
    printf("- FAIL\n");

  printf("- call cspace.myvec.Norm\n");
  */
  printf("- call cspace.corecvar.iSCF_SCF.UnloadUnusedModules\n");
  csRef<iScriptValue> test7 = Call("cspace.corecvar.iSCF_SCF.UnloadUnusedModules",args);
  if (test7)
  {
    Print(test7);
    printf("OK\n");
  }
  else
    printf("- FAIL\n");

  printf("- call cspace.GetDefaultAppID\n");
  csRef<iScriptValue> test5 = Call("cspace.csInitializer.GetDefaultAppID",args);
  if (test5)
  {
    Print(test5);
    printf("OK\n");
  }
  else
    printf("- FAIL\n");

  printf("- new cspace.csVector3\n");
  csRef<iScriptObject> test2 = New("cspace.csVector3",args);
  if (test2)
  {
    Print(test2);
    printf("OK\n");
  }
  else
    printf("- FAIL\n");

  printf("- call csVector3.Norm\n");
  csRef<iScriptValue> res1 = test1->Call("Norm");
  if (res1)
  {
    Print(res1);
    printf("- OK\n");
  }
  else
    printf("- FAIL\n");



  printf("- set csVector3.x\n");
  if (test1->Set("x",csRef<iScriptValue>(RValue(10))))
  {
    printf("- OK\n");
  }
  else
    printf("- FAIL\n");


  printf("- get csVector3.xx\n");
  csRef<iScriptValue> res3 = test1->Get("xx");
  if (res3)
  {
    Print(res3);
    printf("- OK\n");
  }
  else
    printf("- FAIL\n");


  printf("-- INIT BUG TESTS --\n\n");
  printf("- store cspacex.foo2\n");
  if (Store("cspacex.foo2",csRef<iScriptValue>(RValue(10))))
    printf("- FAIL:FAIL\n");
  else
    printf("- FAIL:OK\n");


  printf("- new cspacex.csVector3\n");
  csRef<iScriptObject> test3 = New("cspacex.csVector3",args);
  if (test3)
  {
    Print(test3);
    printf("FAIL:FAIL\n");
  }
  else
    printf("- FAIL:OK\n");

  printf("- xcsVector3\n");
  csRef<iScriptObject> test4 = New("xcsVector3",args);
  if (test4)
  {
    Print(test4);
    printf("FAIL:FAIL\n");
  }
  else
    printf("- FAIL:OK\n");



  printf("- call csVector3.Normx\n");
  csRef<iScriptValue> res2 = test1->Call("Normx");
  if (res2)
  {
    Print(res2);
    printf("- FAIL:FAIL\n");
  }
  else
    


  printf("remove cspace.barxxx\n");
  if (Remove("cspace.barxxx"))
    printf("- FAIL:FAIL\n");
  else
    printf("- FAIL:OK\n");
  printf("-- END TESTS --\n\n");


/*
  printf("retrieve cspace.foo\n");
  val = Retrieve("cspace.foo");
  if (val)
  {
    iScriptValue *val2 = val;
    printf("retrieved: ");
    PyObject_Print(Query(val2)->self,stdout,Py_PRINT_RAW);
    printf("\n");
  }
  else
    printf("failed retrieve\n");
*/
}


}
CS_PLUGIN_NAMESPACE_END(cspython)