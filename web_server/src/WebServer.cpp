#include "WebServer.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

WebServer::WebServer(const uint16_t port, std::string documentRoot)
	: m_port(port)
	, m_documentRoot(std::move(documentRoot))
	, m_serverSocket(-1)
{
	std::cout << "Initializing server..." << std::endl;
	setupServerSocket();
}

WebServer::~WebServer()
{
	if (m_serverSocket != -1)
	{
		std::cout << "Closing server socket." << std::endl;
		close(m_serverSocket);
	}
}

void WebServer::setupServerSocket()
{
	m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_serverSocket < 0)
	{
		throw std::runtime_error("Failed to create socket");
	}
	std::cout << "Socket created successfully. FD: " << m_serverSocket << std::endl;

	sockaddr_in serverAddress = {};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(m_port);

	if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
	{
		throw std::runtime_error("Failed to bind socket to port " + std::to_string(m_port));
	}
	std::cout << "Socket successfully bound to port " << m_port << std::endl;

	if (listen(m_serverSocket, 10) < 0)
	{
		throw std::runtime_error("Failed to listen on socket");
	}
	std::cout << "Server is listening..." << std::endl;
}

void WebServer::run() const
{
	while (true)
	{
		std::cout << "\nWaiting for a new connection..." << std::endl;

		const int clientSocket = accept(m_serverSocket, nullptr, nullptr);
		if (clientSocket < 0)
		{
			std::cerr << "Failed to accept connection" << std::endl;
			continue;
		}

		std::cout << "Connection accepted. Client FD: " << clientSocket << std::endl;
		handleClientConnection(clientSocket);

		close(clientSocket);
		std::cout << "Client connection closed. Client FD: " << clientSocket << std::endl;
	}
}

void WebServer::handleClientConnection(int clientSocket) const
{
	char buffer[4096] = {};
	auto bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
	if (bytesRead < 0)
	{
		std::cerr << "Failed to read from client socket" << std::endl;
		return;
	}
	std::cout << "--- Client Request ---\n"
			  << buffer << "----------------------" << std::endl;

	std::string filePath = parseRequestPath(buffer);

	if (filePath.empty())
	{
		std::cerr << "Invalid or unsupported request" << std::endl;
		return;
	}

	if (filePath == "/")
	{
		filePath = "/index.html";
	}

	std::string fullPath = m_documentRoot + filePath;

	std::ifstream file(fullPath);
	if (file.is_open())
	{
		std::stringstream fileStream;
		fileStream << file.rdbuf();
		std::string fileContent = fileStream.str();
		file.close();

		std::string contentType = getContentType(fullPath);
		std::string response = buildHttpResponse("200", "OK", contentType, fileContent);
		sendResponse(clientSocket, response);
		std::cout << "Sent 200 OK response for: " << fullPath << std::endl;
	}
	else
	{
		std::string body = "File Not Found";
		std::string response = buildHttpResponse("404", "Not Found", "text/plain", body);
		sendResponse(clientSocket, response);
		std::cout << "Sent 404 Not Found response for: " << fullPath << std::endl;
	}
}

std::string WebServer::parseRequestPath(const char* requestBuffer)
{
	std::string request(requestBuffer);
	if (request.rfind("GET ", 0) != 0)
	{
		return "";
	}

	const size_t pathEnd = request.find(' ', 4);
	if (pathEnd == std::string::npos)
	{
		return "";
	}

	return request.substr(4, pathEnd - 4);
}

std::string WebServer::getContentType(const std::string& path)
{
	const size_t dotPos = path.rfind('.');
	if (dotPos != std::string::npos)
	{
		const std::string extension = path.substr(dotPos);
		if (extension == ".html")
		{
			return "text/html";
		}
		if (extension == ".css")
		{
			return "text/css";
		}
		if (extension == ".js")
		{
			return "application/javascript";
		}
		if (extension == ".jpg" || extension == ".jpeg")
		{
			return "image/jpeg";
		}
		if (extension == ".png")
		{
			return "image/png";
		}
	}
	return "application/octet-stream";
}

void WebServer::sendResponse(const int clientSocket, const std::string& httpResponse)
{
	write(clientSocket, httpResponse.c_str(), httpResponse.length());
}

std::string WebServer::buildHttpResponse(const std::string& statusCode, const std::string& statusText, const std::string& contentType, const std::string& body)
{
	std::stringstream response;
	response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "\r\n";
	response << body;
	return response.str();
}