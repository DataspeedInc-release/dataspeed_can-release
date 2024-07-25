/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015-2020, Dataspeed Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Dataspeed Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/subscription.hpp>
#include "CanExtractor.h"
#include <can_msgs/msg/frame.hpp>

rclcpp::Node::SharedPtr node_;
dataspeed_can_tools::CanExtractor* extractor_;

void recv(const can_msgs::msg::Frame::ConstSharedPtr& msg)
{
  if (!msg->is_error && !msg->is_rtr) {
    dataspeed_can_tools::RosCanMsgStruct info;
    info.id = msg->id | (msg->is_extended ? 0x80000000 : 0x00000000);

    if (extractor_->getMessage(info)) {
      RCLCPP_DEBUG(node_->get_logger(), "New message ID (%d), initializing publishers...", info.id);
      extractor_->initPublishers(info, *node_);
    }

    extractor_->pubMessage(msg);
  }
}

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  node_ = rclcpp::Node::make_shared("can_parser_node");

  // Declare parameters
  node_->declare_parameter("dbc_files", rclcpp::ParameterType::PARAMETER_STRING_ARRAY);
  node_->declare_parameter("expand",    rclcpp::ParameterType::PARAMETER_BOOL);
  node_->declare_parameter("unknown",   rclcpp::ParameterType::PARAMETER_BOOL);

  std::vector<std::string> dbc_files;
  if (!node_->get_parameter("dbc_files", dbc_files)) {
    RCLCPP_FATAL(node_->get_logger(), "DBC file not specified. Exiting.");
    rclcpp::shutdown(nullptr, "DBC file not specified.");
    return -1;
  }
  bool expand;
  expand = node_->get_parameter_or<bool>("expand", true);
  bool unknown;
  unknown = node_->get_parameter_or<bool>("unknown", false);

  printf("Opening dbc files: \n");
  for (unsigned int i = 0; i < dbc_files.size(); i++) {
    printf("  - %s\n", dbc_files[i].c_str());
  }
  dataspeed_can_tools::CanExtractor extractor(dbc_files, false, expand, unknown);
  extractor_ = &extractor;

  auto qos = rclcpp::QoS(100);
  auto sub_can = node_->create_subscription<can_msgs::msg::Frame>("can_rx", qos, recv);

  rclcpp::spin(node_);
}
