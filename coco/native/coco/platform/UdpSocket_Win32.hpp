#pragma once

#include <coco/UdpSocket.hpp>
#include <coco/BufferImpl.hpp>
#include <coco/LinkedList.hpp>
#define NOMINMAX
#include <winsock2.h> // see https://learn.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
#include <ws2tcpip.h>
#include <coco/platform/Loop_native.hpp> // includes Windows.h


namespace coco {

class UdpSocket_Win32 : public UdpSocket, public Loop_Win32::CompletionHandler {
public:
	/**
		Constructor
		@param loop event loop
	*/
	UdpSocket_Win32(Loop_Win32 &loop);

	~UdpSocket_Win32() override;

	class Buffer;

	State state() override;
	Awaitable<State> untilState(State state) override;
	int getBufferCount() override;
	Buffer &getBuffer(int index) override;

	bool open(uint16_t localPort) override;
	bool join(ipv6::Address const &multicastGroup) override;
	void close() override;

	/**
		Buffer for transferring data to/from a file
	*/
	class Buffer : public BufferImpl, public LinkedListNode {
		friend class UdpSocket_Win32;
	public:
		Buffer(UdpSocket_Win32 &socket, int size);
		~Buffer() override;

		bool setHeader(const uint8_t *data, int size);
		using BufferImpl::setHeader;
		bool getHeader(uint8_t *data, int size);
		using BufferImpl::getHeader;
		bool startInternal(int size, Op op) override;
		void cancel() override;

	protected:
		void handle(OVERLAPPED *overlapped);

		UdpSocket_Win32 &socket;
		sockaddr_in6 endpoint;
		INT endpointSize;
		OVERLAPPED overlapped;
		Op op;
	};

protected:
	void handle(OVERLAPPED *overlapped) override;

	Loop_Win32 &loop;

	// socket handle
	SOCKET socket = INVALID_SOCKET;

	// properties
	State stat = State::DISABLED;

	TaskList<State> stateTasks;

	LinkedList<Buffer> buffers;
};

} // namespace coco
