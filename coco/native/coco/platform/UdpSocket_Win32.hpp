#pragma once

#include <coco/UdpSocket.hpp>
#include <coco/IntrusiveList.hpp>
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

	// Device methods
    //StateTasks<const State, Events> &getStateTasks() override;
	void close() override;

	// BufferDevice methods
	int getBufferCount() override;
	Buffer &getBuffer(int index) override;

	// UdpSocket methods
	bool open(uint16_t localPort) override;
	bool join(ipv6::Address const &multicastGroup) override;

	/**
	 * Buffer for transferring data to/from a file
	 */
	class Buffer : public coco::Buffer, public IntrusiveListNode, public IntrusiveListNode2 {
		friend class UdpSocket_Win32;
	public:
		Buffer(UdpSocket_Win32 &device, int size);
		~Buffer() override;

		// Buffer methods
		bool start(Op op) override;
		bool cancel() override;

	protected:
		void start();
		void handle(OVERLAPPED *overlapped);

		UdpSocket_Win32 &device;
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

	// device state
	//StateTasks<State, Events> st = State::DISABLED;

	// list of buffers
	IntrusiveList<Buffer> buffers;

	// pending transfers
	IntrusiveList2<Buffer> transfers;
};

} // namespace coco
