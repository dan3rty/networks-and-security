#include "TcpClient.h"
#include "PacketProtocol.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <limits>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

TcpClient::TcpClient(const std::string& serverIp, const uint16_t port, const std::string& name)
	: m_serverIp(serverIp)
	, m_port(port)
	, m_clientName(name)
{
	std::cout << "Client '" << m_clientName << "' is being created." << std::endl;
}

int32_t TcpClient::getUserInput()
{
	int32_t number;
	while (true)
	{
		std::cout << "Enter an integer from 1 to 100 (or any other to stop the server): ";
		std::cin >> number;
		if (std::cin.good())
		{
			return number;
		}
		std::cout << "Invalid input. Please enter an integer." << std::endl;
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
}

int TcpClient::connectToServer() const
{
	const int sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd < 0)
	{
		throw std::runtime_error("Failed to create socket");
	}
	std::cout << "Socket created successfully." << std::endl;

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(m_port);
	if (inet_pton(AF_INET, m_serverIp.c_str(), &serverAddr.sin_addr) <= 0)
	{
		close(sockFd);
		throw std::runtime_error("Invalid server IP address");
	}

	std::cout << "Connecting to server at " << m_serverIp << ":" << m_port << "..." << std::endl;
	if (connect(sockFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
	{
		close(sockFd);
		throw std::runtime_error("Connection to server failed");
	}

	std::cout << "Successfully connected to the server." << std::endl;
	return sockFd;
}

void TcpClient::run() const
{
	const int32_t numberToSend = getUserInput();
	const int sockFd = connectToServer();

	std::cout << "Sending data to server..." << std::endl;
	DataPacket packetToSend = { m_clientName, numberToSend };
	if (!PacketProtocol::sendPacket(sockFd, packetToSend))
	{
		std::cerr << "Failed to send data." << std::endl;
		close(sockFd);
		return;
	}
	std::cout << "Data sent successfully." << std::endl;

	std::cout << "Waiting for response from server..." << std::endl;
	DataPacket receivedPacket;
	if (!PacketProtocol::receivePacket(sockFd, receivedPacket))
	{
		std::cerr << "Failed to receive response." << std::endl;
	}
	else
	{
		std::cout << "Response received." << std::endl;
		std::cout << "\n--- Client-side Info ---" << std::endl;
		std::cout << "My (Client) Name: " << m_clientName << std::endl;
		std::cout << "Server Name: " << receivedPacket.senderName << std::endl;
		std::cout << "My (Client) Number: " << numberToSend << std::endl;
		std::cout << "Server Number: " << receivedPacket.number << std::endl;
		std::cout << "Sum: " << numberToSend + receivedPacket.number << std::endl;
		std::cout << "------------------------" << std::endl;
	}

	std::cout << "Closing socket." << std::endl;
	close(sockFd);
}