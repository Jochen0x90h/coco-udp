#pragma once

#include "ipv6.hpp"
#include <coco/BufferDevice.hpp>


namespace coco {

/**
 * UDP socket
 */
class UdpSocket : public BufferDevice {
public:
    UdpSocket(State state) : BufferDevice(state) {}

    /**
     * Open the socket on a local port
     * @param localPort local port number
     * @return true if successful
     */
    virtual bool open(uint16_t localPort) = 0;

    /**
     * Join a multicast group
     * @param multicastGroup address of multicast group
     * @return true if successful
     */
    virtual bool join(ipv6::Address const &multicastGroup) = 0;

    // virtual bool connect(ipv6::Address const &destination) = 0;
};

} // namespace coco
