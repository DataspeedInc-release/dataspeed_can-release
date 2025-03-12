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

#include "CanExtractor.h"

#include <rclcpp/serialization.hpp>

namespace dataspeed_can_tools {

CanExtractor::CanExtractor(const std::string &dbc_file, bool offline, bool expand, bool unknown, bool copy) :
    dbc_(dbc_file), offline_(offline), bag_(new rosbag2_cpp::Writer()), expand_(expand), unknown_(unknown), copy_(copy) {}

CanExtractor::CanExtractor(const std::vector<std::string> &dbc_file, bool offline, bool expand, bool unknown, bool copy) :
    dbc_(dbc_file), offline_(offline), bag_(new rosbag2_cpp::Writer()), expand_(expand), unknown_(unknown), copy_(copy) {}


uint64_t CanExtractor::unsignedSignalData(const std::vector<uint8_t> &buffer, const RosCanSigStruct& sig_props)
{
  // 64 bytes maximum CAN FD payload.
  if (!(buffer.size() <= 64)) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "unsignedSignalData(): Buffer too large: %zu bytes", buffer.size());
    return 0;
  }

  if (!(sig_props.start_bit >= 0 && sig_props.start_bit < (int)(buffer.size() * 8))) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"),
      "unsignedSignalData(): Start bit out of range: Bit %d", sig_props.start_bit);
    return 0;
  }

  // 64 bits maximum signal length.
  if (!(sig_props.length >= 0 && sig_props.length <= 64)) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "unsignedSignalData(): Bit length out of range: %d bits", sig_props.length);
    return 0;
  }

  if (!(sig_props.order == INTEL || sig_props.order == MOTOROLA)) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "unsignedSignalData(): Invalid bit order: %d", sig_props.order);
    return 0;
  }

  uint16_t start_bit = (uint16_t)sig_props.start_bit;
  uint8_t length_bits = (uint16_t)sig_props.length;
  bool lsb_order = (sig_props.order == INTEL);

  uint64_t value = 0;
  uint8_t bits_remaining = length_bits;
  uint16_t current_bit = start_bit;
  while (bits_remaining > 0) {
    uint16_t next_bit_multiple = (((current_bit + 8) / 8) * 8);
    uint16_t prev_bit_multiple = ((current_bit / 8) * 8);
    uint8_t bits_to_consume = (lsb_order) ? (next_bit_multiple - current_bit) : (current_bit + 1 - prev_bit_multiple);
    bits_to_consume = (bits_to_consume <= bits_remaining) ? bits_to_consume : bits_remaining;
    uint8_t mask_shift = (lsb_order) ? (current_bit - prev_bit_multiple) : (current_bit + 1 - bits_to_consume - prev_bit_multiple);
    uint8_t mask = (((1 << bits_to_consume) - 1) << mask_shift);
    uint16_t value_shift = (lsb_order) ? (length_bits - bits_remaining) : (bits_remaining - bits_to_consume);
    if (!((current_bit / 8) < buffer.size())) {
       RCLCPP_WARN(
        rclcpp::get_logger("rclcpp"),
        "unsignedSignalData(): Calculated bit index exceeds buffer length: Bit %u: Max %zu",
        current_bit,
        ((buffer.size() * 8) - 1));
      return 0;
    }
    value |= ((((uint64_t)(buffer[current_bit / 8]) & mask) >> mask_shift) << value_shift);
    bits_remaining -= bits_to_consume;
    if (lsb_order) {
        current_bit += bits_to_consume;
    } else {
        if ((current_bit + 1 - bits_to_consume) % 8 == 0) {
            current_bit = (((current_bit / 8) * 8) + 15);
        } else {
            // Only hit when (bits_remaining == 0) and loop is complete.
            assert(bits_remaining == 0);
            current_bit -= bits_to_consume;
        }
    }
  }

  return value;
}

int64_t CanExtractor::signedSignalData(const std::vector<uint8_t> &buffer, const RosCanSigStruct& sig_props)
{
  uint64_t value = unsignedSignalData(buffer, sig_props);
  // Sign extension.
  if (sig_props.length != 64) {
    value |= (value & (1ul << (sig_props.length - 1))) ? (((1ul << (64 - sig_props.length)) - 1) << sig_props.length) : 0;
  }
  return (int64_t)value;
}

