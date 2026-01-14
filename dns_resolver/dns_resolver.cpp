#include <arpa/inet.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

constexpr int TYPE_A = 1;
constexpr int TYPE_NS = 2;
constexpr int TYPE_CNAME = 5;
constexpr int TYPE_AAAA = 28;
const std::string ROOT_SERVER = "198.41.0.4";

#pragma pack(push, 1)
struct DnsHeader
{
	uint16_t id;
	uint16_t flags;
	uint16_t q_count;
	uint16_t ans_count;
	uint16_t auth_count;
	uint16_t add_count;
};

struct DnsQuestion
{
	uint16_t qtype;
	uint16_t qclass;
};

struct RDataHeader
{
	uint16_t type;
	uint16_t _class;
	uint32_t ttl;
	uint16_t data_len;
};
#pragma pack(pop)

class DnsResolver
{
public:
	explicit DnsResolver(const bool debug_mode)
		: debug(debug_mode)
	{
		srand(time(nullptr));
	}

	std::string query(const std::string& server_ip, const std::string& domain, int type, bool& is_final)
	{
		unsigned char buffer[65536];
		log("Sending request to " + server_ip + " for " + domain);

		addrinfo hints = {}, *res;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;

		if (getaddrinfo(server_ip.c_str(), "53", &hints, &res) != 0)
		{
			log("Invalid IP address: " + server_ip);
			return "";
		}

		int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
		{
			log("Socket creation failed");
			freeaddrinfo(res);
			return "";
		}

		timeval tv = { 2, 0 };
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

		auto* dns = reinterpret_cast<DnsHeader*>(&buffer);
		dns->id = htons(rand() % 65535);
		dns->flags = 0;
		dns->q_count = htons(1);
		dns->ans_count = 0;
		dns->auth_count = 0;
		dns->add_count = 0;

		auto qname = &buffer[sizeof(DnsHeader)];
		encodeName(qname, domain);
		auto* qinfo = reinterpret_cast<DnsQuestion*>(&buffer[sizeof(DnsHeader) + (strlen(reinterpret_cast<const char*>(qname)) + 1)]);
		qinfo->qtype = htons(type);
		qinfo->qclass = htons(1);

		int packet_size = sizeof(DnsHeader) + (strlen(reinterpret_cast<const char*>(qname)) + 1) + sizeof(DnsQuestion);

		if (sendto(sockfd, buffer, packet_size, 0, res->ai_addr, res->ai_addrlen) < 0)
		{
			log("Sendto failed");
			close(sockfd);
			freeaddrinfo(res);
			return "";
		}
		freeaddrinfo(res);

		int n = recvfrom(sockfd, buffer, 65536, 0, nullptr, nullptr);
		close(sockfd);

		if (n < 0)
		{
			log("Timeout receiving data");
			return "";
		}

		auto* resp = reinterpret_cast<DnsHeader*>(buffer);
		unsigned char* cursor = &buffer[packet_size];

		if (resp->flags & htons(0x0200))
		{
			log("WARNING: TC Flag detected! Packet truncated.");
		}

		int rcode = ntohs(resp->flags) & 0x000F;
		if (rcode != 0)
		{
			if (rcode == 3)
				return "NXDOMAIN";
			return "";
		}

		int answers = ntohs(resp->ans_count);
		int authorities = ntohs(resp->auth_count);
		int additional = ntohs(resp->add_count);

		log("Received: Ans=" + std::to_string(answers) + ", Auth=" + std::to_string(authorities) + ", Add=" + std::to_string(additional));

		for (int i = 0; i < answers; i++)
		{
			int bytes_read = 0;
			decodeName(cursor, buffer, &bytes_read);
			cursor += bytes_read;
			auto* rdata = reinterpret_cast<RDataHeader*>(cursor);
			cursor += sizeof(RDataHeader);
			int len = ntohs(rdata->data_len);
			int rtype = ntohs(rdata->type);

			if (rtype == type)
			{
				is_final = true;
				char ip_str[INET6_ADDRSTRLEN];
				if (type == TYPE_A)
					inet_ntop(AF_INET, cursor, ip_str, INET_ADDRSTRLEN);
				else
					inet_ntop(AF_INET6, cursor, ip_str, INET6_ADDRSTRLEN);
				return std::string(ip_str);
			}
			if (rtype == TYPE_CNAME)
			{
				int clen;
				std::string cname = decodeName(cursor, buffer, &clen);
				is_final = true;
				return "CNAME:" + cname;
			}
			cursor += len;
		}

		for (int i = 0; i < authorities; i++)
		{
			int bytes_read = 0;
			decodeName(cursor, buffer, &bytes_read);
			cursor += bytes_read;
			auto* rdata = reinterpret_cast<RDataHeader*>(cursor);
			cursor += sizeof(RDataHeader) + ntohs(rdata->data_len);
		}

		std::string ip4_glue;
		std::string ip6_glue;
		std::string found_name;

		for (int i = 0; i < additional; i++)
		{
			int bytes_read = 0;
			std::string name = decodeName(cursor, buffer, &bytes_read);
			cursor += bytes_read;

			auto* rdata = reinterpret_cast<RDataHeader*>(cursor);
			cursor += sizeof(RDataHeader);
			int len = ntohs(rdata->data_len);
			int rtype = ntohs(rdata->type);

			if (rtype == TYPE_A)
			{
				char ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, cursor, ip, INET_ADDRSTRLEN);
				ip4_glue = std::string(ip);
				found_name = name;
			}
			else if (rtype == TYPE_AAAA)
			{
				char ip[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, cursor, ip, INET6_ADDRSTRLEN);
				ip6_glue = std::string(ip);
				if (found_name.empty())
					found_name = name;
			}
			cursor += len;
		}

