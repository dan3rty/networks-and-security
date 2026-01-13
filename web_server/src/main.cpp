#include "WebServer.h"
#include <iostream>

constexpr uint16_t PORT = 8080;
const std::string DOCUMENT_ROOT = "./public";

int main()
{
	try
	{
		WebServer server(PORT, DOCUMENT_ROOT);
		server.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}