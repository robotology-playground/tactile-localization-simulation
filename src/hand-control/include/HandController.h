/******************************************************************************
 *                                                                            *
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

/**
 * @author: Nicola Piga
 */

#ifndef HAND_CONTROLLER_H
#define HAND_CONTROLLER_H

// yarp
#include <yarp/sig/Vector.h>
#include <yarp/sig/Matrix.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/api.h>
#include <yarp/dev/IPositionControl2.h>
#include <yarp/dev/IVelocityControl2.h>
#include <yarp/dev/IControlMode2.h>
#include <yarp/dev/IControlLimits2.h>
#include <yarp/dev/IAnalogSensor.h>

// icub-main
#include <iCub/iKin/iKinFwd.h>

// std
#include <string>
#include <vector>
#include <unordered_map>

#include "FingerController.h"

class HandController
{
private:
    // hand name
    std::string hand_name;

    // driver
    yarp::dev::PolyDriver drv_arm;
    yarp::dev::PolyDriver drv_analog;

    // views
    yarp::dev::IEncoders *ienc_arm;
    yarp::dev::IAnalogSensor *ianalog_arm;
    yarp::dev::IControlMode2 *imod_arm;
    yarp::dev::IControlLimits2 *ilimits_arm;

    yarp::dev::IPositionControl2 *ipos_arm;
    yarp::dev::IVelocityControl2 *ivel_arm;

    // fingers
    std::vector<std::string> fingers_names;
    std::unordered_map<std::string, FingerController> fingers;
    std::unordered_map<std::string, bool> contacts;

    // whether to use or not additional finger encoders
    bool use_analogs;

public:
    /*
     * Configure the hand controller.
     * @param robot_name is the name of the robot
     * @param hand_name is the name of the hand
     * @param use_analogs whether to use additional encoders for fingers
     * @return true/false con success/failure
     */
    bool configure(yarp::os::ResourceFinder &rf,
                   const std::string &robot_name,
                   const std::string &hand_name,
                   const bool &use_analogs = false);

    /*
     * Close all the finger controllers and
     * close the drivers.
     * @return true/false con success/failure
     */
    bool close();

    /*
     * Get motor encoders of the whole arm
     * @param encs a yarp::sig::Vector containing encoders
     * @return true/false con success/failure
     */
    bool getMotorEncoders(yarp::sig::Vector &encs);

    /*
     * Get additional analogs encoders of the fingers
     * @param encs a yarp::sig::Vector containing encoders values
     * @return true/false con success/failure
     */
    bool getAnalogsEncoders(yarp::sig::Vector &encs);

    /*
     * Reset the internal state used within the method moveFingersUntilContact.
     * @return true/false con success/failure
     */
    void resetFingersContacts();

    /*
     * Move the fingers forward until a contact is detected. Once a contact
     * is detected for a given finger the finger stops moving forward.
     *
     * To be used in "streaming" mode by providing tactile feedback using the
     * input number_contacts.
     *
     * @param names list of fingers involved in the movement
     * @param speed the desired speed in the positive y direction
     *              of the root frame of the finger
     * @param fingers_contacts the current status of contacts for each finger
     * @param done true if all fingers reached contact, false otherwise
     * @return true/false con success/failure
     */
    bool moveFingersUntilContact(const std::vector<std::string> names,
                                 const double &speed,
                                 const std::unordered_map<std::string, bool> &number_contacts,
                                 bool &done);

    bool moveFingersMaintainingContact(const std::vector<std::string> names,
                                       const double &speed,
                                       const std::unordered_map<std::string, bool> &fingers_contacts);

    /* Restore the initial configuration for the specified fingers
     * @param ref_vel reference joints velocity used during movement
     * @param finger_list the list of fingers to be commanded
     * @return true/false con success/failure
     */
    bool restoreFingersPosition(const std::vector<std::string> &finger_list,
                                const double &ref_vel);

    /*
     * Return true if the fingers positions have been restored
     * @param finger_list the list of fingers to check for
     * @param is_done whether the restore is done or not for the fingers
     *        in finger_list
     * @return true/false con success/failure
     */
    bool isFingersRestoreDone(const std::vector<std::string> &finger_list,
                              bool &is_done);

    /*
     * Stop all the fingers.
     * @param finger_list the list of the fingers to stop
     * @return true/false con success/failure
     */
    bool stopFingers(const std::vector<std::string> finger_list);

    /*
     * Switch to position control for the specified fingers
     * @param finger_list the list of the fingers
     * @return true/false con success/failure
     */
    bool switchToPositionControl(const std::vector<std::string> finger_list);
};

#endif
