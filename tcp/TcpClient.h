#pragma once

#include <cstdint>
#include <string>

class TcpClient
{
public:
	TcpClient(std::string serverIp, uint16_t port, std::string name);

	void run() const;

private:
	int connectToServer() const;
	static int32_t getUserInput();

	std::string m_serverIp;
	uint16_t m_port;
	std::string m_clientName;
};