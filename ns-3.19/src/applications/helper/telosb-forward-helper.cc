
#include "ns3/telosb.h"

#include "telosb-forward-helper.h"
#include "ns3/node-container.h"

namespace ns3
{

    TelosBForwardHelper::TelosBForwardHelper ()
            : m_ifIndex (0)
    {
        m_factory.SetTypeId (TelosB::GetTypeId ());
    }

    void TelosBForwardHelper::SetLocal (Ipv6Address ip)
    {
        m_localIp = ip;
    }

    void TelosBForwardHelper::SetRemote (Ipv6Address ip)
    {
        m_remoteIp = ip;
    }

    void TelosBForwardHelper::SetAttribute (std::string name, const AttributeValue& value)
    {
        m_factory.Set (name, value);
    }

    ApplicationContainer TelosBForwardHelper::Install (NodeContainer c)
    {
        ApplicationContainer apps;
        for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
        {
            Ptr<Node> node = *i
            Ptr<TelosB> client = m_factory.Create<TelosB> ();
            node->AddApplication (client);
            apps.Add (client);
        }
        return apps;
    }

} /* namespace ns3 */

