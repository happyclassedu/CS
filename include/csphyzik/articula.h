/*
    Dynamics/Kinematics modeling and simulation library.
    Copyright (C) 1999 by Michael Alexander Ewert

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

#ifndef ARTICULATEDBODY_H
#define ARTICULATEDBODY_H

#include "csphyzik/rigidbod.h"
#include "csphyzik/ctmspat.h"
#include "csphyzik/ctvspat.h"

#define ABSTATESIZE 0  // all state is in solver and joint

class ctJoint;
class ctFeatherstoneAlgorithm;
class ctInverseKinematics;

// articulated body.  Definition: a handle ( rigid body ) with 0 or more  
// articulated bodies attached to it via joints
class ctArticulatedBody : public ctPhysicalEntity
{
public:

	//!me this should be in ctJoint
	static void set_joint_friction( double pfrict = DEFAULT_JOINT_FRICTION );

	// debug var
	int tag;

	ctArticulatedBody();
	ctArticulatedBody( ctRigidBody *phandle );

	virtual ~ctArticulatedBody();

	// return reference frame for this handle ( to world )
	ctReferenceFrame *get_handle_RF(){ if( handle ) return handle->get_RF(); else return NULL; }

	// return the physical body that is the root of this articulated figure
	ctRigidBody *get_handle(){ return handle; }

	// rotate this link around it's axis by ptheta radians.
	void rotate_around_axis( real ptheta );

	// link two bodies together with a revolute joint.
	// pin_joint_offset is the offset from the inboard( parent ) link to joint 
	// pout_joint_offset is from the outbaord (child) link to the joint.
	// all offsets are directed from a link's C.O.M. to the joint
	void link_revolute( ctArticulatedBody *child, ctVector3 &pin_joint_offset, ctVector3 &pout_joint_offset, ctVector3 &pjoint_axis );

	virtual void apply_given_F( ctForce &frc );

	// compute force and torque from all forces acting on this articulated body
	// that includeds this handle and all children down to the leaf nodes. 
	void apply_forces( real t );

	virtual void init_state();

	// methods for state to and from integrator
	virtual int get_state_size();
	virtual int set_state( real *sa );
	virtual int get_state( const real *sa );
	virtual int set_delta_state( real *state_array ); 

	// state methods for all children.  just need to call these for root and
	// it will recurse down and call standard state methods appropriately for
	// all children
	int set_state_links( real *sa );
	int get_state_links( const real *sa );
	int set_delta_state_links( real *state_array );
	
	// ungrounded articulated bodies are not staticaly attached to the world frame
	void make_grounded(){ is_grounded = true; };
	void make_ungrounded(){ is_grounded = false; };

	// forward dynamics algorithm.  The default solver
	ctFeatherstoneAlgorithm *install_featherstone_solver();

	// inverse kinematics algorithm.  set the goal after this
	ctInverseKinematics *install_IK_solver();

	// propagates velocities from base link to ends of articulated bodies
	// also calculates R_fg and r_fg
	void compute_link_velocities();

	// calculate relative frame of reference from parent
	void calc_relative_frame();

	void update_links();

	friend class ctFeatherstoneAlgorithm;
	friend class ctInverseKinematics;

protected:

	ctRigidBody *handle;	// rigid body for the root of this articulated body
	ctJoint *inboard_joint;  // joint attaching this body to parent
	ctLinkList_ctArticulatedBody outboard_links;  // children 

	bool is_grounded;  // is it fixed to the world frame?

	// work variables

	// calculated in compute_link_velocities
	ctMatrix3 R_fg; // coord transfrom matrix, not rotation, from inboard link
					// frame to this frame
	ctVector3 r_fg; // vector from center of inboard frame to this frame in this
					// frame's coords

};


#endif
