cmake_minimum_required(VERSION 3.5)
project(dataspeed_can_msgs)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Frame.msg"
  "msg/Frame16.msg"
  "msg/Frame32.msg"
  "msg/Frame48.msg"
  "msg/Frame64.msg"
  DEPENDENCIES
  "std_msgs"
)

ament_package()
