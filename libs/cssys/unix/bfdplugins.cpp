/*
    Load plugin meta data from object file headers.
    Copyright (C) 2003 by Jorrit Tyberghein
	      (C) 2003 by Eric Sunshine
	      (C) 2003 by Frank Richter
	      (C) 2003 by Mat Sutcliffe

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

#define CS_SYSDEF_PROVIDE_DIR
#include "cssysdef.h"
#include "cssys/csshlib.h"
#include "cssys/sysfunc.h"
#include "csutil/csstring.h"
#include "csutil/scfstringarray.h"
#include "csutil/physfile.h"
#include "csutil/scfstr.h"
#include "csutil/util.h"
#include "csutil/xmltiny.h"
#include "iutil/document.h"

#include <string.h>
#include <bfd.h>

static void AppendStrVecString (iStringArray*& strings, const char* str)
{
  if (!strings)
  {
    strings = new scfStringArray ();
  }
  strings->Push (str);
}

csRef<iString> csGetPluginMetadata (const char* fullPath, 
				    csRef<iDocument>& metadata)
{
  iString* result = 0;
  bool conflict = false;

  int len = strlen (fullPath);
  csString cspluginPath (fullPath);
  cspluginPath.Truncate (len - 3);
  cspluginPath += ".csplugin";

  if (!metadata)
  {
    // Since TinyXML documents maintain references to the document system, we
    // must allocate the document system via the heap, rather than the stack.
    csRef<iDocumentSystem> docsys = csPtr<iDocumentSystem>
      (new csTinyDocumentSystem ());
    metadata = docsys->CreateDocument ();
  }
  char const* errmsg;
  bool usebfd = false;
  bfd *abfd = bfd_openr (fullPath, 0);
  if (abfd)
  {
    if (bfd_check_format (abfd, bfd_object))
    {
      asection *sect = bfd_get_section_by_name (abfd, ".crystal");
      if (sect)
      {
        int size = bfd_section_size (abfd, sect);
        char *buf = (char *) malloc (size + 1);
        if (buf)
        {
          if (! bfd_get_section_contents (abfd, sect, buf, 0, size))
            errmsg = "libbfd can't get '.crystal' section contents";
          else
          {
            buf[size] = 0;
            errmsg = metadata->Parse (buf);
            usebfd = true;
          }
          free (buf);
        }
      }
    }
    bfd_close (abfd);
  }

  csPhysicalFile file (cspluginPath, "rb");
  if (file.GetStatus () == VFS_STATUS_OK)
  {
    if (usebfd) conflict = true;
    else errmsg = metadata->Parse (&file);
  }

  csString errstr;
  if (conflict) errstr += csString().Format ("Warning: %s has embedded data and"
    " .csplugin file, using embedded.%s", fullPath, errmsg ? "\n" : "");
  if (errmsg) errstr += csString().Format ("Error parsing metadata in %s: %s",
    usebfd ? fullPath : cspluginPath.GetData (), errmsg);
  if (errstr.Length ()) result = new scfString (errstr);

  return csPtr<iString> (result);
}
  
void InternalScanPluginDir (iStringArray*& messages,
			    const char* dir, 
			    csRef<iStringArray>& plugins,
			    bool recursive)
{
  struct dirent* de;
  DIR* dh = opendir(dir);
  if (dh != 0)
  {
    while ((de = readdir(dh)) != 0)
    {
      if (!isdir(dir, de))
      {
        int const n = strlen(de->d_name);
        if (n >= 3 && strcasecmp(de->d_name + n - 3, ".so") == 0)
        {
	    csString scffilepath;
	    scffilepath << dir << PATH_SEPARATOR << de->d_name;

	    plugins->Push (scffilepath);
        }
      }
      else
      {
	if (recursive && (strcmp (de->d_name, ".") != 0)
	  && (strcmp (de->d_name, "..") != 0))
	{
	  iStringArray* subdirMessages = 0;
	    
	  csString scffilepath;
	  scffilepath << dir << PATH_SEPARATOR << de->d_name;
	  
	  InternalScanPluginDir (subdirMessages, scffilepath,
	    plugins, recursive);
  
	  if (subdirMessages != 0)
	  {
	    for (int i = 0; i < subdirMessages->Length(); i++)
	    {
	      AppendStrVecString (messages, subdirMessages->Get (i));
	    }
	    subdirMessages->DecRef();
	  }
        }
      }
    }
    closedir(dh);
  }
}

csRef<iStringArray> csScanPluginDir (const char* dir, 
				   csRef<iStringArray>& plugins,
				   bool recursive)
{
  iStringArray* messages = 0;

  if (!plugins)
    plugins.AttachNew (new scfStringArray ());

  InternalScanPluginDir (messages, dir, plugins, recursive);
 
  return csPtr<iStringArray> (messages);
}

csRef<iStringArray> csScanPluginDirs (csPluginPaths* dirs, 
				    csRef<iStringArray>& plugins)
{
  iStringArray* messages = 0;

  if (!plugins)
    plugins.AttachNew (new scfStringArray ());

  for (int i = 0; i < dirs->GetCount (); i++)
  {
    iStringArray* dirMessages = 0;
    InternalScanPluginDir (dirMessages, (*dirs)[i].path, plugins, 
      (*dirs)[i].scanRecursive);
    
    if (dirMessages != 0)
    {
      csString tmp;
      tmp.Format ("The following error(s) occured while scanning '%s':",
	(*dirs)[i].path);

      AppendStrVecString (messages, tmp);

      for (int i = 0; i < dirMessages->Length(); i++)
      {
	tmp.Format (" %s", dirMessages->Get (i));
	AppendStrVecString (messages, tmp);
      }
      dirMessages->DecRef();
    }
  }
 
  return csPtr<iStringArray> (messages);
}
