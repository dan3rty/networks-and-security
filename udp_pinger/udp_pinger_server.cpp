#include <cstring>
#include <ctime>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 12000
#define BUFFER_SIZE 1024

int main()
{
	int sockfd;
	sockaddr_in server_addr{}, client_addr{};
	char buffer[BUFFER_SIZE];
	socklen_t addr_len = sizeof(client_addr);

	srand(time(nullptr));

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Ошибка создания сокета");
		return 1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if (bind(sockfd, reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr)) < 0)
	{
		perror("Ошибка привязки (bind)");
		close(sockfd);
		return 1;
	}

	std::cout << "UDP Pinger Server запущен на порту " << PORT << "..." << std::endl;

	while (true)
	{
		const int n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL,
			reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
		buffer[n] = '\0';

		const int random_num = rand() % 10;
		if (random_num < 3)
		{
			std::cout << "Пакет потерян (симуляция): " << buffer << std::endl;
			continue;
		}

		std::cout << "Получено и отправлено обратно: " << buffer << std::endl;

		sendto(sockfd, buffer, n, MSG_CONFIRM,
			reinterpret_cast<const sockaddr*>(&client_addr), addr_len);
	}

	return 0;
}