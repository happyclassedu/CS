/*
  Copyright (C) 2010 Christian Van Brussel, Communications and Remote
  Sensing Laboratory of the School of Engineering at the 
    Universite catholique de Louvain, Belgium
    http://www.tele.ucl.ac.be

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifndef __KRYSTAL_H__
#define __KRYSTAL_H__

#include "hairtest.h"

class KrystalScene : public AvatarScene
{
public:
  KrystalScene (HairTest* hairTest);
  ~KrystalScene ();

  // Camera related
  csVector3 GetCameraStart ();
  float GetCameraMinimumDistance ();
  csVector3 GetCameraTarget ();

  // Dynamic simulation related
  float GetSimulationSpeed ();
  bool HasPhysicalObjects ();

  // Creation of objects
  bool CreateAvatar ();

  // User interaction with the scene
  void ResetScene ();

  // Display of information on the state of the scene
  void UpdateStateDescription ();

  // Switch Fur Physics
  void SwitchFurPhysics();

private:
  HairTest* hairTest;

  // Ragdoll node related
  bool krystalDead;
  csRef<CS::Animation::iSkeletonRagdollNode2> ragdollNode;
  CS::Animation::iBodyChain* bodyChain;
  CS::Animation::iBodyChain* hairChain;

  // Krystal's hairs & skirt (soft bodies)
  csRef<iMeshWrapper> hairsMesh;
  csRef<iMeshWrapper> skirtMesh;
  csRef<CS::Physics::Bullet::iSoftBody> hairsBody;
  csRef<CS::Physics::Bullet::iSoftBody> skirtBody;

  // Hair physics
  bool hairPhysicsEnabled;
  csRef<CS::Mesh::iFurPhysicsControl> hairPhysicsControl;
  csRef<CS::Mesh::iFurPhysicsControl> animationPhysicsControl;
  csRef<iRigidBody> headBody;
};

#endif // __KRYSTAL_H__