#include "UdpSocket_Win32.hpp"
#include <iostream>


namespace coco {

UdpSocket_Win32::UdpSocket_Win32(Loop_Win32 &loop)
	: UdpSocket(State::DISABLED)
	, loop(loop)
{
	// initialize winsock
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);
}

UdpSocket_Win32::~UdpSocket_Win32() {
	WSACleanup();
}

//StateTasks<const Device::State, Device::Events> &UdpSocket_Win32::getStateTasks() {
//	return makeConst(this->st);
//}

/*BufferDevice::State UdpSocket_Win32::state() {
	return this->stat;
}

Awaitable<Device::Condition> UdpSocket_Win32::until(Condition condition) {
	// check if IN_* condition is met
	if ((int(condition) >> int(this->stat)) & 1)
		return {}; // don't wait
	return {this->stateTasks, condition};
}*/

void UdpSocket_Win32::close() {
	// close socket
	closesocket(this->socket);
	this->socket = INVALID_SOCKET;

	// set state
	this->st.state = State::CLOSING;

	// disable buffers
	for (auto &buffer : this->buffers) {
		buffer.setDisabled();
	}

	// set state and resume all coroutines waiting for state change
	this->st.set(State::DISABLED, Events::ENTER_CLOSING | Events::ENTER_DISABLED);
}

int UdpSocket_Win32::getBufferCount() {
	return this->buffers.count();
}

UdpSocket_Win32::Buffer &UdpSocket_Win32::getBuffer(int index) {
	return this->buffers.get(index);
}

bool UdpSocket_Win32::open(uint16_t localPort) {
	// create socket
	SOCKET socket = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (socket == INVALID_SOCKET) {
		//int e = WSAGetLastError();
		return false;
	}

	// reuse address/port
	// https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
	int reuse = 1;
	if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) != 0) {
		//int e = WSAGetLastError();
		closesocket(socket);
		return false;
	}

	// bind to local port
	sockaddr_in6 ep = {.sin6_family = AF_INET6, .sin6_port = htons(localPort)};
	if (bind(socket, (struct sockaddr*)&ep, sizeof(ep)) != 0) {
		//int e = WSAGetLastError();
		closesocket(socket);
		return false;
	}

	// add socket to completion port of event loop
	Loop_Win32::CompletionHandler *handler = this;
	if (CreateIoCompletionPort(
		(HANDLE)socket,
		this->loop.port,
		ULONG_PTR(handler),
		0) == nullptr)
	{
		//int e = WSAGetLastError();
		closesocket(socket);
		return false;
	}
	this->socket = socket;

	// set state
	this->st.state = State::OPENING;

	// enable buffers
	for (auto &buffer : this->buffers) {
		buffer.setReady(0);
	}

	// set state and resume all coroutines waiting for state change
	this->st.set(State::READY, Events::ENTER_OPENING | Events::ENTER_READY);

	return true;
}

bool UdpSocket_Win32::join(ipv6::Address const &multicastGroup) {
	// join multicast group
	struct ipv6_mreq group;
	std::copy(multicastGroup.u8, multicastGroup.u8 + 16, group.ipv6mr_multiaddr.s6_addr);
	group.ipv6mr_interface = 0;
	int r = setsockopt(this->socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&group, sizeof(group));
	if (r < 0) {
		int e = WSAGetLastError();
		return false;
	}
	return true;
}

void UdpSocket_Win32::handle(OVERLAPPED *overlapped) {
	for (auto &buffer : this->transfers) {
		if (overlapped == &buffer.overlapped) {
			buffer.handle(overlapped);
			break;
		}
	}
}


// Buffer

UdpSocket_Win32::Buffer::Buffer(UdpSocket_Win32 &device, int size)
	: coco::Buffer(new uint8_t[sizeof(ipv6::Endpoint) + size], sizeof(ipv6::Endpoint), size, device.st.state)
	, device(device)
	, endpoint{.sin6_family = AF_INET6}
{
	device.buffers.add(*this);
}

UdpSocket_Win32::Buffer::~Buffer() {
	delete [] this->p.data;
}

bool UdpSocket_Win32::Buffer::start(Op op) {
	if (this->st.state != State::READY) {
		assert(this->st.state != State::BUSY);
		return false;
	}

	// check if READ or WRITE flag is set
	assert((op & Op::READ_WRITE) != 0);
	this->op = op;

	if ((op & Op::WRITE) == 0) {
		// receive: set header size
		this->p.headerSize = sizeof(ipv6::Endpoint);
	} else {
		// send: check if header size is ok
		if (this->p.headerSize != sizeof(ipv6::Endpoint)) {
			assert(false);
			return false;
		}
	}

	// add to list of pending transfers
	this->device.transfers.add(*this);

	// start if device is ready
	if (this->device.st.state == Device::State::READY)
		start();

	// set state
	setBusy();

	return true;
}

bool UdpSocket_Win32::Buffer::cancel() {
	if (this->st.state != State::BUSY)
		return false;

	auto result = CancelIoEx((HANDLE)this->device.socket, &this->overlapped);
	if (!result) {
		auto e = WSAGetLastError();
		std::cerr << "cancel error " << e << std::endl;
	}

	return true;
}

void UdpSocket_Win32::Buffer::start() {
	// initialize overlapped
	memset(&this->overlapped, 0, sizeof(OVERLAPPED));

	// get header
	auto header = this->p.data;
	int headerSize = sizeof(ipv6::Endpoint);

	int result;
	if ((op & Op::WRITE) == 0) {
		// receive
		WSABUF buffer{this->p.capacity - headerSize, (CHAR*)(header + headerSize)};
		DWORD flags = 0;
		this->endpointSize = sizeof(this->endpoint);
		result = WSARecvFrom(this->device.socket, &buffer, 1, nullptr, &flags, (sockaddr *)&this->endpoint, &this->endpointSize, &this->overlapped, nullptr);
	} else {
		// send

		// copy address
		auto &endpoint = *reinterpret_cast<const ipv6::Endpoint *>(header);
		std::copy(endpoint.address.u8, endpoint.address.u8 + 16, this->endpoint.sin6_addr.s6_addr);
		this->endpoint.sin6_port = htons(endpoint.port);

		// start send
		WSABUF buffer{this->p.size - headerSize, (CHAR*)(header + headerSize)};
		result = WSASendTo(this->device.socket, &buffer, 1, nullptr, 0, (sockaddr *)&this->endpoint, sizeof(this->endpoint), &this->overlapped, nullptr);
	}

	if (result != 0) {
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			// "real" error (e.g. if nobody listens on the other end we get WSAECONNRESET = 10054)
			setReady(0);
			//return false;
		}
	}
}

void UdpSocket_Win32::Buffer::handle(OVERLAPPED *overlapped) {
	DWORD transferred;
	DWORD flags;
	auto result = WSAGetOverlappedResult(this->device.socket, overlapped, &transferred, false, &flags);
	if (result) {
		if ((this->op & Op::WRITE) == 0) {
			// copy address
			auto &endpoint = *reinterpret_cast<ipv6::Endpoint *>(this->p.data);
			std::copy(this->endpoint.sin6_addr.s6_addr, this->endpoint.sin6_addr.s6_addr + 16, endpoint.address.u8);
			endpoint.port = ntohs(this->endpoint.sin6_port);
		}
	} else {
		// "real" error or cancelled (ERROR_OPERATION_ABORTED): return zero size
		auto error = WSAGetLastError();
		transferred = 0;
		this->p.headerSize = 0;
	}

	// remove from list of active transfers
	remove2();

	// transfer finished
	setReady(transferred);
}

} // namespace coco
