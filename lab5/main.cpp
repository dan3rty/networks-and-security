#include "TcpClient.h"
#include "TcpServer.h"
#include <iostream>
#include <string>

void printUsage()
{
	std::cerr << "Usage: ./app <mode>" << std::endl;
	std::cerr << "Modes:" << std::endl;
	std::cerr << "  server" << std::endl;
	std::cerr << "  client" << std::endl;
}

int main(const int argc, char* argv[])
{
	if (argc != 2)
	{
		printUsage();
		return 1;
	}

	const std::string mode = argv[1];
	constexpr uint16_t port = 5555;
	const std::string serverIp = "127.0.0.1";

	try
	{
		if (mode == "server")
		{
			TcpServer server(port, "Server of Daniil Dolgorukov");
			server.run();
		}
		else if (mode == "client")
		{
			const TcpClient client(serverIp, port, "Client of Daniil Dolgorukov");
			client.run();
		}
		else
		{
			printUsage();
			return EXIT_FAILURE;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}