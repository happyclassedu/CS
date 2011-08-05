#ifndef __CS_IVARIA_COLLISION2_H__
#define __CS_IVARIA_COLLISION2_H__

#include "csutil/scf.h"
#include "csutil/csstring.h"
#include "csgeom/vector3.h"
#include "csgeom/matrix3.h"
#include "csgeom/transfrm.h"
#include "csgeom/plane3.h"

struct iTerrainSystem;
struct iSector;
struct iMeshWrapper;
struct iMovable;
struct iPortal;
struct iCamera;
struct iView;

namespace CS
{
namespace Physics2
{
struct iPhysicalBody;
}
}

namespace CS
{
namespace Collision2
{

struct csConvexResult;
struct iCollisionCallback;
struct iCollisionObject;
typedef short CollisionGroupMask;

enum ColliderType
{
COLLIDER_INVALID = 0,
COLLIDER_BOX,
COLLIDER_SPHERE,
COLLIDER_CYLINDER,
COLLIDER_CAPSULE,
COLLIDER_CONE,
COLLIDER_PLANE,
COLLIDER_CONVEX_MESH,
COLLIDER_CONCAVE_MESH,
COLLIDER_CONCAVE_MESH_SCALED,        
COLLIDER_TERRAIN, 
};

enum CollisionObjectType
{
COLLISION_OBJECT_BASE = 0,
COLLISION_OBJECT_PHYSICAL,
COLLISION_OBJECT_GHOST,
COLLISION_OBJECT_ACTOR,
};

struct CollisionGroup
{
  csString name;
  CollisionGroupMask value;

  CollisionGroup () {}

  CollisionGroup (const char* name)
    : name (name)
  {}
};

struct MoveResult
{
  bool hasHit;
  csVector3 hitNormalWorld;
  csVector3 hitPointWorld;
  csVector3 hitNormalLocal;
  csVector3 hitPointLocal;
};

struct HitBeamResult
{
  HitBeamResult ()
  : hasHit (false), object (0), isect (0.0f), normal (0.0f), vertexIndex (0)
  {}

  /// Whether the beam has hit a body or not.
  bool hasHit;

  /// The collision object that was hit, or \a nullptr if no object was hit.
  iCollisionObject* object;

  /// Intersection point in world space.
  csVector3 isect;

  /// Normal to the surface of the body at the intersection point.
  csVector3 normal;

  /**
  * The index of the closest vertex of the soft body to be hit. This is only valid
  * if it is a soft body which is hit.
  */
  size_t vertexIndex;
};

struct CollisionData
{
  iCollisionObject* objectA;
  iCollisionObject* objectB;
  csVector3 positionWorldOnA;
  csVector3 positionWorldOnB;
  csVector3 normalWorldOnB;
  float penetration; 
};

struct iCollisionCallback: public virtual iBase
{
  SCF_INTERFACE (CS::Collision2::iCollisionCallback, 1, 0, 0);

  /**
  * A collision occurred.
  * \param thisbody The body that received a collision.
  * \param otherbody The body that collided with \a thisBody.
  * \param collisions The list of collisions between the two bodies. 
  * \param timesteps Since how many simulation time steps this collision occured. 
  */
  virtual void OnCollision (iCollisionObject *thisbody, iCollisionObject *otherbody, 
      const csArray<CollisionData>& collisions, size_t timesteps) = 0; 
};

/**
* A base interface for colliders. 
* Other colliders will be derived from this one.
*/
struct iCollider : public virtual iBase
{
  SCF_INTERFACE (CS::Collision2::iCollider, 1, 0, 0);

  /// Get the geometry type of this collider. 
  virtual ColliderType GetGeometryType () const = 0;

  virtual void SetLocalScale (const csVector3& scale) = 0;

  virtual const csVector3& GetLocalScale () const = 0;
  
  /// Set the margin of collision shape.
  virtual void SetMargin (float margin) = 0;

