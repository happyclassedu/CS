/*
    Crystal Space Virtual File System class
    Copyright (C) 1998,1999,2000 by Andrew Zabolotny <bit@eltech.ru>
	Copyright (C) 2006 by Brandon Hamilton <brandon.hamilton@gmail.com>

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

#ifndef __CS_VFS_H__
#define __CS_VFS_H__

#include "csutil/refarr.h"
#include "csutil/scf_implementation.h"
#include "csutil/scopedmutexlock.h"
#include "iutil/vfs.h"
#include "iutil/eventh.h"
#include "iutil/comp.h"

namespace cspluginVFS
{

/**
 * TODO: Rewrite class description.
 */
class csVFS : public scfImplementation2<csVFS, iVFS, iComponent>
{

public:

  /// Initialize VFS by reading contents of given INI file
  csVFS (iBase *iParent);

  /// Virtual File System destructor
  virtual ~csVFS ();

  /// Initialize the Virtual File System
  virtual bool Initialize (iObjectRegistry *object_reg);

  /// Set current working directory
  virtual bool ChDir (const char *Path);

  /// Get current working directory
  virtual const char *GetCwd () const { return "/";//return cwd; 
  }

  /// Push current directory
  virtual void PushDir (char const* Path = 0);

  /// Pop current directory
  virtual bool PopDir ();

  /**
   * Expand given virtual path, interpret all "." and ".."'s relative to
   * 'current virtual directory'. Return a new iString object.
   * If IsDir is true, expanded path ends in an '/', otherwise no.
   */
  virtual csPtr<iDataBuffer> ExpandPath (const char *Path, bool IsDir = false) const;

  /// Check whenever a file exists
  virtual bool Exists (const char *Path) const;

  /// Find all files in a virtual directory and return an array of their names
  virtual csPtr<iStringArray> FindFiles (const char *Path) const;

  /// Replacement for standard fopen()
  virtual csPtr<iFile> Open (const char *FileName, int Mode);
  
  /**
   * Get an entire file at once. You should delete[] returned data
   * after usage. This is more effective than opening files and reading
   * the file in blocks.  Note that the returned buffer is always null-
   * terminated (so that it can be conveniently used with string functions)
   * but the extra null-terminator is not counted as part of the returned size.
   */
  virtual csPtr<iDataBuffer> ReadFile (const char *FileName, bool nullterm);

  /// Write an entire file in one pass.
  virtual bool WriteFile (const char *FileName, const char *Data, size_t Size);

  /// Delete a file on VFS
  virtual bool DeleteFile (const char *FileName);

  /// Close all opened archives, free temporary storage etc.
  virtual bool Sync ();

  /// Mount an VFS path on a "real-world-filesystem" path
  virtual bool Mount (const char *VirtualPath, const char *RealPath);
  /// Unmount an VFS path; if RealPath is 0, entire VirtualPath is unmounted
  virtual bool Unmount (const char *VirtualPath, const char *RealPath);
  
  /// Mount the root directory or directories 
  virtual csRef<iStringArray> MountRoot (const char *VirtualPath);

  /// Save current configuration back into configuration file
  virtual bool SaveMounts (const char *FileName);
  /// Load a configuration file
  virtual bool LoadMountsFromFile (iConfigFile* file);

  /// Auto-mount ChDir.
  virtual bool ChDirAuto (const char* path, const csStringArray* paths = 0,	const char* vfspath = 0, const char* filename = 0);

  /// Query file local date/time
  virtual bool GetFileTime (const char *FileName, csFileTime &oTime) const;
  /// Set file local date/time
  virtual bool SetFileTime (const char *FileName, const csFileTime &iTime);

  /// Query file size (without opening it)
  virtual bool GetFileSize (const char *FileName, size_t &oSize);

  /**
   * Query real-world path from given VFS path.
   * This will work only for files that are stored on real filesystem,
   * not in archive files. You should expect this function to return
   * 0 in this case.
   */
  virtual csPtr<iDataBuffer> GetRealPath (const char *FileName);

  /// Get all current virtual mount paths
  virtual csRef<iStringArray> GetMounts ();

  /// Get the real paths associated with a mount
  virtual csRef<iStringArray> GetRealMountPaths (const char *VirtualPath);

  /// Register a filesystem plugin
  virtual bool RegisterPlugin(csRef<iFileSystem> FileSystem);

private:

  /// Mutex to make VFS thread-safe.
  csRef<csMutex> mutex;

  /// Currently registered FileSystem plugins
  csRefArray<iFileSystem> fsPlugins;

  // Reference to the object registry.
  iObjectRegistry *object_reg;
};

} // namespace cspluginVFS

#endif  // __CS_VFS_H__

