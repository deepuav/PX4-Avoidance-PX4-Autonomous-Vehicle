#include <gnc_functions.hpp>

//get armed state
void state_cb(const mavros_msgs::State::ConstPtr& msg)
{
  current_state_g = *msg;
}

geometry_msgs::Point enu_2_local(nav_msgs::Odometry current_pose_enu)
{
  float x = current_pose_enu.pose.pose.position.x;
  float y = current_pose_enu.pose.pose.position.y;
  float z = current_pose_enu.pose.pose.position.z;
  float deg2rad = (M_PI/180);
  geometry_msgs::Point current_pos_local;
  current_pos_local.x = x*cos((local_offset_g - 90)*deg2rad) - y*sin((local_offset_g - 90)*deg2rad);
  current_pos_local.y = x*sin((local_offset_g - 90)*deg2rad) + y*cos((local_offset_g - 90)*deg2rad);
  current_pos_local.z = z;

  return current_pos_local;

  //ROS_INFO("Local position %f %f %f",X, Y, Z);
}

//get current position of drone
void pose_cb(const nav_msgs::Odometry::ConstPtr& msg)
{
  current_pose_g = *msg;
  enu_2_local(current_pose_g);
  float q0 = current_pose_g.pose.pose.orientation.w;
  float q1 = current_pose_g.pose.pose.orientation.x;
  float q2 = current_pose_g.pose.pose.orientation.y;
  float q3 = current_pose_g.pose.pose.orientation.z;
  float psi = atan2((2*(q0*q3 + q1*q2)), (1 - 2*(pow(q2,2) + pow(q3,2))) );
  //ROS_INFO("Current Heading %f ENU", psi*(180/M_PI));
  //Heading is in ENU
  //IS YAWING COUNTERCLOCKWISE POSITIVE?
  current_heading_g = psi*(180/M_PI) - local_offset_g;
  //ROS_INFO("Current Heading %f origin", current_heading_g);
  //ROS_INFO("x: %f y: %f z: %f", current_pose_g.pose.pose.position.x, current_pose_g.pose.pose.position.y, current_pose_g.pose.pose.position.z);
}

geometry_msgs::Point get_current_location()
{
	geometry_msgs::Point current_pos_local;
	current_pos_local = enu_2_local(current_pose_g);
	return current_pos_local;

}

float get_current_heading()
{
	return current_heading_g;
}


//set orientation of the drone (drone should always be level) 
// Heading input should match the ENU coordinate system
/**
\ingroup control_functions
This function is used to specify the drone’s heading in the local reference frame. Psi is a counter clockwise rotation following the drone’s reference frame defined by the x axis through the right side of the drone with the y axis through the front of the drone. 
@returns n/a
*/
void set_heading(float heading)
{
	local_desired_heading_g = heading; 
	heading = heading + correction_heading_g + local_offset_g;
	
	// ROS_INFO("Desired Heading %f ", local_desired_heading_g);
	float yaw = heading*(M_PI/180);
	float pitch = 0;
	float roll = 0;

	float cy = cos(yaw * 0.5);
	float sy = sin(yaw * 0.5);
	float cr = cos(roll * 0.5);
	float sr = sin(roll * 0.5);
	float cp = cos(pitch * 0.5);
	float sp = sin(pitch * 0.5);

	float qw = cy * cr * cp + sy * sr * sp;
	float qx = cy * sr * cp - sy * cr * sp;
	float qy = cy * cr * sp + sy * sr * cp;
	float qz = sy * cr * cp - cy * sr * sp;

	waypoint_g.pose.orientation.w = qw;
	waypoint_g.pose.orientation.x = qx;
	waypoint_g.pose.orientation.y = qy;
	waypoint_g.pose.orientation.z = qz;
}

