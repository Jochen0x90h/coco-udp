#include <coco/String.hpp>
#include <coco/ipv6.hpp>
#define NOMINMAX
#include <Winsock2.h>
#include <ws2tcpip.h>


namespace coco {
namespace ipv6 {

Address Address::fromString(String str) {
	char buffer[64];
	int count = std::min(str.size(), int(std::size(buffer)) - 1);
	std::copy(str.begin(), str.begin() + count, buffer);
	buffer[count] = 0;

	in6_addr a;
	inet_pton(AF_INET6, buffer, &a);

	Address address;
	std::copy(a.s6_addr, a.s6_addr + 16, address.u8);
	return address;
}

} // namespace ipv6
} // namespace coco
