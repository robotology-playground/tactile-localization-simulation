/******************************************************************************
 *                                                                            *
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

/**
 * @author: Nicola Piga <nicolapiga@gmail.com>
 */

// yarp
#include <yarp/os/ConnectionWriter.h>

#include "headers/HandControlModule.h"

typedef std::map<iCub::skinDynLib::SkinPart, iCub::skinDynLib::skinContactList> skinPartMap;

bool HandControlModule::getNumberContacts(iCub::skinDynLib::skinContactList &skin_contact_list,
					  std::unordered_map<std::string, int> &number_contacts)
{
    // clear number of contacts for each finger
    int n_thumb = 0;
    int n_index = 0;
    int n_middle = 0;
    int n_ring = 0;
    int n_little = 0;

    if (skin_contact_list.size() != 0)
    {
	// split contacts per SkinPart
	skinPartMap map = skin_contact_list.splitPerSkinPart();

	// take the right skinPart
	iCub::skinDynLib::SkinPart skinPart;
	if (hand_name == "right")
	    skinPart = iCub::skinDynLib::SkinPart::SKIN_RIGHT_HAND;
	else
	    skinPart = iCub::skinDynLib::SkinPart::SKIN_LEFT_HAND;

	// count contacts coming from finger tips only
	iCub::skinDynLib::skinContactList &list = map[skinPart];
	for (size_t i=0; i<list.size(); i++)
	{
	    // need to verify if this contact was effectively produced
	    // by taxels on the finger tips
	    // in order to simplify things the Gazebo plugin only sends one
	    // taxel id that is used to identify which finger is in contact
	    std::vector<unsigned int> taxels_ids = list[i].getTaxelList();
	    unsigned int taxel_id = taxels_ids[0];
	    // taxels ids for finger tips are between 0 and 59
	    if (taxel_id >= 0 && taxel_id < 12)
		n_index++;
	    else if (taxel_id >= 12 && taxel_id < 24)
		n_middle++;
	    else if (taxel_id >= 24 && taxel_id < 36)
		n_ring++;
	    else if (taxel_id >= 36 && taxel_id < 48)
		n_little++;
	    else if (taxel_id >= 48 && taxel_id < 60)
		n_thumb++;
	}
    }

    number_contacts["thumb"] = n_thumb;
    number_contacts["index"] = n_index;
    number_contacts["middle"] = n_middle;
    number_contacts["ring"] = n_ring;
    number_contacts["little"] = n_little;

    return true;
}

void HandControlModule::processCommand(const HandControlCommand &cmd,
				       HandControlResponse &response)
{
    // set default response
    response.clear();

    // extract command value
    Command command = cmd.getCommand();

    // check if the command is for this hand
    // (it should not happen)
    if (cmd.getCommandedHand() != hand_name)
    {
	// ignore this command
	return;
    }

    if (command == Command::Empty ||
	command == Command::Idle)
    {
	// nothing to do here
	return;
    }

    // perform actions common to
    // several commands
    if (command == Command::Approach ||
	command == Command::Follow ||
	command == Command::Restore ||
	command == Command::Stop)
    {
	// change the current command
	current_command = command;

	// get commanded fingers
	cmd.getCommandedFingers(commanded_fingers);
    }

    switch(command)
    {

    case Command::ApproachStatus:
    {
	// set the status in the response
	response.setIsApproachDone(is_approach_done);

	break;
    }

    case Command::RestoreStatus:
    {
	// set the status in the response
	response.setIsRestoreDone(is_restore_done);

	break;
    }

    case Command::Approach:
    case Command::Follow:
    {
	// get requested speeds
	cmd.getForwardSpeed(linear_forward_speed);

	// remove pending contact points
	// from last session
	while (port_contacts.getPendingReads() > 0)
	    port_contacts.read(false);

	if (command == Command::Approach)
	{
	    // reset detected contacts within the
	    // hand controller
	    hand.resetFingersContacts();

	    // reset status
	    is_approach_done = false;
	}

	break;
    }

    case Command::Restore:
    {
	// get requested joints speeds
	cmd.getRestoreSpeed(joint_restore_speed);

	// reset status
	is_restore_done = false;

	break;
    }
    }
}

