#include "UdpSocket_Win32.hpp"
#include <iostream>


namespace coco {

UdpSocket_Win32::UdpSocket_Win32(Loop_Win32 &loop)
	: loop(loop)
{
	// initialize winsock
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);
}

UdpSocket_Win32::~UdpSocket_Win32() {
	WSACleanup();
}

Device::State UdpSocket_Win32::state() {
	return this->stat;
}

Awaitable<Device::State> UdpSocket_Win32::untilState(State state) {
	if (this->stat == state)
		return {};
	return {this->stateTasks, state};
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
	this->stat = State::READY;

	// enable buffers
	for (auto &buffer : this->buffers) {
		buffer.setReady(0);
	}

	// resume all coroutines waiting for ready state
	this->stateTasks.resumeAll([](State state) {
		return state == State::READY;
	});

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

void UdpSocket_Win32::close() {
	// close socket
	closesocket(this->socket);
	this->socket = INVALID_SOCKET;

	// set state
	this->stat = State::DISABLED;

	// set state of buffers to disabled
	for (auto &buffer : this->buffers) {
		buffer.setDisabled();
	}

	// resume all coroutines waiting for disabled state
	this->stateTasks.resumeAll([](State state) {
		return state == State::DISABLED;
	});
}

void UdpSocket_Win32::handle(OVERLAPPED *overlapped) {
	for (auto &buffer : this->buffers) {
		if (overlapped == &buffer.overlapped) {
			buffer.handle(overlapped);
			break;
		}
	}
}


// Buffer

UdpSocket_Win32::Buffer::Buffer(UdpSocket_Win32 &socket, int size)
	: BufferImpl(new uint8_t[size], size, socket.stat)
	, socket(socket)
	, endpoint{.sin6_family = AF_INET6}
{
	socket.buffers.add(*this);
}

UdpSocket_Win32::Buffer::~Buffer() {
	delete [] this->dat;
}

bool UdpSocket_Win32::Buffer::startInternal(int size, Op op) {
	if (this->stat != State::READY) {
		assert(false);
		return false;
	}

	// check if READ or WRITE flag is set
	assert((op & Op::READ_WRITE) != 0);

	this->op = op;

	// initialize overlapped
	memset(&this->overlapped, 0, sizeof(OVERLAPPED));

	WSABUF buffer;
	buffer.buf = (CHAR*)this->dat;
	buffer.len = size;
	int result;
	if ((op & Op::READ) != 0) {
		// receive
		//buffer.len = this->p.capacity;
		DWORD flags = 0;
		this->endpointSize = sizeof(this->endpoint);
		result = WSARecvFrom(this->socket.socket, &buffer, 1, nullptr, &flags, (sockaddr *)&this->endpoint, &this->endpointSize, &this->overlapped, nullptr);
	} else {
		// send
		//buffer.len = this->p.size;
		result = WSASendTo(this->socket.socket, &buffer, 1, nullptr, 0, (sockaddr *)&this->endpoint, sizeof(this->endpoint), &this->overlapped, nullptr);
	}

	if (result != 0) {
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			// "real" error (e.g. if nobody listens on the other end we get WSAECONNRESET = 10054)
			setReady(0);
			return false;
		}
	}

	// set state
	setBusy();

	return true;
}

void UdpSocket_Win32::Buffer::cancel() {
	if (this->stat != State::BUSY)
		return;

	auto result = CancelIoEx((HANDLE)this->socket.socket, &this->overlapped);
	if (!result) {
		auto e = WSAGetLastError();
		std::cerr << "cancel error " << e << std::endl;
	}

	// set state and resume all coroutines waiting for cancelled state
	//setCancelled();
}

bool UdpSocket_Win32::Buffer::setHeader(const uint8_t *data, int size) {
	if (size < sizeof(ipv6::Endpoint)) {
		assert(false);
		return false;
	}
	auto &endpoint = *reinterpret_cast<const ipv6::Endpoint *>(data);
	std::copy(endpoint.address.u8, endpoint.address.u8 + 16, this->endpoint.sin6_addr.s6_addr);
	this->endpoint.sin6_port = htons(endpoint.port);
	return true;
}

bool UdpSocket_Win32::Buffer::getHeader(uint8_t *data, int size) {
	if (size < sizeof(ipv6::Endpoint)) {
		assert(false);
		return false;
	}
	auto &endpoint = *reinterpret_cast<ipv6::Endpoint *>(data);
	std::copy(this->endpoint.sin6_addr.s6_addr, this->endpoint.sin6_addr.s6_addr + 16, endpoint.address.u8);
	endpoint.port = ntohs(this->endpoint.sin6_port);
	return true;
}

void UdpSocket_Win32::Buffer::handle(OVERLAPPED *overlapped) {
	DWORD transferred;
	DWORD flags;
	auto result = WSAGetOverlappedResult(this->socket.socket, overlapped, &transferred, false, &flags);
	if (!result) {
		// "real" error or cancelled (ERROR_OPERATION_ABORTED): return zero size
		auto error = WSAGetLastError();
		transferred = 0;
	}

	// transfer finished
	setReady(transferred);
}

} // namespace coco
