#pragma once

#include "ipv6.hpp"
#include <coco/Device.hpp>


namespace coco {

/**
	UDP socket
*/
class UdpSocket : public Device {
public:
	/**
		Open the socket on a local port
		@param localPort local port number
		@return true if successful
	*/
	virtual bool open(uint16_t localPort) = 0;

	/**
		Join a multicast group
		@param multicastGroup address of multicast group
		@return true if successful
	*/
	virtual bool join(ipv6::Address const &multicastGroup) = 0;

	/**
		Close the socket
	*/
	virtual void close() = 0;
};

} // namespace coco