void HandControlModule::performControl()
{
    // get command safely
    mutex.lock();
    Command cmd = current_command;
    mutex.unlock();

    // switch according to the current command
    switch(cmd)
    {
    case Command::Empty:
    case Command::Idle:
    {
	// nothing to do here
	break;
    }

    case Command::Approach:
    case Command::Follow:
    {
	iCub::skinDynLib::skinContactList* list_ptr;
	iCub::skinDynLib::skinContactList empty_list;

	// read from contacts port
	list_ptr = port_contacts.read(false);
	if (list_ptr == YARP_NULLPTR)
	    list_ptr = &empty_list;

	// extract contact informations
	std::unordered_map<std::string, int> number_contacts;
	getNumberContacts(*list_ptr, number_contacts);

	// command fingers
	bool done = false;
	bool ok = false;
	if (cmd == Command::Approach)
	{
	    ok = hand.moveFingersUntilContact(commanded_fingers,
					      linear_forward_speed,
					      number_contacts,
					      done);
	}
	else if (cmd == Command::Follow)
	{
	    ok = hand.moveFingersMaintainingContact(commanded_fingers,
						    linear_forward_speed,
						    number_contacts);
	}

	if (!ok)
	{
	    // something went wrong
	    // stop finger movements
	    stopControl();

	    // go in Idle
	    mutex.lock();
	    current_command = Command::Idle;
	    mutex.unlock();

	    return;
	}

	// in case of Approach
	// check if contact was reached for all the fingers
	if (cmd == Command::Approach)
	{
	    if (done)
	    {
		mutex.lock();

		// approach phase completed
		// go in Idle
		current_command = Command::Idle;

		// update flag
		is_approach_done = true;

		mutex.unlock();
	    }
	}

	break;
    }

    case Command::Restore:
    {
	// issue finger restore command
	hand.restoreFingersPosition(commanded_fingers,
				    joint_restore_speed);

	// go in WaitRestoreDone
	mutex.lock();
	current_command = Command::WaitRestoreDone;
	mutex.unlock();

	break;
    }

    case Command::WaitRestoreDone:
    {
	bool is_done;
	bool ok;
	ok = hand.isFingersRestoreDone(commanded_fingers,
				       is_done);

	// this is commented due to issues with Gazebo
	// if (!ok)
	// {
	//     // something went wrong
	//     // stop finger movements
	//     stopControl();

	//     // go in Idle
	//     mutex.lock();
	//     current_command = Command::Idle;
	//     mutex.unlock();

	//     return;
	// }

	mutex.lock();

	// update flag
	is_restore_done = is_done;

	// go in Idle when done
	if (is_done)
	    current_command = Command::Idle;

	mutex.unlock();

	break;
    }

    case Command::Stop:
    {
	stopControl();
	break;
    }
    }
}

void HandControlModule::stopControl()
{
    // stop any ongoing movement
    hand.stopFingers(commanded_fingers);
}

bool HandControlModule::configure(yarp::os::ResourceFinder &rf)
{
    // get the name of the hand to be controlled
    hand_name = rf.find("handName").asString();
    if (rf.find("handName").isNull())
    {
	yError() << "HandControlModule::configure"
		 << "Error: cannot find parameter 'handName'"
		 << "in current configuration";
	return false;
    }

    yarp::os::ResourceFinder inner_rf;
    inner_rf = rf.findNestedResourceFinder(hand_name.c_str());

    // get the period
    period = inner_rf.find("period").asDouble();
    if (inner_rf.find("period").isNull())
	period = 0.03;
    yInfo() << "HandControlModule: period is" << period;
    
    // get the name of the contact points port
    port_contacts_name = inner_rf.find("contactsInputPort").asString();
    if (inner_rf.find("contactsInputPort").isNull())
	port_contacts_name = "/hand-control/" + hand_name + "/contacts:i";
    yInfo() << "HandControlModule: contact points input port name is" << port_contacts_name;

    // get the name of rpc port
    port_rpc_name = inner_rf.find("rpcPort").asString();
    if (inner_rf.find("rpcPort").isNull())
	port_rpc_name = "/hand-control/" + hand_name + "/rpc:i";
    yInfo() << "HandControlModule: rpc port name is" << port_rpc_name;
    
    // open the contact points port
    bool ok = port_contacts.open(port_contacts_name);
    if (!ok)
    {
	yError() << "HandControlModule::configure"
		 << "Error: unable to open the contacts port";
	return false;
    }

    // open the rpc server port
    ok = rpc_server.open(port_rpc_name);
    if (!ok)
    {
	yError() << "HandControlModule::configure"
		 << "Error: unable to open the rpc port";
	return false;
    }

    // configure callback for rpc
    rpc_server.setReader(*this);

    // configure hand the hand controller
    ok = hand.configure(hand_name);
    if (!ok)
    {
	yError() << "HandControlModule::configure"
		 << "Error: unable to configure the"
		 << hand_name
		 << "hand controller";
	return false;
    }

    // reset current command
    current_command = Command::Idle;

    // reset flags
    is_approach_done = false;
    is_restore_done = false;
 	
    return true;
}

double HandControlModule::getPeriod()
{
    return period;
}

bool HandControlModule::updateModule()
{
    performControl();

    return true;
}

bool HandControlModule::close()
{
    // stop all movements for safety
    stopControl();

    // close ports
    port_contacts.close();
    rpc_server.close();
}

bool HandControlModule::read(yarp::os::ConnectionReader& connection)
{
    // get command from the connection
    HandControlCommand hand_cmd;
    bool ok = hand_cmd.read(connection);
    if (!ok)
    {
	yError() << "HandControlModule::read"
		 << "Error: unable to read the hand control command"
		 << "from the incoming connection";
	return false;
    }

    // process the received commands
    HandControlResponse response;

    mutex.lock();

    processCommand(hand_cmd, response);

    mutex.unlock();

    // sends the response back
    yarp::os::ConnectionWriter* to_sender = connection.getWriter();
    if (to_sender == NULL)
    {
	yError() << "HandControlModule::read"
		 << "Error: unable to get a ConnectionWriter from the"
		 << "incoming connection";

	return false;
    }
    response.write(*to_sender);

    return true;
}

int main(int argc, char **argv)
{
    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
	yError() << "HandControlModule: cannot find YARP!";
	return 1;
    }

    // instantiate the resource finder
    yarp::os::ResourceFinder rf;
    rf.setDefaultConfigFile("hand_control_module_config.ini");
    rf.configure(argc,argv);

    // instantiate the localizer
    HandControlModule controller;

    // run the controller
    controller.runModule(rf);
}