std::tuple<std::pair<int, int>, size_t> get_loc(int input, int relative_to)
{
    std::map<int, std::pair<int, int>> list = 
    {
        {0, {0, 0}},
        {1, {0, 5}},
        {2, {0, -5}},
        {3, {5, 0}},
        {4, {-5, 0}},
        {5, {-5, 5}},
        {6, {5, 5}},
        {7, {-5, -5}},
        {8, {5, -5}}
        // Add more pairs as needed
    };
    
    // std::pair<int, int> result = list[input];
	std::pair<int, int> result = std::make_pair(list[input].first - list[relative_to].first, list[input].second - list[relative_to].second);
    size_t list_size = list.size();  // Get the size of the list map
    if (list.find(input) != list.end()) {
        // std::cout << "Output: " << result.first << ", " << result.second << std::endl;
    } else {
        std::cout << "Number not found in the list." << std::endl;
    }

    return std::make_tuple(result, list_size);
}

// set position to fly to in the local frame
/**
\ingroup control_functions
This function is used to command the drone to fly to a waypoint. These waypoints should be specified in the local reference frame. This is typically defined from the location the drone is launched. Psi is counter clockwise rotation following the drone’s reference frame defined by the x axis through the right side of the drone with the y axis through the front of the drone. 
@returns n/a
*/
void set_destination(float x, float y, float z, float psi)
{

	set_heading(psi);
	//transform map to local
	float deg2rad = (M_PI/180);
	float Xlocal = x*cos((correction_heading_g + local_offset_g - 90)*deg2rad) - y*sin((correction_heading_g + local_offset_g - 90)*deg2rad);
	float Ylocal = x*sin((correction_heading_g + local_offset_g - 90)*deg2rad) + y*cos((correction_heading_g + local_offset_g - 90)*deg2rad);
	float Zlocal = z;

	x = Xlocal + correction_vector_g.position.x + local_offset_pose_g.x;
	y = Ylocal + correction_vector_g.position.y + local_offset_pose_g.y;
	z = Zlocal + correction_vector_g.position.z + local_offset_pose_g.z;
	ROS_INFO("Destination set to x: %f y: %f z: %f origin frame", x, y, z);

	waypoint_g.pose.position.x = x;
	waypoint_g.pose.position.y = y;
	waypoint_g.pose.position.z = z;

	local_pos_pub.publish(waypoint_g);

}

/**
\ingroup control_functions
Wait for connect is a function that will hold the program until communication with the FCU is established.
@returns 0 - connected to fcu 
@returns -1 - failed to connect to drone
*/
int wait4connect()
{
	ROS_INFO("Waiting for FCU connection");
	// wait for FCU connection
	while (ros::ok() && !current_state_g.connected)
	{
		ros::spinOnce();
		ros::Duration(0.01).sleep();
	}
	if(current_state_g.connected)
	{
		ROS_INFO("Connected to FCU");	
		return 0;
	}else{
		ROS_INFO("Error connecting to drone");
		return -1;	
	}
	
}

/**
\ingroup control_functions
This function will create a local reference frame based on the starting location of the drone. This is typically done right before takeoff. This reference frame is what all of the the set destination commands will be in reference to.
@returns 0 - frame initialized
*/
int initialize_local_frame()
{
	//set the orientation of the local reference frame
	ROS_INFO("Initializing local coordinate system");
	local_offset_g = 0;
	for (int i = 1; i <= 30; i++) {
		ros::spinOnce();
		ros::Duration(0.1).sleep();

		

		float q0 = current_pose_g.pose.pose.orientation.w;
		float q1 = current_pose_g.pose.pose.orientation.x;
		float q2 = current_pose_g.pose.pose.orientation.y;
		float q3 = current_pose_g.pose.pose.orientation.z;
		float psi = atan2((2*(q0*q3 + q1*q2)), (1 - 2*(pow(q2,2) + pow(q3,2))) ); // yaw

		local_offset_g += psi*(180/M_PI);

		local_offset_pose_g.x = local_offset_pose_g.x + current_pose_g.pose.pose.position.x;
		local_offset_pose_g.y = local_offset_pose_g.y + current_pose_g.pose.pose.position.y;
		local_offset_pose_g.z = local_offset_pose_g.z + current_pose_g.pose.pose.position.z;

		geometry_msgs::PoseStamped pose;
		pose.pose.position.x = 0;
		pose.pose.position.y = 0;
		pose.pose.position.z = 5;
		local_pos_pub.publish(pose);
		// ROS_INFO("current heading%d: %f", i, local_offset_g/i);
	}
	local_offset_pose_g.x = local_offset_pose_g.x/30;
	local_offset_pose_g.y = local_offset_pose_g.y/30;
	local_offset_pose_g.z = local_offset_pose_g.z/30;
	local_offset_g /= 30;
	ROS_INFO("Coordinate offset set");
	ROS_INFO("the X' axis is facing: %f", local_offset_g);
	return 0;
}

