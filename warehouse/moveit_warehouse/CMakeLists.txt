cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

# Set the build type.  Options are:
#  Coverage       : w/ debug symbols, w/o optimization, w/ code-coverage
#  Debug          : w/ debug symbols, w/o optimization
#  Release        : w/o debug symbols, w/ optimization
#  RelWithDebInfo : w/ debug symbols, w/ optimization
#  MinSizeRel     : w/o debug symbols, w/ optimization, stripped binaries
#set(ROS_BUILD_TYPE RelWithDebInfo)

rosbuild_init()

rosbuild_add_boost_directories()

#set the default path for built executables to the "bin" directory
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#set the default path for built libraries to the "lib" directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#common commands for building c++ executables and libraries
rosbuild_add_boost_directories()
rosbuild_add_library(${PROJECT_NAME} 
  src/moveit_message_storage.cpp
  src/planning_scene_storage.cpp
  src/constraints_storage.cpp
  src/warehouse_connector.cpp)
rosbuild_link_boost(${PROJECT_NAME} date_time thread system)

rosbuild_add_executable(warehouse_connector_test test/warehouse_connector_test.cpp)
target_link_libraries(warehouse_connector_test ${PROJECT_NAME})

rosbuild_add_executable(broadcast src/broadcast.cpp)
target_link_libraries(broadcast ${PROJECT_NAME})
rosbuild_link_boost(broadcast program_options)

rosbuild_add_executable(save_to_warehouse src/save_to_warehouse.cpp)
target_link_libraries(save_to_warehouse ${PROJECT_NAME})
rosbuild_link_boost(save_to_warehouse program_options)
