cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

# Set the build type.  Options are:
#  Coverage       : w/ debug symbols, w/o optimization, w/ code-coverage
#  Debug          : w/ debug symbols, w/o optimization
#  Release        : w/o debug symbols, w/ optimization
#  RelWithDebInfo : w/ debug symbols, w/ optimization
#  MinSizeRel     : w/o debug symbols, w/ optimization, stripped binaries
set(ROS_BUILD_TYPE Release)

rosbuild_init()

rosbuild_add_boost_directories()

#set the default path for built executables to the "bin" directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#set the default path for built libraries to the "lib" directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

rosbuild_add_executable(environment_server src/environment_server.cpp)

rosbuild_add_executable(print_planning_model_info src/print_planning_model_info.cpp)

rosbuild_add_executable(display_random_state src/display_random_state.cpp)

rosbuild_add_executable(visualize_robot_collision_volume src/visualize_robot_collision_volume.cpp)

rosbuild_add_executable(evaluate_collision_checking_speed src/evaluate_collision_checking_speed.cpp)
rosbuild_link_boost(evaluate_collision_checking_speed thread program_options)

rosbuild_add_executable(inverse_kinematics_sanity_checker src/inverse_kinematics_sanity_checker_main.cpp)

rosbuild_add_gtest(test_planning_scene_distance_field src/test_planning_scene_distance_field.cpp)
