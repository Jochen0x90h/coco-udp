#include <coco/debug.hpp>
#include "UdpSocketTest.hpp"
#ifdef NATIVE
#include <string>
#include <iostream>
#endif


/*
    UdpSocketTest: Either start one instance without arguments that sends to itself on port 1337
    or start two instances, one with arguments 1337 1338 and one with arguments 1338 1337.
    The sender toggles the red LED, the receiver toggles the green LED.
*/

Coroutine sender(Loop &loop, Buffer &buffer) {
    const uint8_t data[] = {1, 2, 3, 4};
    while (true) {
        co_await buffer.writeArray(data);
        debug::toggleRed();
#ifdef NATIVE
        std::cout << "Sent " << buffer.size() << " to port " << buffer.header<ipv6::Endpoint>().port << std::endl;
#endif
        co_await loop.sleep(1s);
    }
}

Coroutine receiver(Loop &loop, Buffer &buffer) {
    while (true) {
        co_await buffer.read();
        debug::toggleGreen();
#ifdef NATIVE
        std::cout << "Received " << buffer.size() << " from port " << buffer.header<ipv6::Endpoint>().port << std::endl;
#endif
    }
}

// it is possible to start two instances with different ports
uint16_t localPort = 1337;
uint16_t remotePort = 1337;

#ifdef NATIVE
int main(int argc, char const **argv) {
    if (argc >= 3) {
        localPort = std::stoi(argv[1]);
        remotePort = std::stoi(argv[2]);
    }
    //std::cout << "Local port " << localPort << std::endl;
    //std::cout << "Remote port " << remotePort << std::endl;
#else
int main() {
#endif
    drivers.socket.open(localPort);

    ipv6::Endpoint destination = {ipv6::Address::fromString("::1"), remotePort};
    drivers.buffer1.setHeader(destination);
    sender(drivers.loop, drivers.buffer1);

    receiver(drivers.loop, drivers.buffer2);

    drivers.loop.run();
}