		if (!ip4_glue.empty())
		{
			is_final = false;
			log("Found Glue Record (A) for " + found_name + ": " + ip4_glue);
			return ip4_glue;
		}
		if (!ip6_glue.empty())
		{
			is_final = false;
			log("Found Glue Record (AAAA) for " + found_name + ": " + ip6_glue);
			return ip6_glue;
		}

		return "";
	}

	void resolve(std::string domain, const int type)
	{
		std::string current_server = ROOT_SERVER;
		bool is_final = false;

		std::cout << "Resolving " << domain << "..." << std::endl;

		for (int i = 0; i < 20; i++)
		{
			std::string result = query(current_server, domain, type, is_final);

			if (result == "NXDOMAIN")
			{
				std::cout << "Error: Host not found (NXDOMAIN)" << std::endl;
				return;
			}
			if (result.empty())
			{
				std::cout << "Failed to resolve." << std::endl;
				std::cout << "Tip: Check if you have IPv6 connectivity if looking up AAAA records." << std::endl;
				return;
			}

			if (result.find("CNAME:") == 0)
			{
				domain = result.substr(6);
				current_server = ROOT_SERVER;
				is_final = false;
				std::cout << "Redirecting to CNAME: " << domain << std::endl;
				continue;
			}

			if (is_final)
			{
				std::cout << "---------------------------------" << std::endl;
				std::cout << "Final Result: " << result << std::endl;
				return;
			}
			current_server = result;
		}
		std::cout << "Too many hops." << std::endl;
	}

private:
	bool debug;

	void log(const std::string& msg) const
	{
		if (debug)
			std::cout << "[DEBUG] " << msg << std::endl;
	}

	static void encodeName(unsigned char* buffer, const std::string& host)
	{
		const std::string copy = host + ".";
		int start = 0;
		unsigned char* ptr = buffer;
		for (int i = 0; i < copy.length(); i++)
		{
			if (copy[i] == '.')
			{
				*ptr++ = i - start;
				for (; start < i; start++)
					*ptr++ = copy[start];
				start++;
			}
		}
		*ptr = 0;
	}

	static std::string decodeName(unsigned char* cursor, unsigned char* packet_start, int* bytes_read)
	{
		std::string name;
		*bytes_read = 1;
		unsigned char* local_cursor = cursor;
		bool jumped = false;

		while (*local_cursor != 0)
		{
			if (*local_cursor >= 192)
			{
				const unsigned int offset = (*local_cursor) * 256 + *(local_cursor + 1) - 49152;
				local_cursor = packet_start + offset;
				jumped = true;
			}
			else
			{
				name.append(reinterpret_cast<char*>(local_cursor) + 1, *local_cursor);
				name.append(".");
				local_cursor += *local_cursor + 1;
			}
		}
		if (jumped)
			*bytes_read += 1;
		else
			*bytes_read = (local_cursor - cursor) + 1;

		if (!name.empty())
			name.pop_back();
		return name;
	}
};

int main(const int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cout << "./dns_resolver <domain> [A|AAAA] [-d]" << std::endl;
		return 0;
	}
	const std::string domain = argv[1];
	const std::string type_str = (argc > 2 && argv[2][0] != '-') ? argv[2] : "A";
	bool d = false;
	for (int i = 0; i < argc; i++)
		if (strcmp(argv[i], "-d") == 0)
			d = true;
	DnsResolver r(d);
	r.resolve(domain, (type_str == "AAAA" ? TYPE_AAAA : TYPE_A));
	return 0;
}