template<class T>
T CanExtractor::buildMsg(const RosCanSigStruct& info, const std::vector<uint8_t> &buffer, bool sign)
{
  T msg;
  if (sign) {
    msg.data = (info.factor *   signedSignalData(buffer, info)) + info.offset;
  } else {
    msg.data = (info.factor * unsignedSignalData(buffer, info)) + info.offset;
  }
  return msg;
}

int CanExtractor::getAppropriateSize(const RosCanSigStruct& sig_props, bool output_signed)
{
  if (sig_props.length >= 64) {
    return 64;
  }

  int64_t max_val;
  int64_t min_val;
  if ((sig_props.sign == SIGNED)) {
    max_val = (((int64_t)1 << (sig_props.length - 1)) - 1);
    min_val = -((int64_t)1 << (sig_props.length - 1));
  } else {
    max_val = (((int64_t)1 << sig_props.length) - 1);
    min_val = 0;
  }
  max_val = max_val * (int64_t)sig_props.factor + (int64_t)sig_props.offset;
  min_val = min_val * (int64_t)sig_props.factor + (int64_t)sig_props.offset;
  if (max_val < min_val) {
    std::swap(min_val, max_val);
  }

  if (output_signed) {
    if ((INT8_MIN <= min_val) && (max_val <= INT8_MAX)) {
      return 8;
    } else if ((INT16_MIN <= min_val) && (max_val <= INT16_MAX)) {
      return 16;
    } else if ((INT32_MIN <= min_val) && (max_val <= INT32_MAX)) {
      return 32;
    } else {
      return 64;
    }
  } else {
    if (max_val <= UINT8_MAX) {
      return 8;
    } else if (max_val <= UINT16_MAX) {
      return 16;
    } else if (max_val <= UINT32_MAX) {
      return 32;
    } else {
      return 64;
    }
  }
}

bool CanExtractor::getMessage(RosCanMsgStruct& can_msg)
{
  if (msgs_.find(can_msg.id) == msgs_.end()) {
    for (DBCIterator::const_iterator it = dbc_.begin(); it < dbc_.end(); it++) {
      if (it->getId() == can_msg.id) {
        can_msg.msg_name = it->getName();

        for (Message::const_iterator m_it = it->begin(); m_it < it->end(); m_it++) {
          RosCanSigStruct new_sig;
          new_sig.factor = m_it->getFactor();
          new_sig.length = m_it->getLength();
          new_sig.maximum = m_it->getMaximum();
          new_sig.minimum = m_it->getMinimum();
          new_sig.offset = m_it->getOffset();
          new_sig.order = m_it->getByteOrder();
          new_sig.sig_name = m_it->getName();
          new_sig.sign = m_it->getSign();
          new_sig.start_bit = m_it->getStartbit();
          new_sig.multiplexor = m_it->getMultiplexor();
          new_sig.multiplexNum = m_it->getMultiplexedNumber();
          can_msg.sigs.push_back(new_sig);
        }

        msgs_[can_msg.id] = can_msg;
        return true;
      }
    }

    if (unknown_msgs_.find(can_msg.id) == unknown_msgs_.end()) {
      unknown_msgs_[can_msg.id] = 0;
      if (unknown_) {
        std::stringstream ss;
        ss << "x" << std::hex << std::setfill('0') << std::setw(3) << can_msg.id;
        can_msg.msg_name = ss.str();
        msgs_[can_msg.id] = can_msg;
        return true;
      }
      RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Received unknown CAN message with ID = 0x%X", can_msg.id);
    }
  } else {
    can_msg = msgs_[can_msg.id];
  }

  return false;
}

void CanExtractor::initPublishers(RosCanMsgStruct& info, rclcpp::Node& node)
{
  auto qos = rclcpp::QoS(0);

  auto nh_msg = node.create_sub_node(info.msg_name);

  info.message_pub = node.create_generic_publisher(info.msg_name, "can_msgs/msg/Frame", qos);

  if (expand_) {
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "Initializing publishers for %zu signals...", info.sigs.size());
    for (size_t i=0; i<info.sigs.size(); i++){
      registerCanSignalPublisher(info.sigs[i], *nh_msg);
    }
  }

  msgs_[info.id] = info;
}

bool CanExtractor::openBag(const std::string &fname)
{
  if (!bag_open_) {
    // Save the desired file name, to actually open the bag later for writing.
    bag_->open(fname);
    bag_fname_ = fname;
    bag_open_ = true;
    return true;
  }
  return false;
}

