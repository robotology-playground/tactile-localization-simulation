/******************************************************************************
 *                                                                            *
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

/**
 * @author: Nicola Piga <nicolapiga@gmail.com>
 */

#include "headers/FingerController.h"

using namespace yarp::math;

bool FingerController::configure(const std::string &hand_name,
				 const std::string &finger_name,
				 yarp::dev::IControlMode2 *imod,
				 yarp::dev::IPositionControl2 *ipos,				 
				 yarp::dev::IVelocityControl2 *ivel)
{
    bool ok;

    // store pointer to ControlMode2 instance
    this->imod = imod;

    // store pointer to PositionControl2 instance
    this->ipos = ipos;
    
    // store pointer to VelocityControl2 instance
    this->ivel = ivel;
    
    // store name of the finger
    this->finger_name = finger_name;

    // store name of the hand
    this->hand_name = hand_name;
    
    // initialize the finger
    finger = iCub::iKin::iCubFinger(hand_name + "_" + finger_name);

    // set the controlled joints depending on the finger name
    if (finger_name == "thumb")
    {
	// up to now only thumb opposition is considered
	ctl_joints.push_back(8);
    }
    else if (finger_name == "index")
    {
	ctl_joints.push_back(11);
	ctl_joints.push_back(12);
    }
    else if (finger_name == "middle")
    {
	ctl_joints.push_back(13);
	ctl_joints.push_back(14);	
    }
    else if (finger_name == "ring")
    {
	ctl_joints.push_back(15);
	// FIX ME :the forward kinematics of the ring finger is not available
	// using the forward kinematics of the index finger
	finger = iCub::iKin::iCubFinger(hand_name + "_index");
    }
    else
    {
	yError() << "FingerController:configure"
		 << "Error: finger"
		 << finger_name
		 << "is not valid or not supported";
	return false;
    }

    // get the current control modes for the controlled DoFs
    // FIX ME: not working with Gazebo
    // initial_modes.resize(ctl_joints.size());
    // ok = imod->getControlModes(ctl_joints.size(),
    // 		   		  ctl_joints.getFirst(),
    // 				  initial_modes.getFirst());
    
    // set the velocity control mode for the controlled DoFs
    ok = setControlMode(VOCAB_CM_VELOCITY);
    if (!ok)
    {
	yError() << "FingerController:configure"
		 << "Error: unable to set the velocity control"
		 << "mode for the joints of the"
		 << hand_name << finger_name
		 << "finger";
	
	return false;
    }

    // compose jacobian coupling matrix
    coupling.resize(3, ctl_joints.size());
    coupling = 0;
    if (finger_name == "index" || finger_name == "middle")
    {
	coupling[0][0] = 1;   // proximal joint velocity = velocity of first DoF
	coupling[1][1] = 0.5; // first distal joint velocity = half velocity of second DoF
	coupling[2][1] = 0.5; // second distal joint velocity = half velocity of second DoF
    }
    else if (finger_name == "ring")
    {
	// within ring only one DoF is available
	coupling = 1.0 / 3.0;
    }
    else if (finger_name == "thumb")
    {
	// only thumb opposition is considered
	coupling.resize(1, 1);
	coupling = 1.0;
    }

    // extract the constant transformation between the hand
    // and the root frame of the finger once for all
    bool use_axis_angle = true;
    yarp::sig::Vector pose;
    pose = finger.Pose(0, use_axis_angle);
    finger_root_pos = pose.subVector(0,2);
    finger_root_att = yarp::math::axis2dcm(pose.subVector(3,6));
    finger_root_att = finger_root_att.submatrix(0, 2, 0, 2);

    // set default home joints position
    joints_home.resize(ctl_joints.size(), 0.0);

    return true;
}

bool FingerController::setControlMode(const int &mode)
{
    bool ok;

    // get current control modes first
    yarp::sig::VectorOf<int> modes(ctl_joints.size());
    ok = imod->getControlModes(ctl_joints.size(),
			       ctl_joints.getFirst(),
			       modes.getFirst());
    if (!ok)
    {
	yError() << "FingerController:setControlMode"
		 << "Error: unable to get current joints control modes for finger"
		 << hand_name << finger_name;
	return false;
    }

    // set only the control modes different from the desired one
    for (size_t i=0; i<modes.size(); i++)
    {
	if (modes[i] != mode)
	{
	    ok = imod->setControlMode(ctl_joints[i], mode);
	    if (!ok)
	    {
		yError() << "FingerController:setControlMode"
			 << "Error: unable to set control mode for one of the joint of the finger"
			 << hand_name << finger_name;
		return false;
	    }
	}
    }
    
    return true;
}

