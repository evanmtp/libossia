#pragma once
#include <ossia/network/value/value.hpp>

#include <oscpack/osc/OscOutboundPacketStream.h>
#include <oscpack/osc/OscReceivedElements.h>
namespace oscpack
{
class OutboundPacketStream;
}
namespace ossia
{
namespace net
{

struct osc_outbound_visitor
{
  oscpack::OutboundPacketStream& p;
  void operator()(impulse) const
  {
  }

  void operator()(int32_t i) const
  {
    p << int32_t(i);
  }

  void operator()(float f) const
  {
    p << float(f);
  }

  void operator()(bool b) const
  {
    p << int32_t(b);
  }

  void operator()(char c) const
  {
    p << int32_t(c);
  }

  void operator()(const std::string& str) const
  {
    p << (ossia::string_view)str;
  }

  void operator()(vec2f vec) const
  {
    p << vec[0] << vec[1];
  }

  void operator()(vec3f vec) const
  {
    p << vec[0] << vec[1] << vec[2];
  }

  void operator()(vec4f vec) const
  {
    p << vec[0] << vec[1] << vec[2] << vec[3];
  }

  void operator()(const std::vector<value>& t) const
  {
    for (const auto& val : t)
    {
      val.apply(*this);
    }
  }

  void operator()() const
  {
  }
};
}
}