/**
\ingroup control_functions
Wait for strat will hold the program until the user signals the FCU to enther mode OFFBOARD. This is typically done from a switch on the safety pilot’s remote or from the ground control station.
@returns 0 - mission started
@returns -1 - failed to start mission
*/
int wait4start()
{
    ros::Rate rate(20.0);

    mavros_msgs::SetMode offb_set_mode;
	mavros_msgs::CommandBool arm_cmd;
	arm_cmd.request.value = true;
	ros::Time last_request = ros::Time::now();
	offb_set_mode.request.custom_mode = "AUTO.TAKEOFF";
	ROS_INFO("Setting to TAKEOFF Mode...");

	while(ros::ok())
	{
		if(current_state_g.mode != "AUTO.TAKEOFF")
		{
			set_mode_client.call(offb_set_mode);
		}

		if (!current_state_g.armed && (ros::Time::now() - last_request > ros::Duration(5.0))) 
		{
			if (arming_client.call(arm_cmd) && arm_cmd.response.success) 
			{
				ROS_INFO("Vehicle armed");
				break;
			}
			last_request = ros::Time::now();
		}
		else if (current_state_g.armed) 
		{
			break;
		}
		rate.sleep();
	}

	//intitialize first waypoint of mission
	set_destination(0,0,5,0);

	// arming
	ROS_INFO("Arming drone");
	mavros_msgs::CommandBool arm_request;
	arm_request.request.value = true;
	while (!current_state_g.armed && ros::ok())
	{
		ros::Duration(.1).sleep();
		arming_client.call(arm_request);
		local_pos_pub.publish(waypoint_g);
		if(arm_request.response.success)
		{
			ROS_INFO("Arming Successful");
			break;
		}
		else
		{
			ROS_INFO("Arming failed with %d", arm_request.response.success);
			// ros::shutdown();
		}
	}

	ROS_INFO("Offboard Start");
	for(int i=0; i<1000; i++)
	{
		local_pos_pub.publish(waypoint_g);
		ros::spinOnce();
		ros::Duration(0.01).sleep();
	}

	// ROS_INFO("Waiting for user to set mode to OFFBOARD");
	offb_set_mode.request.custom_mode = "OFFBOARD";
	while(ros::ok() && current_state_g.mode != "OFFBOARD")
	{

        if (current_state_g.mode != "OFFBOARD" && (ros::Time::now() - last_request > ros::Duration(5.0))) 
		{
            if (set_mode_client.call(offb_set_mode) && offb_set_mode.response.mode_sent) 
			{
                ROS_INFO("OFFBOARD mode enabled");
            }
            last_request = ros::Time::now();
        }
		else 
		{
            if (!current_state_g.armed && (ros::Time::now() - last_request > ros::Duration(5.0))) 
			{
                if (arming_client.call(arm_cmd) && arm_cmd.response.success) 
				{
                    ROS_INFO("Vehicle armed");
                }
                last_request = ros::Time::now();
            }
        }

		set_destination(0,0,5,0);
	    ros::spinOnce();
	    // ros::Duration(0.01).sleep();
		rate.sleep();
  	}

  	if(current_state_g.mode == "OFFBOARD")
	{
		ROS_INFO("Mode set to OFFBOARD. Mission starting");
		return 0;
	}
	else
	{
		ROS_INFO("Error starting mission!!");
		return -1;	
	}
}

