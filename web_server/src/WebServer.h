#pragma once

#include <cstdint>
#include <string>

class WebServer
{
public:
	WebServer(uint16_t port, std::string  documentRoot);

	~WebServer();

	void run() const;

private:
	void setupServerSocket();
	void handleClientConnection(int clientSocket) const;
	static std::string parseRequestPath(const char* requestBuffer);
	static std::string getContentType(const std::string& path);
	static void sendResponse(int clientSocket, const std::string& httpResponse);
	static std::string buildHttpResponse(const std::string& statusCode, const std::string& statusText, const std::string& contentType, const std::string& body);

	uint16_t m_port;
	std::string m_documentRoot;
	int m_serverSocket;
};