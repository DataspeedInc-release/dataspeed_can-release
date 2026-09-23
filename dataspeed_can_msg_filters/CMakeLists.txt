cmake_minimum_required(VERSION 3.22)
project(dataspeed_can_msg_filters)

# Default to C++20
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Enabling coverage testing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

find_package(ament_cmake REQUIRED)
find_package(can_msgs REQUIRED)
find_package(rclcpp REQUIRED)

# Detect rclcpp version and revert back to C++17 for pre ROS Jazzy
message(STATUS "rclcpp_VERSION ${rclcpp_VERSION}")
if(rclcpp_VERSION VERSION_LESS "28.0.0")
  set(CMAKE_CXX_STANDARD 17)
endif()

install(
  DIRECTORY include/${PROJECT_NAME}/
  DESTINATION include/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(${PROJECT_NAME}_test tests/test.cpp)
  target_include_directories(${PROJECT_NAME}_test PUBLIC include)
  target_link_libraries(${PROJECT_NAME}_test
    ${can_msgs_TARGETS}
    rclcpp::rclcpp
  )
endif()

ament_export_include_directories(include)
ament_package()