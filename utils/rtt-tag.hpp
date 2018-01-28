#ifndef NDN_RTT_TAG_H
#define NDN_RTT_TAG_H

#include "ns3/tag.h"

namespace ns3 {
namespace ndn {

class RTTTag : public Tag
{
public:
	RTTTag (): m_rtt_label (0) {}

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::ndn::RTTTag")
      .SetParent<Tag> ()
      .AddConstructor<RTTTag> ()
    ;
    return tid;
  }

  virtual TypeId GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  virtual uint32_t GetSerializedSize (void) const
  {
    return sizeof(uint64_t);
  }

  virtual void Serialize (TagBuffer i) const
  {
    i.WriteDouble (m_rtt_label);
  }

  virtual void Deserialize (TagBuffer i)
  {
    m_rtt_label = i.ReadDouble();
  }

  virtual void Print (std::ostream &os) const
  {
    os << "Label= " << m_rtt_label;
  }

  void SetLabel (uint64_t label)
  {
	  m_rtt_label = label;
  }

  uint64_t GetLabel (void) const
  {
    return m_rtt_label;
  }

private:
  uint64_t m_rtt_label;
};

} //ndn
} //ns3
#endif // NDN_REROUTE_TAG_H
