/*
    Copyright (C) 2000 by Jorrit Tyberghein
  
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

#ifndef POLYTREE_H
#define POLYTREE_H

#include "csgeom/math3d.h"
#include "csgeom/plane3.h"
#include "csgeom/vector3.h"
#include "csgeom/box.h"
#include "csengine/arrays.h"

class csSector;
class csPolygonInt;
class csPolygonTree;
class csPolygonTreeNode;
class csPolygonStub;
class csPolyTreeObject;
class Dumper;
struct iFile;


#define NODE_OCTREE 1
#define NODE_BSPTREE 2

/**
 * Visit a node in a polygon tree. If this function returns non-NULL
 * the scanning will stop and the pointer will be returned.
 */
typedef void* (csTreeVisitFunc)(csSector*, csPolygonInt**,
	int num, void*);

/**
 * Potentially cull a node from the tree just before it would otherwise
 * have been traversed in Back2Front() or Front2Back().
 * If this function returns true then the node is potentially visible.
 */
typedef bool (csTreeCullFunc)(csPolygonTree* tree, csPolygonTreeNode* node,
	const csVector3& pos, void* data);

/**
 * A general node in a polygon tree.
 */
class csPolygonTreeNode
{
  friend class Dumper;

protected:
  /**
   * A linked list for all polygons stubs that are added
   * to this node. These stubs represents sets of polygons
   * from this object that belong to the same plane as the
   * plane of the splitter in the tree node.
   */
  csPolygonStub* first_stub;

  /**
   * A linked list of all polygons stubs that still need to
   * be processed whenever this node becomse visible.
   */
  csPolygonStub* todo_stubs;

public:
  /**
   * Constructor.
   */
  csPolygonTreeNode () : first_stub (NULL), todo_stubs (NULL) { }

  /**
   * Destructor.
   */
  virtual ~csPolygonTreeNode ();

  /// Return true if node is empty.
  virtual bool IsEmpty () = 0;

  /// Return type (NODE_???).
  virtual int Type () = 0;

  /**
   * Unlink a stub from the stub list.
   * Warning! This function does not test if the stub
   * is really on the list!
   */
  void UnlinkStub (csPolygonStub* ps);

  /**
   * Link a stub to the todo list.
   */
  void LinkStubTodo (csPolygonStub* ps);

  /**
   * Link a stub to the stub list.
   */
  void LinkStub (csPolygonStub* ps);

  /**
   * Traverse all the polygons in the dynamic objects
   * added to this node.
   */
  void* TraverseObjects (csSector* sector, const csVector3& pos,
  	csTreeVisitFunc* func, void* data);
};

/**
 * A general polygon tree. This is an abstract data type.
 * Concrete implementations like csBspTree or csOctree inherit
 * from this class.
 */
class csPolygonTree
{
  friend class Dumper;

protected:
  /// The root of the tree.
  csPolygonTreeNode* root;

  /// The parent sector that this tree is made for.
  csSector* sector;

  /// Clear the nodes.
  void Clear () { CHK (delete root); }

  // Various routines to write to an iFile. Used by 'Cache'.
  void WriteString (iFile* cf, char* str, int len);
  void WriteBox3 (iFile* cf, const csBox3& box);
  void WriteVector3 (iFile* cf, const csVector3& v);
  void WritePlane3 (iFile* cf, const csPlane3& v);
  void WriteLong (iFile* cf, long l);
  void WriteByte (iFile* cf, unsigned char b);
  void WriteBool (iFile* cf, bool b);

  // Various routines to write from an iFile. Used by 'ReadFromCache'.
  void ReadString (iFile* cf, char* str, int len);
  void ReadBox3 (iFile* cf, csBox3& box);
  void ReadVector3 (iFile* cf, csVector3& v);
  void ReadPlane3 (iFile* cf, csPlane3& v);
  long ReadLong (iFile* cf);
  unsigned char ReadByte (iFile* cf);
  bool ReadBool (iFile* cf);

public:
  /**
   * Constructor.
   */
  csPolygonTree (csSector* sect) : root (NULL), sector (sect) { }

  /**
   * Destructor.
   */
  virtual ~csPolygonTree () { }

  /// Get the sector for this tree.
  csSector* GetSector () { return sector; }

  /**
   * Create the tree for the default parent set.
   */
  virtual void Build () = 0;

  /**
   * Create the tree with a given set of polygons.
   */
  virtual void Build (csPolygonInt** polygons, int num) = 0;

  /**
   * Create the tree with a given set of polygons.
   */
  void Build (csPolygonArray& polygons)
  {
    Build (polygons.GetArray (), polygons.Length ());
  }

  /**
   * Add a dynamic object to the tree.
   */
  void AddObject (csPolyTreeObject* obj);

  /**
   * Add a polygon stub to the todo list of the tree.
   */
  void AddStubTodo (csPolygonStub* stub)
  {
    root->LinkStubTodo (stub);
  }

  /**
   * Test if any polygon in the list covers any other polygon.
   * If this function returns false we have convexity.
   */
  bool Covers (csPolygonInt** polygons, int num);

  /// Traverse the tree from back to front starting at the root and 'pos'.
  virtual void* Back2Front (const csVector3& pos, csTreeVisitFunc* func,
  	void* data, csTreeCullFunc* cullfunc = NULL, void* culldata = NULL) = 0;
  /// Traverse the tree from front to back starting at the root and 'pos'.
  virtual void* Front2Back (const csVector3& pos, csTreeVisitFunc* func,
  	void* data, csTreeCullFunc* cullfunc = NULL, void* culldata = NULL) = 0;

  /// Print statistics about this tree.
  virtual void Statistics () = 0;
};

#endif /*POLYTREE_H*/