  virtual float GetMargin () const = 0; 
};

/**
* A box collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderBox()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderBox : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderBox, 1, 0, 0);

  /// Get the box geometry of this collider.
  virtual csVector3 GetBoxGeometry ()  = 0;
};

/**
* A sphere collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderSphere()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderSphere : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderSphere, 1, 0, 0);

  /// Get the sphere geometry of this collider.
  virtual float GetSphereGeometry () = 0;
};

/**
* A cylinder collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderCylinder()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderCylinder : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderCylinder, 1, 0, 0);

  /// Get the cylinder geometry of this collider.
  virtual void GetCylinderGeometry (float& length, float& radius) = 0;
};

/**
* A capsule collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderCapsule()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderCapsule : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderCapsule, 1, 0, 0);

  /// Get the capsule geometry of this collider.
  virtual void GetCapsuleGeometry (float& length, float& radius) = 0;
};

/**
* A cone collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderCone()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderCone : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderCone, 1, 0, 0);

  /// Get the capsule geometry of this collider.
  virtual void GetConeGeometry (float& length, float& radius) = 0;
};

/**
* A plane collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderPlane()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderPlane : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderPlane, 1, 0, 0);

  /// Get the plane geometry of this collider.
  virtual csPlane3 GetPlaneGeometry () = 0;
};

/**
* A convex mesh collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderConvexMesh()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderConvexMesh : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderConvexMesh, 1, 0, 0);

  /// Get the mesh factory of this collider.
  virtual iMeshWrapper* GetMesh () = 0;
};

/**
* A concave mesh collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderConcaveMesh()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderConcaveMesh : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderConcaveMesh, 1, 0, 0);

  /// Get the mesh factory of this collider.
  virtual iMeshWrapper* GetMesh () = 0;
};

/**
* A scaled concave mesh collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderConcaveMeshScaled()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderConcaveMeshScaled : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderConcaveMeshScaled, 1, 0, 0);

  /// Get the concave collider scaled by this collider.
  virtual iColliderConcaveMesh* GetCollider () = 0;
};

/**
* A terrain collider.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateColliderTerrain()
* 
* Main ways to get pointers to this interface:
* - iCollisionObject::GetCollider()
* 
* Main users of this interface:
* - iCollisionObject
*/
struct iColliderTerrain : public virtual iCollider
{
  SCF_INTERFACE (CS::Collision2::iColliderTerrain, 1, 0, 0);

  /// Get the terrain system.
  virtual iTerrainSystem* GetTerrain () const = 0;
};

/**
* This is the interface of a collision object. 
* It contains the collision information of the object.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateCollisionObject()
* 
* Main ways to get pointers to this interface:
* - iCollisionSystem::GetCollisionObject()
* 
* Main users of this interface:
* - iCollisionSystem
*/
struct iCollisionObject : public virtual iBase
{
  SCF_INTERFACE (CS::Collision2::iCollisionObject, 1, 0, 0);

  virtual iCollisionObject* QueryCollisionObject () = 0;

  virtual CS::Physics2::iPhysicalBody* QueryPhysicalBody () = 0;

  /// Set the type of the collision object.
  virtual void SetObjectType (CollisionObjectType type, bool forceRebuild = true) = 0;

  /// Return the type of the collision object.
  virtual CollisionObjectType GetObjectType () = 0;

  /**
  * Set the movable attached to this collision object. Its position will be updated
  * automatically when this object is moved.
  */
  virtual void SetAttachedMovable (iMovable* movable) = 0;

  /// Get the movable attached to this collision object.
  virtual iMovable* GetAttachedMovable () = 0;
  
  /// Set the transform.
  virtual void SetTransform (const csOrthoTransform& trans) = 0;

  /// Get the transform.
  virtual csOrthoTransform GetTransform () = 0;

  /// Add a collider to this collision body.
  virtual void AddCollider (iCollider* collider, const csOrthoTransform& relaTrans) = 0;

  /// Remove the given collider from this collision object.
  virtual void RemoveCollider (iCollider* collider) = 0;

  /// Remove the collider with the given index from this collision object.
  virtual void RemoveCollider (size_t index) = 0;

  /// Get the collider with the given index.
  virtual iCollider* GetCollider (size_t index) = 0;

  /// Get the count of colliders in this collision object.
  virtual size_t GetColliderCount () = 0;

  /// Rebuild this collision object.
  virtual void RebuildObject () = 0;

  /// Set the collision group this object belongs to by name.
  virtual void SetCollisionGroup (const char* name) = 0;

  /// Get the collision group this object belongs to.
  virtual const char* GetCollisionGroup () const = 0;

  /**
  * Set a callback to be executed when this body collides with another.
  * If 0, no callback is executed.
  */
  virtual void SetCollisionCallback (iCollisionCallback* cb) = 0;

  /// Get the collision response callback.
  virtual iCollisionCallback* GetCollisionCallback () = 0;

  /// Test collision with another collision objects.
  virtual bool Collide (iCollisionObject* otherObject) = 0;

