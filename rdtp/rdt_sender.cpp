#include "rdt_common.h"
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

#include <arpa/inet.h>
#include <cstring>

using namespace std;

bool DEBUG = false;
int LOSS_RATE = 0;

void udt_send(const int sockfd, const Packet& pkt, sockaddr_in& addr)
{
	if (rand() % 100 < LOSS_RATE)
	{
		log_debug(DEBUG, "[SIM] Packet SEQ %u dropped internally", pkt.seq_num);
		return;
	}

	sendto(sockfd, &pkt, sizeof(pkt), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	if (pkt.flags == FLAG_DATA)
	{
		log_debug(DEBUG, "[SEND] Packet SEQ %u, Size %u sent", pkt.seq_num, pkt.data_size);
	}
	else if (pkt.flags == FLAG_FIN)
	{
		log_debug(DEBUG, "[SEND] FIN packet sent");
	}
}

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		cerr << "Usage: " << argv[0] << " <receiver_host> <receiver_port> <file.txt> [-d] [--loss %]" << endl;
		return 1;
	}

	const char* dest_ip = argv[1];
	int dest_port = atoi(argv[2]);
	const char* filename = argv[3];

	for (int i = 4; i < argc; ++i)
	{
		if (strcmp(argv[i], "-d") == 0)
			DEBUG = true;
		if (strcmp(argv[i], "--loss") == 0 && i + 1 < argc)
			LOSS_RATE = atoi(argv[++i]);
	}

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("Socket creation failed");
		return 1;
	}

	sockaddr_in servaddr = {};
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(dest_port);
	inet_pton(AF_INET, dest_ip, &servaddr.sin_addr);

	ifstream file(filename, ios::binary);
	if (!file)
	{
		cerr << "Cannot open file: " << filename << endl;
		return 1;
	}
	vector file_buffer((istreambuf_iterator(file)), istreambuf_iterator<char>());
	file.close();

	size_t total_size = file_buffer.size();
	size_t total_packets = (total_size + MSS - 1) / MSS;
	if (total_size == 0)
		total_packets = 0;

	cout << "Starting transfer: " << total_size << " bytes (" << total_packets << " packets)." << endl;

	uint32_t base = 0;
	uint32_t next_seq_num = 0;
	int window_size = WINDOW_SIZE;

	timeval timeout_tv;

	srand(time(nullptr));

	while (base < total_packets)
	{
		while (next_seq_num < base + window_size && next_seq_num < total_packets)
		{
			Packet pkt{};
			pkt.seq_num = next_seq_num;
			pkt.ack_num = 0;
			pkt.flags = FLAG_DATA;

			size_t offset = next_seq_num * MSS;
			size_t chunk_size = min(static_cast<size_t>(MSS), total_size - offset);
			pkt.data_size = chunk_size;
			memcpy(pkt.data, &file_buffer[offset], chunk_size);

			udt_send(sockfd, pkt, servaddr);
			next_seq_num++;
		}

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		timeout_tv.tv_sec = TIMEOUT_SEC;
		timeout_tv.tv_usec = TIMEOUT_USEC;

		int activity = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout_tv);

		if (activity < 0)
		{
			perror("Select error");
			break;
		}
		if (activity == 0)
		{
			log_debug(DEBUG, "[TIMEOUT] Retransmitting window from SEQ %u to %u", base, next_seq_num - 1);
			for (uint32_t i = base; i < next_seq_num; ++i)
			{
				Packet pkt{};
				pkt.seq_num = i;
				pkt.flags = FLAG_DATA;
				size_t offset = i * MSS;
				size_t chunk_size = min(static_cast<size_t>(MSS), total_size - offset);
				pkt.data_size = chunk_size;
				memcpy(pkt.data, &file_buffer[offset], chunk_size);

				udt_send(sockfd, pkt, servaddr);
			}
		}
		else
		{
			Packet ack_pkt{};
			sockaddr_in from_addr{};
			socklen_t addr_len = sizeof(from_addr);
			recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, reinterpret_cast<sockaddr*>(&from_addr), &addr_len);

			if (ack_pkt.flags == FLAG_ACK)
			{
				log_debug(DEBUG, "[RECV] ACK for SEQ %u", ack_pkt.ack_num);
				if (ack_pkt.ack_num >= base)
				{
					base = ack_pkt.ack_num + 1;
				}
			}
		}

		if (!DEBUG)
		{
			printf("\rProgress: %u / %lu packets", base, total_packets);
			fflush(stdout);
		}
	}

	Packet fin_pkt{};
	fin_pkt.seq_num = next_seq_num;
	fin_pkt.flags = FLAG_FIN;
	fin_pkt.data_size = 0;

	bool fin_acked = false;
	int tries = 0;
	while (!fin_acked && tries < 10)
	{
		udt_send(sockfd, fin_pkt, servaddr);

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		timeout_tv.tv_sec = TIMEOUT_SEC;
		timeout_tv.tv_usec = TIMEOUT_USEC;

		int res = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout_tv);
		if (res > 0)
		{
			Packet ack_pkt{};
			socklen_t len = sizeof(servaddr);
			recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, reinterpret_cast<struct sockaddr*>(&servaddr), &len);
			if (ack_pkt.flags == FLAG_ACK)
				fin_acked = true;
		}
		tries++;
	}

	cout << "\nTransfer complete." << endl;
	close(sockfd);
	return 0;
}