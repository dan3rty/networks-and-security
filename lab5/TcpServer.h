#pragma once

#include <cstdint>
#include <string>

class TcpServer
{
public:
	TcpServer(uint16_t port, const std::string& name);
	~TcpServer();

	void run();

private:
	void setupSocket();
	void handleClient(int clientSocketFd);

	uint16_t m_port;
	std::string m_serverName;
	const int32_t m_serverNumber = 50;
	int m_listeningSocketFd;
	bool m_shouldStop;
};