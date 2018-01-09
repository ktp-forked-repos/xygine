#ifndef XY_NET_PEER_INL_
#define XY_NET_PEER_INL_

template <>
inline std::string NetPeer::getAddress<_ENetPeer>() const
{
    return Detail::getEnetPeerAddress(m_peer);
}

template <>
inline sf::Uint16 NetPeer::getPort<_ENetPeer>() const
{
    return Detail::getEnetPeerPort(m_peer);
}

template <>
inline sf::Uint32 NetPeer::getID<_ENetPeer>() const
{
    return Detail::getEnetPeerID(m_peer);
}

template <>
inline sf::Uint32 NetPeer::getRoundTripTime<_ENetPeer>()const
{
    return Detail::getEnetRoundTrip(m_peer);
}

template <>
inline NetPeer::State NetPeer::getState<_ENetPeer>() const
{
    return Detail::getEnetPeerState(m_peer);
}

#endif //XY_NET_PEER_INL_
