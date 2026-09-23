cmake_minimum_required(VERSION 3.22)
project(dataspeed_can_usb)

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
find_package(std_msgs REQUIRED)
find_package(can_msgs REQUIRED)
find_package(lusb REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/CanDriver.cpp
  src/CanUsb.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}
  ${std_msgs_TARGETS}
  ${can_msgs_TARGETS}
  lusb::lusb
  rclcpp::rclcpp
  rclcpp_components::component
  rclcpp_components::component_manager
)
rclcpp_components_register_node(${PROJECT_NAME}
  PLUGIN "${PROJECT_NAME}::CanDriver"
  EXECUTABLE can_node
)

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION include/${PROJECT_NAME}
)
install(DIRECTORY scripts
  DESTINATION lib/${PROJECT_NAME}
  USE_SOURCE_PERMISSIONS
)
install(DIRECTORY firmware launch
  DESTINATION share/${PROJECT_NAME}
)

# Install udev rules with dh_installudev: http://manpages.ubuntu.com/manpages/man1/dh_installudev.1.html
FILE(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/debian/udev)
FILE(GLOB UDEV_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/udev" "${CMAKE_CURRENT_SOURCE_DIR}/udev/*.rules")
FOREACH(FILENAME ${UDEV_FILES})
  MESSAGE(STATUS "Appending contents of udev/${FILENAME} to debian/udev")
  FILE(READ udev/${FILENAME} FILEDATA)
  FILE(APPEND ${CMAKE_CURRENT_SOURCE_DIR}/debian/udev ${FILEDATA})
ENDFOREACH(FILENAME)
install(DIRECTORY udev
        DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(${PROJECT_NAME}_test_mac_addr "tests/test_mac_addr.cpp")
  target_include_directories(${PROJECT_NAME}_test_mac_addr PUBLIC "include")
  target_link_libraries(${PROJECT_NAME}_test_mac_addr
    ${can_msgs_TARGETS}
    rclcpp::rclcpp
  )

  ament_add_gtest(${PROJECT_NAME}_test_module_version "tests/test_module_version.cpp")
  target_include_directories(${PROJECT_NAME}_test_module_version PUBLIC "include")
  target_link_libraries(${PROJECT_NAME}_test_module_version
    ${can_msgs_TARGETS}
    rclcpp::rclcpp
  )

endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_package()
