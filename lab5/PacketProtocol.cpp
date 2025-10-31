#include "PacketProtocol.h"
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

namespace PacketProtocol
{
static bool sendExactly(const int sockFd, const void* data, size_t length)
{
	auto ptr = static_cast<const char*>(data);
	while (length > 0)
	{
		const int bytesSent = send(sockFd, ptr, length, 0);
		if (bytesSent < 0)
		{
			perror("send failed");
			return false;
		}
		ptr += bytesSent;
		length -= bytesSent;
	}
	return true;
}

static bool receiveExactly(const int sockFd, void* data, size_t length)
{
	auto ptr = static_cast<char*>(data);
	while (length > 0)
	{
		const int bytesReceived = recv(sockFd, ptr, length, 0);
		if (bytesReceived < 0)
		{
			perror("recv failed");
			return false;
		}
		if (bytesReceived == 0)
		{
			std::cerr << "Connection closed by peer" << std::endl;
			return false;
		}
		ptr += bytesReceived;
		length -= bytesReceived;
	}
	return true;
}

bool sendPacket(const int sockFd, const DataPacket& packet)
{
	const uint32_t nameLen = packet.senderName.length();
	const uint32_t nameLenNet = htonl(nameLen);
	if (!sendExactly(sockFd, &nameLenNet, sizeof(nameLenNet)))
	{
		return false;
	}

	if (!sendExactly(sockFd, packet.senderName.c_str(), nameLen))
	{
		return false;
	}

	const int32_t numberNet = htonl(packet.number);
	if (!sendExactly(sockFd, &numberNet, sizeof(numberNet)))
	{
		return false;
	}

	return true;
}

bool receivePacket(const int sockFd, DataPacket& outPacket)
{
	uint32_t nameLenNet;
	if (!receiveExactly(sockFd, &nameLenNet, sizeof(nameLenNet)))
	{
		return false;
	}
	const uint32_t nameLen = ntohl(nameLenNet);

	std::vector<char> nameBuffer(nameLen);
	if (!receiveExactly(sockFd, nameBuffer.data(), nameLen))
	{
		return false;
	}
	outPacket.senderName.assign(nameBuffer.data(), nameLen);

	int32_t numberNet;
	if (!receiveExactly(sockFd, &numberNet, sizeof(numberNet)))
	{
		return false;
	}
	outPacket.number = ntohl(numberNet);

	return true;
}
} // namespace PacketProtocol