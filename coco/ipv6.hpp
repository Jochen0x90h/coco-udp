#pragma once

#include <coco/String.hpp>


namespace coco {
namespace ipv6 {

constexpr uint32_t networkByteOrder(uint32_t x) {
	return (x >> 24)
		| ((x >> 8) & 0x0000ff00)
		| ((x << 8) & 0x00ff0000)
		| (x << 24);
}

union Address {
	uint8_t u8[16];
	uint32_t u32[4];

	static Address fromString(String s);

	bool linkLocal() const {
		return this->u32[0] == networkByteOrder(0xfe800000U) && this->u32[1] == 0;
	}

	bool operator ==(const Address &b) const {
		for (int i = 0; i < 4; ++i) {
			if (this->u32[i] != b.u32[i])
				return false;
		}
		return true;
	}
};

struct Endpoint {
	Address address;
	uint16_t port;

	//static Endpoint fromString(String s, uint16_t defaultPort);

	bool operator ==(const Endpoint &e) const {
		return e.address == this->address && e.port == this->port;
	}
};

} // namespace ipv6
} // namespace coco