void CanExtractor::registerCanSignalPublisher(RosCanSigStruct& info, rclcpp::Node& node)
{
  const rclcpp::QoS qos(10); // Queue size of 10.
  if (info.length == 1) {
    info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/Bool", qos);
  } else if ((fmod(info.factor, 1.0) != 0) || (fmod(info.offset, 1.0) != 0)) {
    info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/Float64", qos);
  } else {
    if ((info.sign == SIGNED) || (info.offset < 0) || (info.factor < 0)) {
      switch (getAppropriateSize(info, true)) {
        case  8: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/Int8",  qos); break;
        case 16: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/Int16", qos); break;
        case 32: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/Int32", qos); break;
        case 64: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/Int64", qos); break;
      }
    } else {
      switch (getAppropriateSize(info, false)) {
        case  8: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/UInt8",  qos); break;
        case 16: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/UInt16", qos); break;
        case 32: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/UInt32", qos); break;
        case 64: info.sig_pub = node.create_generic_publisher(info.sig_name, "std_msgs/msg/UInt64", qos); break;
      }
    }
  }
}

template<class T>
void CanExtractor::writeToBag(const std::string& frame, const rclcpp::Time& stamp, const T& msg) {
  // Check the bag file is open before writing.
  if (!bag_open_) {
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "Opening bag file for writing...");
    bag_open_ = true;
    bag_->open(bag_fname_);
  }
  bag_->write<T>(msg, frame, stamp);
}

template<class T>
void CanExtractor::pubCanSig(const RosCanMsgStruct& info, const T& sig_msg, const rclcpp::Time& stamp, size_t i) {
  RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"),
    "  Publishing value (%s): %f", info.sigs[i].sig_name.c_str(), (double)sig_msg.data);
  if (i < info.sigs.size()) {
    if (offline_) {
      writeToBag(info.msg_name + "/" + info.sigs[i].sig_name, stamp, sig_msg);
    } else {
      rclcpp::Serialization<T> serialization;
      auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

      serialization.serialize_message(&sig_msg, serialized_msg.get());
      info.sigs[i].sig_pub->publish(*serialized_msg);
    }
  }
}

void CanExtractor::pubCanMsg(const RosCanMsgStruct& info, const can_msgs::msg::Frame& msg, const rclcpp::Time& stamp) {
  if (offline_) {
    writeToBag(info.msg_name, stamp, msg);
  } else {
    rclcpp::Serialization<can_msgs::msg::Frame> serialization;
    auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

    serialization.serialize_message(&msg, serialized_msg.get());
    info.message_pub->publish(*serialized_msg);
  }
}
void CanExtractor::pubCanMsg(const RosCanMsgStruct& info, const dataspeed_can_msgs::msg::Frame& msg, const rclcpp::Time& stamp) {
  if (offline_) {
    writeToBag(info.msg_name, stamp, msg);
  } else {
    rclcpp::Serialization<dataspeed_can_msgs::msg::Frame> serialization;
    auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

    serialization.serialize_message(&msg, serialized_msg.get());
    info.message_pub->publish(*serialized_msg);
  }
}
void CanExtractor::pubCanMsg(const RosCanMsgStruct& info, const dataspeed_can_msgs::msg::Frame16& msg, const rclcpp::Time& stamp) {
  if (offline_) {
    writeToBag(info.msg_name, stamp, msg);
  } else {
    rclcpp::Serialization<dataspeed_can_msgs::msg::Frame16> serialization;
    auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

    serialization.serialize_message(&msg, serialized_msg.get());
    info.message_pub->publish(*serialized_msg);
  }
}
void CanExtractor::pubCanMsg(const RosCanMsgStruct& info, const dataspeed_can_msgs::msg::Frame32& msg, const rclcpp::Time& stamp) {
  if (offline_) {
    writeToBag(info.msg_name, stamp, msg);
  } else {
    rclcpp::Serialization<dataspeed_can_msgs::msg::Frame32> serialization;
    auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

    serialization.serialize_message(&msg, serialized_msg.get());
    info.message_pub->publish(*serialized_msg);
  }
}
void CanExtractor::pubCanMsg(const RosCanMsgStruct& info, const dataspeed_can_msgs::msg::Frame48& msg, const rclcpp::Time& stamp) {
  if (offline_) {
    writeToBag(info.msg_name, stamp, msg);
  } else {
    rclcpp::Serialization<dataspeed_can_msgs::msg::Frame48> serialization;
    auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

    serialization.serialize_message(&msg, serialized_msg.get());
    info.message_pub->publish(*serialized_msg);
  }
}
void CanExtractor::pubCanMsg(const RosCanMsgStruct& info, const dataspeed_can_msgs::msg::Frame64& msg, const rclcpp::Time& stamp) {
  if (offline_) {
    writeToBag(info.msg_name, stamp, msg);
  } else {
    rclcpp::Serialization<dataspeed_can_msgs::msg::Frame64> serialization;
    auto serialized_msg = std::make_shared<rclcpp::SerializedMessage>();

    serialization.serialize_message(&msg, serialized_msg.get());
    info.message_pub->publish(*serialized_msg);
  }
}