/**
\ingroup control_functions
This function returns an int of 1 or 0. THis function can be used to check when to request the next waypoint in the mission. 
@return 1 - waypoint reached 
@return 0 - waypoint not reached
*/
int check_waypoint_reached()
{

	//check the leader here
	local_pos_pub.publish(waypoint_g);

	//check for correct position 
	float pos_tolerance=0.3; 
	float deltaX = abs(waypoint_g.pose.position.x - current_pose_g.pose.pose.position.x);
	float deltaY = abs(waypoint_g.pose.position.y - current_pose_g.pose.pose.position.y);
	float deltaZ = abs(waypoint_g.pose.position.z - current_pose_g.pose.pose.position.z);
	float dMag = sqrt( pow(deltaX, 2) + pow(deltaY, 2) + pow(deltaZ, 2) );
	//check orientation
	// float heading_tolerance=0.01;
	// float cosErr = cos(current_heading_g*(M_PI/180)) - cos(local_desired_heading_g*(M_PI/180));
	// float sinErr = sin(current_heading_g*(M_PI/180)) - sin(local_desired_heading_g*(M_PI/180));
	// float headingErr = sqrt( pow(cosErr, 2) + pow(sinErr, 2) );
	// if( dMag < pos_tolerance && headingErr < heading_tolerance)

	if( dMag < pos_tolerance)
	{
		return 1;
	}else{
		return 0;
	}


}

/**
\ingroup control_functions
this function changes the mode of the drone to land
@returns 1 - mode change successful
@returns 0 - mode change not successful
*/
int land()
{
  mavros_msgs::CommandTOL srv_land;
  if(land_client.call(srv_land) && srv_land.response.success)
  {
    // ROS_INFO("land sent %d", srv_land.response.success);
	leader_landing_command.data = true;
	landing_command_pub.publish(leader_landing_command);
    return 0;
  }
  else
  {
    ROS_ERROR("Landing failed");
    return -1;
  }
}

void setupTransforms(tf2_ros::TransformBroadcaster &tf_broadcaster, geometry_msgs::TransformStamped &base_link_master, std::vector<geometry_msgs::TransformStamped> &transforms, int n, tf2_ros::Buffer &tf_buffer, int my_drone_id)
{

	// geometry_msgs::TransformStamped base_link_master;
	base_link_master = tf_buffer.lookupTransform("map", "base_link_" + std::to_string(my_drone_id), ros::Time(0));
	base_link_master.header.stamp = ros::Time::now();
	base_link_master.header.frame_id = "base_link_" + std::to_string(my_drone_id);
	base_link_master.child_frame_id = "swarm_master";
	base_link_master.transform.translation.x = 0.0;
	base_link_master.transform.translation.y = 0.0;
	base_link_master.transform.translation.z = 0.0;

	// Invert the rotation part of the base_link_master transform
	tf2::Quaternion base_link_master_quat(
		base_link_master.transform.rotation.x,
		base_link_master.transform.rotation.y,
		base_link_master.transform.rotation.z,
		base_link_master.transform.rotation.w
	);
	double roll, pitch, yaw;
	tf2::Matrix3x3(base_link_master_quat).getRPY(roll, pitch, yaw);

	base_link_master_quat = base_link_master_quat.inverse();

	// Create a new quaternion with only the z-axis rotation (yaw)
	tf2::Quaternion z_rotation_quat;
	z_rotation_quat.setRPY(0, 0, yaw);

	// Multiply the inverted quaternion with the z-axis rotation quaternion
	base_link_master_quat = base_link_master_quat * z_rotation_quat;

	// Apply the inverted rotation to the swarm_master frame
	base_link_master.transform.rotation.x = base_link_master_quat.x();
	base_link_master.transform.rotation.y = base_link_master_quat.y();
	base_link_master.transform.rotation.z = base_link_master_quat.z();
	base_link_master.transform.rotation.w = base_link_master_quat.w();

	std::pair<int, int> loc;
	size_t list_size;
	// std::tie(loc, list_size) = get_loc(my_drone_id);
	// base_link_master.transform.translation.x = -loc.first;
	// base_link_master.transform.translation.y = -loc.second;
	// base_link_master.transform.translation.z = 0.0;

	
	geometry_msgs::TransformStamped transform_stamped;
	for (int i = 0; i < n; ++i)
	{
		std::tie(loc, list_size) = get_loc(i, my_drone_id);

		transforms[i].header.frame_id = "swarm_master";
		transforms[i].child_frame_id = "offset_" + std::to_string(i);
		transforms[i].transform.translation.x = loc.first;
		transforms[i].transform.translation.y = loc.second;
		transforms[i].transform.translation.z = 0.0;

		transforms[i].transform.rotation.x = 0;
		transforms[i].transform.rotation.y = 0;
		transforms[i].transform.rotation.z = 0;
		transforms[i].transform.rotation.w = 1;
	}

	ros::Time now = ros::Time::now();
	base_link_master.header.stamp = now;
	tf_broadcaster.sendTransform(base_link_master);

	for (int i = 0; i < n; ++i)
	{
	transforms[i].header.stamp = now;
	tf_broadcaster.sendTransform(transforms[i]);
	}


}

