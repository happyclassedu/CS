/*
    Copyright (C) 1998 by Jorrit Tyberghein
  
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

#ifndef BSP_H
#define BSP_H

#include "csgeom/math3d.h"
#include "csengine/polytree.h"
#include "csengine/polyint.h"
#include "csengine/arrays.h"

class csBspTree;
class csPolygonArrayNoFree;
class Dumper;

// The BSP tree can be build using the following criteria:
#define BSP_MINIMIZE_SPLITS 1		// Minimize the number of polygon splits
#define BSP_MOST_ON_SPLITTER 2		// Splitter with most coplanar polygons
#define BSP_RANDOM 3			// Choose a random splitter
#define BSP_ALMOST_MINIMIZE_SPLITS 4	// Select a number of polygons and choose minimal split
#define BSP_BALANCED 5			// Try to create a balanced tree
#define BSP_ALMOST_BALANCED 6		// Try to create an approximation of a balanced tree
#define BSP_BALANCE_AND_SPLITS 7	// Combine balanced tree and few splits.
#define BSP_ALMOST_BALANCE_AND_SPLITS 8	// Combine balanced tree and few splits.

/**
 * A BSP node.
 */
class csBspNode : public csPolygonTreeNode
{
  friend class csBspTree;
  friend class Dumper;

private:
  /**
   * All the polygons in this node.
   * These polygons are all on the same plane.
   * The 'front' and 'back' children in this node are seperated
   * by that plane.
   */
  csPolygonIntArray polygons;

  /**
   * This flag is true if all polygons above are on the 'splitter' plane.
   * In this case the BSP tree can backface cull them in one go.
   */
  bool polygons_on_splitter;

  /// The splitter plane.
  csPlane3 splitter;

  /// The front node.
  csBspNode* front;
  /// The back node.
  csBspNode* back;

private:
  /// Make an empty BSP node.
  csBspNode ();

  /**
   * Destroy this BSP node. The list of polygons
   * will be deleted but not the polygons themselves.
   */
  virtual ~csBspNode ();

  /**
   * Add a polygon to this BSP node.
   */
  void AddPolygon (csPolygonInt* poly);

  /**
   * Count all vertices used by polygons in this bsp node
   * and children. Vertices used multiple times will be counted
   * multiple times as well. So really it counts all the corners
   * of all polygons used in the bsp node.
   */
  int CountVertices ();

  /**
   * Fetch all vertex indices used by polygons in this bsp node
   * and children. The given array must be at least CountVertices()
   * big.
   */
  void FetchVertices (int* array, int& cur_idx);

public:
  /// Return true if node is empty.
  bool IsEmpty ()
  {
    return polygons.GetNumPolygons () == 0 &&
    	(!front || front->IsEmpty ()) &&
	(!back || back->IsEmpty ());
  }

  /// Return type (NODE_???).
  int Type () { return NODE_BSPTREE; }
};

/**
 * The BSP tree.
 */
class csBspTree : public csPolygonTree
{
  friend class Dumper;

private:
  /// The mode.
  int mode;

private:
  /**
   * Build the tree from the given node and number of polygons.
   * The 'was_splitter' array contains 'true' for all polygons that
   * have already been used as a splitter previously.
   */
  void Build (csBspNode* node, csPolygonInt** polygons, bool* was_splitter, int num);

  /**
   * Select a splitter from a list of polygons and return the index.
   */
  int SelectSplitter (csPolygonInt** polygons, bool* was_splitter, int num);

  /// Traverse the tree from back to front starting at 'node' and 'pos'.
  void* Back2Front (csBspNode* node, const csVector3& pos,
  	csTreeVisitFunc* func, void* data, csTreeCullFunc* cullfunc,
	void* culldata);
  /// Traverse the tree from front to back starting at 'node' and 'pos'.
  void* Front2Back (csBspNode* node, const csVector3& pos,
  	csTreeVisitFunc* func, void* data, csTreeCullFunc* cullfunc,
	void* culldata);

  /// Return statistics about this bsp tree.
  void Statistics (csBspNode* node, int depth, int* num_nodes,
  	int* num_leaves, int* max_depth,
  	int* tot_polygons, int* max_poly_in_node, int* min_poly_in_node);

  /// Handle all dynamic objects in this tree.
  void* HandleObjects (csBspNode* node, const csVector3& pos,
  	csTreeVisitFunc* func, void* data);

  /**
   * Process all todo stubs in a node and add new
   * todo stubs to the children of this node.
   */
  void ProcessTodo (csBspNode* node);

  /**
   * Add all visible polygons in this tree to
   * the given polygon array (for PVS).
   */
  void AddToPVS (csBspNode* node, csPolygonArrayNoFree* polygons);

public:
  /**
   * Create an empty tree for a parent container.
   */
  csBspTree (csSector* sect, int mode = BSP_MINIMIZE_SPLITS);

  /**
   * Destroy the whole BSP tree (but not the actual polygons and parent
   * objects).
   */
  virtual ~csBspTree ();

  /**
   * Create the tree with a given set of polygons.
   */
  void Build (csPolygonInt** polygons, int num);

  /**
   * Create the tree with a given set of polygons.
   */
  void Build (csPolygonArray& polygons)
  {
    Build (polygons.GetArray (), polygons.Length ());
  }

  /**
   * Create the tree for the default parent set.
   */
  void Build ();

  /// Traverse the tree from back to front starting at the root and 'pos'.
  void* Back2Front (const csVector3& pos, csTreeVisitFunc* func, void* data,
  	csTreeCullFunc* cullfunc = NULL, void* culldata = NULL);
  /// Traverse the tree from front to back starting at the root and 'pos'.
  void* Front2Back (const csVector3& pos, csTreeVisitFunc* func, void* data,
  	csTreeCullFunc* cullfunc = NULL, void* culldata = NULL);

  /// Print statistics about this bsp tree.
  void Statistics ();

  /// Return statistics about this particular tree.
  void Statistics (int* num_nodes, int* num_leaves, int* max_depth,
  	int* tot_polygons, int* max_poly_in_node, int* min_poly_in_node);

  /**
   * Get a list of all vertices used by all the polygons in this
   * bsp tree. This list should be deleted with 'delete[]' after use.
   * The vertices are returned as indices into the parent object (pset).
   */
  int* GetVertices (int& count);

  /**
   * Return true if bsp tree is empty.
   */
  bool IsEmpty () { return root ? root->IsEmpty () : false; }

  /**
   * Add all visible polygons in this tree to
   * the given polygon array (for PVS).
   */
  void AddToPVS (csPolygonArrayNoFree* polygons)
  {
    AddToPVS ((csBspNode*)root, polygons);
  }
};

#endif /*BSP_H*/

