#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12000
#define BUFFER_SIZE 1024
#define PING_COUNT 10

int main()
{
	int sockfd;
	sockaddr_in server_addr{};
	char buffer[BUFFER_SIZE];
	char message[BUFFER_SIZE];

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Ошибка создания сокета");
		return 1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
	{
		perror("Неверный адрес или адрес не поддерживается");
		return 1;
	}

	timeval tv_timeout{};
	tv_timeout.tv_sec = 1;
	tv_timeout.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) < 0)
	{
		perror("Ошибка установки тайм-аута");
		close(sockfd);
		return 1;
	}

	std::cout << "Запуск UDP Ping клиента на " << SERVER_IP << ":" << PORT << std::endl;

	for (int i = 1; i <= PING_COUNT; ++i)
	{
		timeval start_time{}, end_time{}, current_time{};

		gettimeofday(&current_time, nullptr);
		const long timestamp = current_time.tv_sec;

		sprintf(message, "Ping %d %ld", i, timestamp);

		gettimeofday(&start_time, nullptr);

		sendto(sockfd, message, strlen(message), MSG_CONFIRM,
			reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr));

		socklen_t len = sizeof(server_addr);
		const int n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL,
			reinterpret_cast<struct sockaddr*>(&server_addr), &len);

		if (n > 0)
		{
			gettimeofday(&end_time, nullptr);
			buffer[n] = '\0';

			const double rtt = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;

			printf("Ответ от сервера: %s, RTT = %.3f сек\n", buffer, rtt);
		}
		else
		{
			printf("Request timed out\n");
		}

		usleep(100000);
	}

	close(sockfd);
	return 0;
}