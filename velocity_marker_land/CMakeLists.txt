cmake_minimum_required(VERSION 2.8.3)
project(velocity_marker_land)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages

#add dynamic reconfigure api
#find_package(catkin REQUIRED dynamic_reconfigure)
#generate_dynamic_reconfigure_options(
#	cfg/tracking_pid.CMAKE_CFG_INTDIR
#	)


find_package(catkin REQUIRED COMPONENTS
  roscpp
  tf
  std_msgs
  sensor_msgs
  geometry_msgs
)

################################################
## Declare ROS messages, services and actions ##
################################################

## Generate messages in the 'msg' folder
add_message_files(
  DIRECTORY msg
  FILES
  command.msg
)

## Generate added messages and services with any dependencies listed here
generate_messages(
  DEPENDENCIES
  std_msgs
)

catkin_package(
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)

## Declare a C++ executable
add_executable(velocity_marker_land_node src/velocity_marker_land_node.cpp)
#add_executable(tracking_pid_node src/tracking_pid.cpp)

add_executable(land_on_apriltag src/land_on_apriltag.cpp)
#add_dependencies(velocity_marker_tracker_node $(velocity_marker_tracker)_gencfg)
#add_executable(auto_land_node src/auto_land.cpp)

## Specify libraries to link a library or executable target against
target_link_libraries(velocity_marker_land_node
  ${catkin_LIBRARIES}
)

target_link_libraries(land_on_apriltag
${catkin_LIBRARIES}
)

#target_link_libraries(auto_land_node
#${catkin_LIBRARIES}
#)