#pragma once

#include <coco/platform/UdpSocket_native.hpp>


using namespace coco;

// drivers for UdpSocketTest
struct Drivers {
	Loop_native loop;
	UdpSocket_native socket{loop};
	UdpSocket_native::Buffer buffer1{socket, sizeof(ipv6::Endpoint) + 128};
	UdpSocket_native::Buffer buffer2{socket, sizeof(ipv6::Endpoint) + 128};
};

Drivers drivers;
