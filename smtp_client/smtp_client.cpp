#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

constexpr int SERVER_PORT = 2525;
auto SERVER_IP = "127.0.0.1";

constexpr int BUFFER_SIZE = 4096;

void send_command(const int socket, const std::string& cmd)
{
	std::cout << "C: " << cmd;
	if (write(socket, cmd.c_str(), cmd.length()) < 0)
	{
		perror("Sending error");
		exit(1);
	}
}

void read_response(const int socket)
{
	char buffer[BUFFER_SIZE] = {};

	const int bytes_read = read(socket, buffer, BUFFER_SIZE - 1);
	if (bytes_read < 0)
	{
		perror("Reading error");
		exit(1);
	}
	if (bytes_read == 0)
	{
		std::cout << "Server closed connection" << std::endl;
		exit(1);
	}

	std::cout << "S: " << buffer;
}

int main()
{
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("Error creating socket");
		return 1;
	}

	sockaddr_in serv_addr = {};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);

	if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
	{
		perror("Error in IP");
		return 1;
	}

	std::cout << "Подключение к " << SERVER_IP << ":" << SERVER_PORT << "..." << std::endl;
	if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0)
	{
		perror("Ошибка подключения");
		return 1;
	}

	read_response(sockfd);

	send_command(sockfd, "HELO myclient.local\r\n");
	read_response(sockfd); // Expected 250 OK

	send_command(sockfd, "MAIL FROM: <student@university.test>\r\n");
	read_response(sockfd); // Expected 250 OK

	send_command(sockfd, "RCPT TO: <professor@university.test>\r\n");
	read_response(sockfd); // Expected 250 OK

	send_command(sockfd, "DATA\r\n");
	read_response(sockfd); // Expected 354 Start mail input

	const std::string email_body = "Subject: Lab Work 7 Report\r\n"
							 "From: student@university.test\r\n"
							 "To: professor@university.test\r\n"
							 "\r\n"
							 "Hello!\r\n"
							 "This is a test email sent from my C++ SMTP Client.\r\n"
							 "It uses basic socket system calls.\r\n"
							 "Hope you receive it well.\r\n";

	send_command(sockfd, email_body);

	send_command(sockfd, "\r\n.\r\n");
	read_response(sockfd); // Expected 250 OK

	send_command(sockfd, "QUIT\r\n");
	read_response(sockfd); // Expected 221 Bye

	close(sockfd);

	return 0;
}