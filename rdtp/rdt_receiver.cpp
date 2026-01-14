#include "rdt_common.h"
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

using namespace std;

bool DEBUG = false;

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <port> <received_file.txt> [-d]" << endl;
		return 1;
	}

	int port = atoi(argv[1]);
	const char* filename = argv[2];
	if (argc > 3 && strcmp(argv[3], "-d") == 0)
		DEBUG = true;

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("Socket creation failed");
		return 1;
	}

	sockaddr_in servaddr = {}, cliaddr{};
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	if (bind(sockfd, reinterpret_cast<const sockaddr*>(&servaddr), sizeof(servaddr)) < 0)
	{
		perror("Bind failed");
		return 1;
	}

	ofstream outfile(filename, ios::binary);
	if (!outfile)
	{
		cerr << "Cannot open file for writing" << endl;
		return 1;
	}

	uint32_t expected_seq = 0;
	cout << "Receiver listening on port " << port << "..." << endl;

	while (true)
	{
		Packet pkt{};
		socklen_t len = sizeof(cliaddr);
		int n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, reinterpret_cast<struct sockaddr*>(&cliaddr), &len);

		if (n > 0)
		{
			if (pkt.flags == FLAG_FIN)
			{
				log_debug(DEBUG, "[RECV] FIN packet. Sending ACK and closing.");
				Packet ack{};
				ack.flags = FLAG_ACK;
				ack.ack_num = pkt.seq_num;
				sendto(sockfd, &ack, sizeof(ack), 0, reinterpret_cast<sockaddr*>(&cliaddr), len);
				break;
			}

			if (pkt.flags == FLAG_DATA)
			{
				if (pkt.seq_num == expected_seq)
				{
					log_debug(DEBUG, "[RECV] Packet SEQ %u accepted", pkt.seq_num);

					outfile.write(pkt.data, pkt.data_size);

					Packet ack{};
					ack.flags = FLAG_ACK;
					ack.ack_num = expected_seq;
					sendto(sockfd, &ack, sizeof(ack), 0, reinterpret_cast<sockaddr*>(&cliaddr), len);
					log_debug(DEBUG, "[SEND] ACK %u sent", expected_seq);

					expected_seq++;
				}
				else
				{
					log_debug(DEBUG, "[RECV] Packet SEQ %u unexpected (expected %u). Discarded.", pkt.seq_num, expected_seq);

					if (expected_seq > 0)
					{
						Packet ack{};
						ack.flags = FLAG_ACK;
						ack.ack_num = expected_seq - 1;
						sendto(sockfd, &ack, sizeof(ack), 0, reinterpret_cast<sockaddr*>(&cliaddr), len);
						log_debug(DEBUG, "[SEND] Duplicate ACK %u sent", expected_seq - 1);
					}
				}
			}
		}
	}

	outfile.close();
	close(sockfd);
	cout << "File received successfully." << endl;
	return 0;
}