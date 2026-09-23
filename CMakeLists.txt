cmake_minimum_required(VERSION 3.22)
project(dataspeed_can_tools)

# Default to C++20
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(std_msgs REQUIRED)
find_package(can_msgs REQUIRED)
find_package(dataspeed_can_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosbag2_cpp REQUIRED)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Enabling coverage testing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

# Detect rosbag2_storage version to handle single or dual recv/send timestamps
message(STATUS "rosbag2_cpp_VERSION ${rosbag2_cpp_VERSION}")
if(rosbag2_cpp_VERSION VERSION_LESS "0.26.0")
  message(STATUS "rosbag2_storage with single message timestamp")
  add_compile_definitions(rosbag2_storage_recv_time_stamp=time_stamp)
  add_compile_definitions(rosbag2_storage_send_time_stamp=time_stamp)
else()
  message(STATUS "rosbag2_storage with separate recv/send timestamp")
  add_compile_definitions(rosbag2_storage_recv_time_stamp=recv_timestamp)
  add_compile_definitions(rosbag2_storage_send_time_stamp=send_timestamp)
endif()

add_compile_options(${TARGET_NAME} -Wall -Wextra -Werror)

add_library(${PROJECT_NAME}
  src/DbcIterator.cpp src/DbcMessage.cpp src/DbcSignal.cpp
  src/CanExtractor.cpp
)
target_link_libraries(${PROJECT_NAME}
  ${std_msgs_TARGETS}
  ${can_msgs_TARGETS}
  ${dataspeed_can_msgs_TARGETS}
  rclcpp::rclcpp
  rosbag2_cpp::rosbag2_cpp
)

add_executable(dbc_node src/dbc_node.cpp)
target_link_libraries(dbc_node
  ${PROJECT_NAME}
)

add_executable(dbc_bag src/dbc_bag.cpp)
target_link_libraries(dbc_bag
  ${PROJECT_NAME}
)

# Install targets
install(TARGETS ${PROJECT_NAME} dbc_node dbc_bag
        ARCHIVE DESTINATION lib/${PROJECT_NAME}
        LIBRARY DESTINATION lib/${PROJECT_NAME}
        RUNTIME DESTINATION lib/${PROJECT_NAME}
)
# Install test files
install(
    DIRECTORY tests/
    DESTINATION share/${PROJECT_NAME}/tests
    FILES_MATCHING
    PATTERN "*.dbc"
)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

ament_export_libraries(${PROJECT_NAME})

ament_package()