#include "TcpServer.h"
#include "PacketProtocol.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

TcpServer::TcpServer(const uint16_t port, const std::string& name)
	: m_port(port)
	, m_serverName(name)
	, m_listeningSocketFd(-1)
	, m_shouldStop(false)
{
	std::cout << "Server '" << m_serverName << "' is being created." << std::endl;
}

TcpServer::~TcpServer()
{
	if (m_listeningSocketFd != -1)
	{
		std::cout << "Closing listening socket." << std::endl;
		close(m_listeningSocketFd);
	}
}

void TcpServer::setupSocket()
{
	m_listeningSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_listeningSocketFd < 0)
	{
		throw std::runtime_error("Failed to create socket");
	}
	std::cout << "Socket created successfully." << std::endl;

	constexpr int opt = 1;
	if (setsockopt(m_listeningSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
	}

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(m_port);

	if (bind(m_listeningSocketFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
	{
		throw std::runtime_error("Failed to bind socket to port " + std::to_string(m_port));
	}
	std::cout << "Socket bound to port " << m_port << std::endl;

	if (listen(m_listeningSocketFd, 5) < 0)
	{
		throw std::runtime_error("Failed to listen on socket");
	}
	std::cout << "Server is listening for incoming connections..." << std::endl;
}

void TcpServer::run()
{
	setupSocket();

	while (!m_shouldStop)
	{
		sockaddr_in clientAddr{};
		socklen_t clientLen = sizeof(clientAddr);
		const int clientSocketFd = accept(m_listeningSocketFd, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

		if (clientSocketFd < 0)
		{
			if (m_shouldStop)
			{
				std::cout << "Server is shutting down, stopping accept." << std::endl;
				break;
			}
			perror("accept failed");
			continue;
		}

		char clientIp[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
		std::cout << "\nAccepted new connection from " << clientIp << ":" << ntohs(clientAddr.sin_port) << std::endl;

		handleClient(clientSocketFd);
		close(clientSocketFd);
		std::cout << "Connection closed for " << clientIp << std::endl;
	}
	std::cout << "Server shutdown sequence complete." << std::endl;
}

void TcpServer::handleClient(const int clientSocketFd)
{
	DataPacket receivedPacket;

	if (!PacketProtocol::receivePacket(clientSocketFd, receivedPacket))
	{
		std::cerr << "Failed to receive packet from client." << std::endl;
		return;
	}

	std::cout << "--- Server-side Info ---" << std::endl;
	std::cout << "Client Name: " << receivedPacket.senderName << std::endl;
	std::cout << "Server Name: " << m_serverName << std::endl;
	std::cout << "Client Number: " << receivedPacket.number << std::endl;
	std::cout << "Server Number: " << m_serverNumber << std::endl;
	std::cout << "Sum: " << receivedPacket.number + m_serverNumber << std::endl;
	std::cout << "------------------------" << std::endl;

	std::cout << "Sending response to client..." << std::endl;
	DataPacket responsePacket = { m_serverName, m_serverNumber };
	if (!PacketProtocol::sendPacket(clientSocketFd, responsePacket))
	{
		std::cerr << "Failed to send response to client." << std::endl;
	}
	else
	{
		std::cout << "Response sent successfully." << std::endl;
	}

	if (receivedPacket.number < 1 || receivedPacket.number > 100)
	{
		std::cout << "Received shutdown signal (number " << receivedPacket.number << " is out of range [1, 100])." << std::endl;
		m_shouldStop = true;
	}
}