  /// Follow a beam from start to end and return whether this body was hit.
  virtual HitBeamResult HitBeam (
      const csVector3& start, const csVector3& end) = 0;

  virtual size_t GetContactObjectsCount () = 0;

  virtual iCollisionObject* GetContactObject (size_t index) = 0;
};

/**
* A iCollisionActor is a kinematic collision object. It has a faster collision detection and response.
* You can use it to create a player or character model with gravity handling.
*
* Main creators of instances implementing this interface:
* - iCollisionSystem::CreateCollisionActor()
* 
* Main ways to get pointers to this interface:
* - iCollisionSystem::GetCollisionActor()
* 
* Main users of this interface:
* - iCollisionSystem
*/
// kickvb: most of this would have to be redesigned, let's do it later
struct iCollisionActor : public virtual iCollisionObject
{
  SCF_INTERFACE (CS::Collision2::iCollisionActor, 1, 0, 0);

  /// Check if we are on the ground.
  virtual bool IsOnGround () = 0;

  /// Set the onground status.
  //virtual void SetOnGround (bool og) = 0;

  virtual void SetCamera (iCamera* camera) = 0;

  /**
  * Set current rotation in angles around every axis and set to actor.
  * If a camera is used, set it to camera too.
  */
  virtual void SetRotation (const csMatrix3& rot) = 0;

  virtual void Rotate (const csVector3& v, float angle) = 0;

  /// Move the actor.
  virtual void UpdateAction (float delta) = 0;

  /// Set the walking velocity of the actor.
  virtual void SetVelocity (float speed) = 0;

  /**
  * This is used by UpdateAction() but you can also call it manually.
  * It will adjust the new position to match with collision
  * detection.
  */
  virtual void PreStep () = 0;

  /**
  * This is used by UpdateAction() but you can also call it manually.
  * Move the actor to proper target position.
  */
  virtual void PlayerStep (float delta) = 0;

  /// Set the falling speed.
  virtual void SetFallSpeed (float fallSpeed) = 0;

  /// Set the jumping speed.
  virtual void SetJumpSpeed (float jumpSpeed) = 0;

  /// Set the max jump height an actor can have.
  virtual void SetMaxJumpHeight (float maxJumpHeight) = 0;

  /// Set the step up height an actor can have.
  virtual void StepHeight (float stepHeight) = 0;

  /// Let the actor jump.
  virtual void Jump () = 0;

  /**
  * The max slope determines the maximum angle that the actor can walk up.
  * The slope angle is measured in radians.
  */
  virtual void SetMaxSlope (float slopeRadians) = 0;
  
  /// Get the max slope.
  virtual float GetMaxSlope () const = 0;
};

struct iCollisionSector : public virtual iBase
{
  SCF_INTERFACE (CS::Collision2::iCollisionSector, 1, 0, 0);

  /// Set the global gravity.
  virtual void SetGravity (const csVector3& v) = 0;

  /// Get the global gravity.
  virtual csVector3 GetGravity () const = 0;

  /**
  * Add a collision object into the sector.
  * The collision object has to be initialized.
  */
  virtual void AddCollisionObject (iCollisionObject* object) = 0;

  /// Remove a collision object by pointer.
  virtual void RemoveCollisionObject (iCollisionObject* object) = 0;

  /// Get the count of collision objects.
  virtual size_t GetCollisionObjectCount () = 0;

  /// Get the collision object by index.
  virtual iCollisionObject* GetCollisionObject (size_t index) = 0;

  /// Add a portal into the sector. Collision objects crossing a portal will be switched from iCollisionSector's.
  virtual void AddPortal (iPortal* portal, const csOrthoTransform& meshTrans) = 0;

  /// Remove the given portal from this sector.
  virtual void RemovePortal (iPortal* portal) = 0;

  /**
  * Set the engine iSector related to this collision sector. The iMovable that are 
  * attached to a iCollisionObject present in this collision sector will be put automatically in the given engine sector.
  */
  virtual void SetSector (iSector* sector) = 0;

  /// Get the engine iSector related to this collison sector.
  virtual iSector* GetSector () = 0;

  /// Follow a beam from start to end and return the first body that is hit.
  virtual HitBeamResult HitBeam (
      const csVector3& start, const csVector3& end) = 0;

  /**
  * Follow a beam from start to end and return the first body that is hit.
  */
  //Lulu: What's this? return the first portal that is hit?
  virtual HitBeamResult HitBeamPortal (
      const csVector3& start, const csVector3& end) = 0;