void CanExtractor::pubCanMsgSignals(const RosCanMsgStruct &info, const std::vector<uint8_t>& buffer, const rclcpp::Time &stamp) {

  // Search for the multiplexor value, if any.
  unsigned short multiplexorValue = -1;
  for (size_t i = 0; i < info.sigs.size(); i++) {
    if (info.sigs[i].multiplexor == MULTIPLEXOR) {
      multiplexorValue = unsignedSignalData(buffer, info.sigs[i]);
      break;
    }
  }

  // Publish signals on their own topics
  for (size_t i = 0; i < info.sigs.size(); i++) {

    // Handle multiplexed signals
    RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "MSG Name: %s", info.sigs[i].sig_pub->get_topic_name());

    if (info.sigs[i].multiplexor == MULTIPLEXED) {
      if (info.sigs[i].multiplexNum != multiplexorValue) {
        RCLCPP_DEBUG(rclcpp::get_logger("rclcpp"), "    Skipping multiplexed value...");
        continue; // Else, skip this iteration of the loop.
      } // If sigs[i].multiplexNum == multiplexorValue, it should be published.
    } // If sigs[i].multiplexor equals MULTIPLEXOR or NONE, it's fine to publish for all messages.

    // Publish various message types
    if (info.sigs[i].length == 1) {
      pubCanSig(info, buildMsg<std_msgs::msg::Bool>(info.sigs[i], buffer, false), stamp, i);
    } else if ((fmod(info.sigs[i].factor, 1.0) != 0) || fmod(info.sigs[i].offset, 1.0) != 0) {
      pubCanSig(info, buildMsg<std_msgs::msg::Float64>(info.sigs[i], buffer, info.sigs[i].sign == SIGNED), stamp, i);
    } else {
      if ((info.sigs[i].sign == SIGNED) || (info.sigs[i].offset < 0) || (info.sigs[i].factor < 0)) {
        if (info.sigs[i].sign == SIGNED) {
          switch (getAppropriateSize(info.sigs[i], true)) {
            case  8: pubCanSig(info, buildMsg<std_msgs::msg::Int8 >(info.sigs[i], buffer, true), stamp, i); break;
            case 16: pubCanSig(info, buildMsg<std_msgs::msg::Int16>(info.sigs[i], buffer, true), stamp, i); break;
            case 32: pubCanSig(info, buildMsg<std_msgs::msg::Int32>(info.sigs[i], buffer, true), stamp, i); break;
            case 64: pubCanSig(info, buildMsg<std_msgs::msg::Int64>(info.sigs[i], buffer, true), stamp, i); break;
          }
        } else {
          switch (getAppropriateSize(info.sigs[i], true)) {
            case  8: pubCanSig(info, buildMsg<std_msgs::msg::Int8 >(info.sigs[i], buffer, false), stamp, i); break;
            case 16: pubCanSig(info, buildMsg<std_msgs::msg::Int16>(info.sigs[i], buffer, false), stamp, i); break;
            case 32: pubCanSig(info, buildMsg<std_msgs::msg::Int32>(info.sigs[i], buffer, false), stamp, i); break;
            case 64: pubCanSig(info, buildMsg<std_msgs::msg::Int64>(info.sigs[i], buffer, false), stamp, i); break;
          }
        }
      } else {
        switch (getAppropriateSize(info.sigs[i], false)) {
          case  8: pubCanSig(info, buildMsg<std_msgs::msg::UInt8 >(info.sigs[i], buffer, false), stamp, i); break;
          case 16: pubCanSig(info, buildMsg<std_msgs::msg::UInt16>(info.sigs[i], buffer, false), stamp, i); break;
          case 32: pubCanSig(info, buildMsg<std_msgs::msg::UInt32>(info.sigs[i], buffer, false), stamp, i); break;
          case 64: pubCanSig(info, buildMsg<std_msgs::msg::UInt64>(info.sigs[i], buffer, false), stamp, i); break;
        }
      }
    }
  }
}