bool FingerController::close()
{
    bool ok;
    
    // stop motion
    ok = ivel->stop(ctl_joints.size(), ctl_joints.getFirst());
    if (!ok)
    {
	yError() << "FingerController::close"
		 << "WARNING: unable to stop joints motion for finger"
		 << hand_name << finger_name;
	return false;
    }

    // restore initial control mode
    // FIX ME: not working with Gazebo 
    // ok = imod->setControlModes(ctl_joints.size(),
    // 				  ctl_joints.getFirst(),
    // 				  initial_modes.getFirst());
    // if (!ok)
    // {
    // 	yError() << "FingerController:close"
    // 		 << "Error: unable to restore the initial control modes for finger"
    // 		 << hand_name << finger_name;
    // 	return false;
    // }
    
    return true;
}

void FingerController::setHomePosition(const yarp::sig::Vector &encoders)
{
    // extract values of controlled DoF
    for (size_t i=0; i<ctl_joints.size(); i++)
	joints_home[i] = encoders[ctl_joints[i]];
}

bool FingerController::updateFingerChain(const yarp::sig::Vector &encoders)
{
    bool ok;
    
    // get subset of joints related to the finger
    ok = finger.getChainJoints(encoders, joints);
    if (!ok)
    {
	yError() << "FingerController::updateFingerChain"
		 << "Error: unable to retrieve the finger's joint values for finger"
		 << hand_name << finger_name;
	return false;
    }

    // convert to radians
    joints = joints * (M_PI/180.0);

    // update chain
    finger.setAng(joints);

    return true;
}

bool FingerController::getJacobianFingerFrame(yarp::sig::Matrix &jacobian)
{
    // jacobian for linear velocity part
    yarp::sig::Matrix j_lin;

    // jacobian for angular velocity part
    yarp::sig::Matrix j_ang;

    // get the jacobian
    jacobian = finger.GeoJacobian();

    // neglect abduction if index or ring
    if (finger_name == "index" || finger_name == "ring")
	jacobian.removeCols(0, 1);
    // retain only opposition if thumb
    else if (finger_name == "thumb")
	jacobian.removeCols(1, 3);

    // the number of columns should be one for the thumb
    bool valid_no_cols = true;
    if (finger_name == "thumb")
    {
	if (jacobian.cols() != 1)
	    valid_no_cols = false;
    }
    // the number of columns should be three
    // for index, middle and ring
    else{
	if (jacobian.cols() != 3)
	    valid_no_cols = false;
    }
    if (!valid_no_cols)
    {
	yError() << "FingerController::getJacobianFingerFrame"
		 << "Error: wrong number of columns"
		 << "for the jacobian of the finger"
		 << hand_name << finger_name;
	
	return false;
    }

    // extract linear velocity part
    if (finger_name == "thumb")
	j_lin = jacobian.submatrix(0, 2, 0, 0);
    else
	j_lin = jacobian.submatrix(0, 2, 0, 2);

    // express linear velocity in the root frame of the finger
    j_lin = finger_root_att.transposed() * j_lin;

    
    // the motion of the finger described w.r.t its root frame
    // is planar and velocities along the z axis (y axis for thumb opposition)
    // are zero hence the third row (second row) of the linear velocity jacobian can be dropped
    if (finger_name == "thumb")
	j_lin = j_lin.removeRows(1, 1);
    else
	j_lin = j_lin.removeRows(2, 1);

    // extract angular velocity part
    if (finger_name == "thumb")
	j_ang = jacobian.submatrix(3, 5, 0, 0);
    else
	j_ang = jacobian.submatrix(3, 5, 0, 2);

    // express angular velocity in root frame of the finger
    j_ang = finger_root_att.transposed() * j_ang;

    // the motion of the finger described w.r.t its root frame
    // is planar and angular velocities is all along the z axis
    // (-y axis for the thumb opposition)
    // hence first and second row (first and third) of the angular velocity jacobian
    // can be dropped
    if (finger_name == "thumb")
    {
	j_ang.removeRows(0, 1);
	// after the first removeRows
	// the third of the angular jacobian occupies the second row
	j_ang.removeRows(1, 1);
    }
    else
    {
	j_ang.removeRows(0, 2);
    }

    // compose the linear velocity and angular velocity parts together
    if (finger_name == "thumb")
	jacobian.resize(3, 1);
    else
	jacobian.resize(3, 3);
    jacobian.setSubmatrix(j_lin, 0, 0);
    jacobian.setSubmatrix(j_ang, 2, 0);

    // take into account coupling
    jacobian = jacobian * coupling;
}