  /**
  * Performs a discrete collision test against all objects in this iCollisionSector.
  * it reports one or more contact points for every overlapping object
  */
  virtual bool CollisionTest (iCollisionObject* object, csArray<CollisionData>& collisions) = 0;

  /// Create a collision group.
  virtual CollisionGroup& CreateCollisionGroup (const char* name) = 0;

  /// Find a collision group by name.
  virtual CollisionGroup& FindCollisionGroup (const char* name) = 0;

  /// Set whether the two groups collide with each other.
  virtual void SetGroupCollision (const char* name1,
    const char* name2, bool collide) = 0;

  /// Get true if the two groups collide with each other.
  virtual bool GetGroupCollision (const char* name1, const char* name2) = 0;

  /**
  * Add a collision actor into the sector.
  * The collision actor has to be initialized.
  */
  virtual void AddCollisionActor (iCollisionActor* actor) = 0;

  /// Remove a collision object by pointer.
  virtual void RemoveCollisionActor () = 0;

  /// Get the collision object by index.
  virtual iCollisionActor* GetCollisionActor () = 0;

  /**
  * Try to move the given object from \a fromWorld to \a toWorld and return the first collision occured if any.
  */
  // kickvb: a test only on convex colliders is probably not interesting, so remove this method if not possible to do on any collision object
  /*virtual MoveResult MoveTest (iCollisionObject* object,
      const csOrthoTransform& fromWorld, const csOrthoTransform& toWorld) = 0;*/

};

/**
* This is the Collision plug-in. This plugin is a factory for creating
* iCollider, iCollisionObject, iCollisionSector and iCollisionActor
* entities. 
*
* Main creators of instances implementing this interface:
* - OPCODE plugin (crystalspace.collisiondetection.opcode)
* - Bullet plugin (crystalspace.dynamics.bullet)
*
* Main ways to get pointers to this interface:
* - csQueryRegistry()
*/
struct iCollisionSystem : public virtual iBase
{
  SCF_INTERFACE (CS::Collision2::iCollisionSystem, 1, 0, 0);

  virtual void SetInternalScale (float scale) = 0;

  /// Create a convex mesh collider.
  virtual csRef<iColliderConvexMesh> CreateColliderConvexMesh (
    iMeshWrapper* mesh, bool simplify = false) = 0;

  /// Create a concave mesh collider.
  virtual csRef<iColliderConcaveMesh> CreateColliderConcaveMesh (iMeshWrapper* mesh) = 0;

  /// Create a scaled concave mesh collider.
  virtual csRef<iColliderConcaveMeshScaled> CreateColliderConcaveMeshScaled (
    iColliderConcaveMesh* collider, csVector3 scale) = 0;

  /// Create a cylinder collider.
  virtual csRef<iColliderCylinder> CreateColliderCylinder (float length, float radius) = 0;

  /// Create a box collider.
  virtual csRef<iColliderBox> CreateColliderBox (const csVector3& size) = 0;

  /// Create a sphere collider.
  virtual csRef<iColliderSphere> CreateColliderSphere (float radius) = 0;

  /// Create a capsule collider.
  virtual csRef<iColliderCapsule> CreateColliderCapsule (float length, float radius) = 0;

  /// Create a cone collider.
  virtual csRef<iColliderCone> CreateColliderCone (float length, float radius) = 0;

  /// Create a plane collider.
  virtual csRef<iColliderPlane> CreateColliderPlane (const csPlane3& plane) = 0;

  /// Create a terrain collider.
  virtual csRef<iColliderTerrain> CreateColliderTerrain (iTerrainSystem* terrain,
      float minHeight = 0, float maxHeight = 0) = 0;

  /**
  * Create a collision object. Without any initialization.
  * Need to call iCollisionObject::RebuildObject.
  */
  virtual csRef<iCollisionObject> CreateCollisionObject () = 0;

  /**
  * Create a collision actor.
  * Need to call iCollisionObject::RebuildObject.
  */
  virtual csRef<iCollisionActor> CreateCollisionActor () = 0;
  
  /// Create a collision sector.
  virtual csRef<iCollisionSector> CreateCollisionSector () = 0;

  /**
  * Decompose a concave mesh in convex parts. Each convex part will be added to
  * the collision object as a separate iColliderConvexMesh.
  */
  virtual void DecomposeConcaveMesh (iCollisionObject* object, 
    iMeshWrapper* mesh, bool simplify = false) = 0;
};
}
}

#endif