void globalPositionCallback(const sensor_msgs::NavSatFix::ConstPtr& msg)
{
    if (publish_my_home_position == true)
    {
		my_home_position = *msg;
        publish_my_home_position = false;
    }
	my_home_position_pub.publish(my_home_position);
}

void leaderglobalPositionCallback(const sensor_msgs::NavSatFix::ConstPtr& msg) 
{
  	// Process the message, for example, print the received data
	leader_home_position.latitude = msg->latitude;
	leader_home_position.longitude = msg->longitude;
	leader_home_position.altitude = msg->altitude;
	// ROS_INFO("Leader home position: %f, %f, %f", leader_home_position.latitude, leader_home_position.longitude, leader_home_position.altitude);
}

void leaderlocalPositionCallback(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
    // ROS_INFO("Position: (%f, %f, %f), Orientation: (%f, %f, %f, %f)", msg->pose.position.x, msg->pose.position.y, msg->pose.position.z, msg->pose.orientation.x, msg->pose.orientation.y, msg->pose.orientation.z, msg->pose.orientation.w);
    leader_shift = *msg;
    
}


std::vector<std::tuple<int, bool, bool, int, int, int, ros::Time>> drone_info;
const ros::Duration timeout(3.0);
void messageCallback(const std_msgs::String::ConstPtr& msg)
{
    std::istringstream iss(msg->data);
    std::vector<std::string> words{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

	int drone_id = std::stoi(words[1]);
	int mission_id = std::stoi(words[4]);

    // Update the drone_info vector
    drone_info.erase(
		std::remove_if(
			drone_info.begin(), drone_info.end(), [&](const auto& info) 
						{
							return std::get<0>(info) == drone_id;
						}
					), drone_info.end()
					);
	ros::Time current_time = ros::Time::now();
    drone_info.emplace_back(drone_id, words[2] == "Leader", words[3] == "Reached", mission_id, 0 , 0 , current_time);
	
	// Sort the drone_info vector by drone_id
    std::sort(
		drone_info.begin(), drone_info.end(), [&](const std::tuple<int, bool, bool, int, int, int, ros::Time>& a, const std::tuple<int, bool, bool, int, int, int, ros::Time>& b) 
		{
        	return std::get<0>(a) < std::get<0>(b);
    	}
			);

	// std::cout << "Drones_info: " << std::endl;
    // for (const auto& t : drone_info) {
    //     std::cout << "[" << std::get<0>(t) << ", "
    //               		 << std::get<1>(t) << ", "
    //               		 << std::get<2>(t) << ", "
    //               		 << std::get<3>(t) << "]";
    // }
	// std::cout << std::endl;

}

bool all_drones_ready() {
    for (const auto& info : drone_info) {
		if (std::get<2>(info) == false)
		{
			// ROS_INFO("Drone %d is not ready", std::get<0>(info));
			return false;
		}
    }
	return true;
}


bool init_leader(const tf2_ros::Buffer &buffer, ros::NodeHandle controlnode)
{
	// Remove old entries
    ros::Time now = ros::Time::now();
    drone_info.erase(
		std::remove_if(
			drone_info.begin(), drone_info.end(), [&](const auto& info) 
				{
        			return (now - std::get<6>(info)) > timeout;
    			}
					), drone_info.end()
					);

	// std::cout << "Drones_info: " << std::endl;
	// for (const auto& t : drone_info) {
	// 	std::cout << "[" << std::get<0>(t) << ", "
	// 					 << std::get<1>(t) << ", "
	// 					 << std::get<2>(t) << ", "
	// 					 << std::get<3>(t) << ", "
	// 					 << std::get<4>(t) << ", "
	// 					 << std::get<5>(t) << ", "
	// 					 << std::get<6>(t) << "]";
	// }
	// std::cout << std::endl;

    // Print the updated drone_info vector
    for (const auto& info : drone_info) 
	{
		if (std::get<1>(info) == true)
		{
			if (std::get<0>(info) == my_drone_id)
			{
				ROS_INFO("Drone: %d I am the leader", my_drone_id);
				// Trigger wifi here
				leader_drone_id_g = my_drone_id;
				// ROS_INFO("Drone: %d counter: %d", my_drone_id, counter);
				swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Leader Reached " + std::to_string(counter);
				my_drone_id_ready_pub.publish(swarm_data);
				
				return true;
			}
			if (std::get<0>(info) != first_init_leader_id)
			{
				ROS_INFO("Leader: %d The first init leader", first_init_leader_id);
				first_init_leader_id = std::get<0>(info);
				leader_drone_id_g = first_init_leader_id;
				ROS_INFO("Leader: %d The first init leader2", first_init_leader_id);
				swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Not_Leader Not_Reached " + std::to_string(counter);
				my_drone_id_ready_pub.publish(swarm_data);
				leader_local_position_sub = controlnode.subscribe<geometry_msgs::PoseStamped>((ros_namespace + std::to_string(first_init_leader_id) + "/mavros/local_position/pose").c_str(), 10, leaderlocalPositionCallback);
				leader_global_position_sub = controlnode.subscribe<sensor_msgs::NavSatFix>((ros_namespace + std::to_string(first_init_leader_id) + "/home_position/global").c_str(), 10, leaderglobalPositionCallback);
				ros::Rate rate(20.0);
				while (ros::ok())
				{
					// convert lat lon to local x y z of the learder and my_drone
					shift_x = (leader_home_position.longitude - my_home_position.longitude)*111111;
					shift_y = (leader_home_position.latitude - my_home_position.latitude)*111111;
					shift_alt = (leader_home_position.altitude - my_home_position.altitude);

					std::pair<int, int> loc;
					size_t list_size;
					std::tie(loc, list_size) = get_loc(my_drone_id, first_init_leader_id);

					waypoint_g.pose.position.x = shift_x + leader_shift.pose.position.x + loc.first;
					waypoint_g.pose.position.y = shift_y + leader_shift.pose.position.y + loc.second;
					waypoint_g.pose.position.z = shift_alt + leader_shift.pose.position.z + my_drone_id;

					// ROS_INFO("waypoint_g: %f %f %f", waypoint_g.pose.position.x, waypoint_g.pose.position.y, waypoint_g.pose.position.z);
					// ROS_INFO("leader_shift: %f %f %f", leader_shift.pose.position.x, leader_shift.pose.position.y, leader_shift.pose.position.z);
					// ROS_INFO("shift: %f %f %f", shift_x, shift_y, shift_alt);
					// ROS_INFO("loc: %d %d", loc.first, loc.second);

					if (check_waypoint_reached() == 1)
					{
						swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Follover Reached " + std::to_string(counter);
						my_drone_id_ready_pub.publish(swarm_data);

						break;
					}
					else
					{
						swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Follover Not_reached " + std::to_string(counter);
						my_drone_id_ready_pub.publish(swarm_data);	
					}
					
					ros::spinOnce();
					rate.sleep();
				}

			}
			else
			{
				// ROS_INFO("Drone Leader: %d Not a leader and not first init", first_init_leader_id);
				std::string target_frame = "offset_" + std::to_string(my_drone_id);
				geometry_msgs::TransformStamped transform_stamped;
				try
				{
					transform_stamped = buffer.lookupTransform("map", target_frame, ros::Time(0));
				}
				catch (tf2::TransformException &ex)
				{
					ROS_WARN_ONCE("%s", ex.what());
				}

				waypoint_g.pose.position.x = shift_x + transform_stamped.transform.translation.x;
				waypoint_g.pose.position.y = shift_y + transform_stamped.transform.translation.y;
				waypoint_g.pose.position.z = shift_alt + transform_stamped.transform.translation.z + my_drone_id;

				waypoint_g.pose.orientation.x = transform_stamped.transform.rotation.x;
				waypoint_g.pose.orientation.y = transform_stamped.transform.rotation.y;
				waypoint_g.pose.orientation.z = transform_stamped.transform.rotation.z;
				waypoint_g.pose.orientation.w = transform_stamped.transform.rotation.w;

				// Get current mission number
				counter = std::get<3>(info) - 1;
				// ROS_INFO("Drone: %d counter: %d", my_drone_id, counter);

				

				if (check_waypoint_reached() == 1)
				{
					swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Follover Reached " + std::to_string(counter);
					my_drone_id_ready_pub.publish(swarm_data);

					return true;
				}
				else
				{

					swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Follover Not_reached " + std::to_string(counter);
					my_drone_id_ready_pub.publish(swarm_data);

					return false;
				}

			}

			return true;
		}
	}


    int count = static_cast<int>(std::count_if(drone_info.begin(), drone_info.end(),
                                                 [&](const std::tuple<int, bool, bool, int, int, int, ros::Time>& drone_id) {
                                                     return std::get<0>(drone_id) < my_drone_id;
                                                 }));

    std::cout << "My drone number: " << my_drone_id << " and the priority is: " << count << std::endl;

    if (count == 0)
	{
		swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Leader Reached " + std::to_string(counter);
		my_drone_id_ready_pub.publish(swarm_data);
	 	// triger wifi here
	}
	return true;

}

void landingCommandCallback(const std_msgs::Bool::ConstPtr& msg)
{
	if (msg->data == true) {
		ROS_INFO("Landing command received.");
		land();
		ros::shutdown();
	}

}

/**
\ingroup control_functions
This function is called at the beginning of a program and will start of the communication links to the FCU. The function requires the program's ros nodehandle as an input 
@returns n/a
*/
int init_publisher_subscriber(ros::NodeHandle controlnode)
{
	if (!controlnode.hasParam("namespace") && !controlnode.hasParam("my_drone_id"))
	{

		ROS_INFO("Please setup the namespace and my_drone_id parameters");
		ros::shutdown();
	}
	else
	{
		controlnode.getParam("namespace", ros_namespace);
		controlnode.getParam("drone_id_g", my_drone_id);
		controlnode.getParam("drone_nums", drone_nums);
	}

	local_pos_pub = 					controlnode.advertise<geometry_msgs::PoseStamped>((ros_namespace + std::to_string(my_drone_id) + "/mavros/setpoint_position/local").c_str(), 10);
	my_home_position_pub = 				controlnode.advertise<sensor_msgs::NavSatFix>((ros_namespace + std::to_string(my_drone_id) + "/home_position/global").c_str(), 10);
	my_drone_id_ready_pub = 			controlnode.advertise<std_msgs::String>("/drone_id_ready", 10);
	landing_command_pub = 				controlnode.advertise<std_msgs::Bool>("/leader_landing_command", 10);

    my_drone_id_ready_sub = 			controlnode.subscribe<std_msgs::String>("/drone_id_ready", 10, messageCallback); 
	landing_command_sub = 				controlnode.subscribe<std_msgs::Bool>("/leader_landing_command", 10, landingCommandCallback);
	local_position_sub = 				controlnode.subscribe<nav_msgs::Odometry>((ros_namespace + std::to_string(my_drone_id) + "/mavros/global_position/local").c_str(), 10, pose_cb);
	global_position_sub = 				controlnode.subscribe<sensor_msgs::NavSatFix>((ros_namespace + std::to_string(my_drone_id) + "/mavros/global_position/global").c_str(), 10, globalPositionCallback);
	state_sub = 						controlnode.subscribe<mavros_msgs::State>((ros_namespace + std::to_string(my_drone_id) + "/mavros/state").c_str(), 10, state_cb);

	arming_client = 					controlnode.serviceClient<mavros_msgs::CommandBool>((ros_namespace + std::to_string(my_drone_id) + "/mavros/cmd/arming").c_str());
	land_client = 						controlnode.serviceClient<mavros_msgs::CommandTOL>((ros_namespace + std::to_string(my_drone_id) + "/mavros/cmd/land").c_str());
	set_mode_client = 					controlnode.serviceClient<mavros_msgs::SetMode>((ros_namespace + std::to_string(my_drone_id) + "/mavros/set_mode").c_str());
	takeoff_client = 					controlnode.serviceClient<mavros_msgs::CommandTOL>((ros_namespace + std::to_string(my_drone_id) + "/mavros/cmd/takeoff").c_str());
	command_client = 					controlnode.serviceClient<mavros_msgs::CommandLong>((ros_namespace + std::to_string(my_drone_id) + "/mavros/cmd/command").c_str());
	auto_waypoint_pull_client = 		controlnode.serviceClient<mavros_msgs::WaypointPull>((ros_namespace + std::to_string(my_drone_id) + "/mavros/mission/pull").c_str());
	auto_waypoint_push_client = 		controlnode.serviceClient<mavros_msgs::WaypointPush>((ros_namespace + std::to_string(my_drone_id) + "/mavros/mission/push").c_str());
	auto_waypoint_set_current_client = 	controlnode.serviceClient<mavros_msgs::WaypointSetCurrent>((ros_namespace + std::to_string(my_drone_id) + "/mavros/mission/set_current").c_str());

	return 0;
}

int main(int argc, char** argv)
{
	//initialize ros 
	ros::init(argc, argv, "gnc_node");
	ros::NodeHandle gnc_node("~");

	//initialize control publisher/subscribers
	init_publisher_subscriber(gnc_node);

    std::vector<geometry_msgs::TransformStamped> transforms(drone_nums);
	geometry_msgs::TransformStamped base_link_master;
	tf2_ros::TransformBroadcaster tf_broadcaster;

	tf2_ros::Buffer buffer;
	tf2_ros::TransformListener tf_listener(buffer);

	wait4connect();
	initialize_local_frame();
	wait4start();
	
	//specify some waypoints 
	std::vector<gnc_api_waypoint> waypointList;
	gnc_api_waypoint nextWayPoint;
	nextWayPoint.x = 0;
	nextWayPoint.y = 0;
	nextWayPoint.z = 3;
	nextWayPoint.psi = 0;
	waypointList.push_back(nextWayPoint);

	nextWayPoint.x = 20;
	nextWayPoint.y = 0;
	nextWayPoint.z = 3;
	nextWayPoint.psi = -90;
	waypointList.push_back(nextWayPoint);

	nextWayPoint.x = 20;
	nextWayPoint.y = 20;
	nextWayPoint.z = 3;
	nextWayPoint.psi = 0;
	waypointList.push_back(nextWayPoint);

	nextWayPoint.x = 0;
	nextWayPoint.y = 20;
	nextWayPoint.z = 3;
	nextWayPoint.psi = 90;
	waypointList.push_back(nextWayPoint);

	nextWayPoint.x = 0;
	nextWayPoint.y = 0;
	nextWayPoint.z = 3;
	nextWayPoint.psi = 180;
	waypointList.push_back(nextWayPoint);

	nextWayPoint.x = 0;
	nextWayPoint.y = 0;
	nextWayPoint.z = 3;
	nextWayPoint.psi = 0;
	waypointList.push_back(nextWayPoint);

	ros::Rate rate(20.0);

	for (int i = 0; i < 3; i++)
	{
		swarm_data.data = "Drone " + std::to_string(my_drone_id) + " Not_Leader Not_Reached " + std::to_string(counter);
		my_drone_id_ready_pub.publish(swarm_data);
		ros::spinOnce();
		rate.sleep();
	}

	while(ros::ok())
	{

		init_leader(buffer, gnc_node);
		if (my_drone_id == leader_drone_id_g)
		{
			setupTransforms(tf_broadcaster, base_link_master, transforms, drone_nums, buffer, my_drone_id);
			// std::cout << "Paramerers" << std::endl;
			// std::cout << check_waypoint_reached() << std::endl;
			// std::cout << all_drones_ready() << std::endl;
			if(check_waypoint_reached() == 1 && all_drones_ready())
			{
				ROS_INFO("Waypoint reached");
				if (counter < waypointList.size())
				{
					set_destination(waypointList[counter].x,waypointList[counter].y,waypointList[counter].z, waypointList[counter].psi);
					counter++;	
				}
				else
				{
					//land after all waypoints are reached
					land();
					ROS_INFO("Finish Task");
					break;
				}	
			}
		}

		ros::spinOnce();
		rate.sleep();
	}
	return 0;
}