bool FingerController::getFingerTipPoseFingerFrame(yarp::sig::Vector &pose)
{
    // get the position of the finger tip
    yarp::sig::Vector finger_tip = finger.EndEffPosition();

    // evaluate the vector from the root frame of the finger
    // to the finger tip
    yarp::sig::Vector diff = finger_tip - finger_root_pos;

    // express it in the root frame of the finger
    diff = finger_root_att.transposed() * diff;

    // evaluate the sum of the controlled joints
    // representing the attitude of the planar chain
    double att = 0;

    // only opposition if thumb
    if (finger_name == "thumb")
	att = joints[0];
    // neglect abduction if index or ring    
    else if (finger_name == "index" || finger_name == "ring")
	att = joints[1] + joints[2] + joints[3];
    // middle finger
    else
	att = joints[0] + joints[1] + joints[2];
    
    pose.resize(3);
    pose[0] = diff[0];
    pose[1] = diff[1];
    pose[2] = att;
    
    return true;
}

bool FingerController::goHome(const double &ref_vel)
{
    bool ok;
    
    // switch to position control
    ok = setControlMode(VOCAB_CM_POSITION);
    if (!ok)
    {
	yInfo() << "FingerController::goHome Error:"
		<< "unable to set Position control mode for finger"
		<< finger_name;

	return false;
    }

    // set reference joints velocities
    // the same velocity is used for all the joints
    yarp::sig::Vector speeds(ctl_joints.size(), ref_vel);
    ok = ipos->setRefSpeeds(ctl_joints.size(),
			    ctl_joints.getFirst(),
			    speeds.data());
    if (!ok)
    {
	yInfo() << "FingerController::goHome Error:"
		<< "unable to set joints reference speeds for finger"
		<< finger_name;

	return false;
    }
    
    // restore initial position of finger joints
    ok = ipos->positionMove(ctl_joints.size(),
			    ctl_joints.getFirst(),
			    joints_home.data());
    if (!ok)
    {
	yInfo() << "FingerController::goHome Error:"
		<< "unable to restore initial positions of joints of finger"
		<< finger_name;

	// stop movements for safety
	stop();

	return false;
    }

    return true;
}

bool FingerController::isPositionMoveDone(bool &done)
{
    bool ok;

    return ipos->checkMotionDone(ctl_joints.size(),
				 ctl_joints.getFirst(),
				 &done);
}

bool FingerController::setJointsVelocities(const yarp::sig::Vector &vels)
{
    bool ok;
    
    // switch to velocity control
    ok = setControlMode(VOCAB_CM_VELOCITY);
    if (!ok)
    {
	yInfo() << "FingerController::setJointsVelocites Error:"
		<< "unable to set Velocity control mode for finger"
		<< finger_name;

	return false;
    }
    
    // convert velocities to deg/s
    yarp::sig::Vector vels_deg = vels * (180.0/M_PI);

    // issue velocity command
    return ivel->velocityMove(ctl_joints.size(), ctl_joints.getFirst(), vels_deg.data());
}

bool FingerController::moveFingerForward(const double &speed)
{
    // get the jacobian in the current configuration
    yarp::sig::Matrix jac;
    getJacobianFingerFrame(jac);

    // remove attitude part (i.e. third row)
    jac.removeRows(2, 1);

    // remove velocity along x part (i.e. first row)
    jac.removeRows(0, 1);
    
    // find joint velocities minimizing v_y - J_y * q_dot
    yarp::sig::Vector q_dot;
    yarp::sig::Vector vel(1, speed);
    yarp::sig::Matrix jac_inv;

    jac_inv = jac.transposed() *
	yarp::math::pinv(jac * jac.transposed());
    q_dot = jac_inv * vel;

    // try to avoid too much displacement for the first
    // joint for fingers index and middle
    if (finger_name == "index" ||
	finger_name == "middle")
    {
	// evaluate null projector
	yarp::sig::Matrix eye2(2, 2);
	yarp::sig::Matrix projector;
	
	eye2.eye();
	projector = eye2 - jac_inv * jac;

	// get current value of the first joint
	double joint;
	if (finger_name == "index")
	    joint = joints[1];
	else
	    joint = joints[0];

	// evaluate gradient of the repulsive potential
	yarp::sig::Vector q_dot_limits(2, 0.0);
	double joint_comfort = 10 * (M_PI / 180);
	double joint_max = 25 * (M_PI / 180);
	double gain = 10;
	q_dot_limits[0] = -0.5 * (joint - joint_comfort) /
	    pow(joint_max, 2);

	q_dot += projector * gain * q_dot_limits;
    }

    // issue velocity command
    bool ok = setJointsVelocities(q_dot);
    if (!ok)
    {
	yInfo() << "FingerController::moveFingerForward Error:"
		<< "unable to set joints velocities for finger"
		<< finger_name;

	// stop movements for safety
	stop();

	return false;
    }

    return true;
}

bool FingerController::stop()
{
    bool ok;
    
    // stop motion
    ok = ivel->stop(ctl_joints.size(), ctl_joints.getFirst());
    if (!ok)
	return false;

    return true;
}