void CanExtractor::pubMessage(const can_msgs::msg::Frame& msg, const rclcpp::Time &stamp)
{
  // Check for valid message information
  const uint32_t id = msg.id | (msg.is_extended ? 0x80000000 : 0x00000000);
  if (msgs_.find(id) == msgs_.end()) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Skipping unknown message ID: 0x%03X", id);
    return;
  }
  const RosCanMsgStruct &info = msgs_[id];

  // Re-publish CAN message on named topic
  if (copy_) {
    pubCanMsg(info, msg, stamp);
  }

  // Publish individual expanded signals
  if (expand_) {
    pubCanMsgSignals(info, std::vector<uint8_t>(msg.data.begin(), msg.data.end()), stamp);
  }
}
void CanExtractor::pubMessage(const dataspeed_can_msgs::msg::Frame& msg, const rclcpp::Time &stamp)
{
  // Check for valid message information
  const uint32_t id = msg.id | (msg.extended ? 0x80000000 : 0x00000000);
  if (msgs_.find(id) == msgs_.end()) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Skipping unknown message ID: 0x%03X", id);
    return;
  }
  const RosCanMsgStruct &info = msgs_[id];

  // Re-publish CAN message on named topic
  if (copy_) {
    pubCanMsg(info, msg, stamp);
  }

  // Publish individual expanded signals
  if (expand_) {
    pubCanMsgSignals(info, std::vector<uint8_t>(msg.data.begin(), msg.data.end()), stamp);
  }
}
void CanExtractor::pubMessage(const dataspeed_can_msgs::msg::Frame16& msg, const rclcpp::Time &stamp)
{
  // Check for valid message information
  const uint32_t id = msg.id | (msg.extended ? 0x80000000 : 0x00000000);
  if (msgs_.find(id) == msgs_.end()) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Skipping unknown message ID: 0x%03X", id);
    return;
  }
  const RosCanMsgStruct &info = msgs_[id];

  // Re-publish CAN message on named topic
  if (copy_) {
    pubCanMsg(info, msg, stamp);
  }

  // Publish individual expanded signals
  if (expand_) {
    pubCanMsgSignals(info, std::vector<uint8_t>(msg.data.begin(), msg.data.end()), stamp);
  }
}
void CanExtractor::pubMessage(const dataspeed_can_msgs::msg::Frame32& msg, const rclcpp::Time &stamp)
{
  // Check for valid message information
  const uint32_t id = msg.id | (msg.extended ? 0x80000000 : 0x00000000);
  if (msgs_.find(id) == msgs_.end()) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Skipping unknown message ID: 0x%03X", id);
    return;
  }
  const RosCanMsgStruct &info = msgs_[id];

  // Re-publish CAN message on named topic
  if (copy_) {
    pubCanMsg(info, msg, stamp);
  }

  // Publish individual expanded signals
  if (expand_) {
    pubCanMsgSignals(info, std::vector<uint8_t>(msg.data.begin(), msg.data.end()), stamp);
  }
}
void CanExtractor::pubMessage(const dataspeed_can_msgs::msg::Frame48& msg, const rclcpp::Time &stamp)
{
  // Check for valid message information
  const uint32_t id = msg.id | (msg.extended ? 0x80000000 : 0x00000000);
  if (msgs_.find(id) == msgs_.end()) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Skipping unknown message ID: 0x%03X", id);
    return;
  }
  const RosCanMsgStruct &info = msgs_[id];

  // Re-publish CAN message on named topic
  if (copy_) {
    pubCanMsg(info, msg, stamp);
  }

  // Publish individual expanded signals
  if (expand_) {
    pubCanMsgSignals(info, std::vector<uint8_t>(msg.data.begin(), msg.data.end()), stamp);
  }
}
void CanExtractor::pubMessage(const dataspeed_can_msgs::msg::Frame64& msg, const rclcpp::Time &stamp)
{
  // Check for valid message information
  const uint32_t id = msg.id | (msg.extended ? 0x80000000 : 0x00000000);
  if (msgs_.find(id) == msgs_.end()) {
    RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Skipping unknown message ID: 0x%03X", id);
    return;
  }
  const RosCanMsgStruct &info = msgs_[id];

  // Re-publish CAN message on named topic
  if (copy_) {
    pubCanMsg(info, msg, stamp);
  }

  // Publish individual expanded signals
  if (expand_) {
    pubCanMsgSignals(info, std::vector<uint8_t>(msg.data.begin(), msg.data.end()), stamp);
  }
}

} // namespace dataspeed_can_tools
