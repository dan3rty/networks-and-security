#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define PROXY_PORT 8888
#define BUFFER_SIZE 4096
#define CACHE_DIR "./cache/"

using namespace std;

void setup_cache_dir()
{
	struct stat st = { 0 };
	if (stat(CACHE_DIR, &st) == -1)
	{
		mkdir(CACHE_DIR, 0700);
	}
}

string get_cache_filename(const string& url)
{
	constexpr hash<string> hasher;
	const size_t hash = hasher(url);
	return string(CACHE_DIR) + to_string(hash) + ".cache";
}

bool parse_url(const string& url, string& host, int& port, string& path)
{
	const size_t protocol_pos = url.find("://");
	string clean_url = (protocol_pos != string::npos) ? url.substr(protocol_pos + 3) : url;

	const size_t path_pos = clean_url.find('/');
	string host_port;

	if (path_pos == string::npos)
	{
		host_port = clean_url;
		path = "/";
	}
	else
	{
		host_port = clean_url.substr(0, path_pos);
		path = clean_url.substr(path_pos);
	}

	size_t port_pos = host_port.find(':');
	if (port_pos != string::npos)
	{
		host = host_port.substr(0, port_pos);
		port = stoi(host_port.substr(port_pos + 1));
	}
	else
	{
		host = host_port;
		port = 80;
	}
	return true;
}

void fetch_from_server(int client_sock, const string& host, int port, const string& path, const string& url_key)
{
	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock < 0)
	{
		perror("Ошибка создания сокета к серверу");
		return;
	}

	hostent* server = gethostbyname(host.c_str());
	if (server == nullptr)
	{
		cerr << "Не удалось найти хост: " << host << endl;
		close(server_sock);
		return;
	}

	sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	bcopy(server->h_addr, &server_addr.sin_addr.s_addr, server->h_length);
	server_addr.sin_port = htons(port);

	if (connect(server_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0)
	{
		perror("Ошибка подключения к серверу");
		close(server_sock);
		return;
	}

	string request = "GET " + path + " HTTP/1.0\r\n";
	request += "Host: " + host + "\r\n";
	request += "Connection: close\r\n\r\n";

	send(server_sock, request.c_str(), request.length(), 0);

	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;
	string filename = get_cache_filename(url_key);

	bool is_cachable = false;
	bool header_parsed = false;
	ofstream cache_file;

	while ((bytes_read = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0)
	{
		if (!header_parsed)
		{
			string response_head(buffer, bytes_read);
			if (response_head.find("HTTP/1.1 200") != string::npos || response_head.find("HTTP/1.0 200") != string::npos)
			{
				is_cachable = true;
				cache_file.open(filename, ios::binary);
				cout << "[LOG] Ответ 200 OK. Начало кэширования: " << url_key << endl;
			}
			else
			{
				cout << "[LOG] Ответ не 200 OK (или ошибка). Кэширование пропущено." << endl;
			}
			header_parsed = true;
		}

		send(client_sock, buffer, bytes_read, 0);

		if (is_cachable && cache_file.is_open())
		{
			cache_file.write(buffer, bytes_read);
		}
	}

	if (cache_file.is_open())
	{
		cache_file.close();
	}
	close(server_sock);
}

void handle_client(int client_sock)
{
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read = recv(client_sock, buffer, BUFFER_SIZE, 0);

	if (bytes_read <= 0)
		return;

	string request(buffer, bytes_read);

	stringstream ss(request);
	string method, url, protocol;
	ss >> method >> url >> protocol;

	if (method != "GET")
	{
		cout << "[LOG] Метод " << method << " не поддерживается." << endl;
		return;
	}

	string host, path;
	int port;
	parse_url(url, host, port, path);

	string filename = get_cache_filename(url);
	ifstream cache_file(filename, ios::binary);

	cout << "------------------------------------------------" << endl;
	cout << "Запрос: " << url << endl;

	if (cache_file.is_open())
	{
		cout << "[LOG] Cache HIT! Отправка из файла." << endl;

		while (cache_file.read(buffer, BUFFER_SIZE) || cache_file.gcount() > 0)
		{
			send(client_sock, buffer, cache_file.gcount(), 0);
		}
		cache_file.close();
	}
	else
	{
		cout << "[LOG] Cache MISS. Загрузка с сервера " << host << "..." << endl;
		fetch_from_server(client_sock, host, port, path, url);
	}

	close(client_sock);
}

int main()
{
	setup_cache_dir();

	const int master_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (master_sock < 0)
	{
		perror("Socket failed");
		return 1;
	}

	constexpr int opt = 1;
	setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PROXY_PORT);

	if (bind(master_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		perror("Bind failed");
		return 1;
	}

	if (listen(master_sock, 10) < 0)
	{
		perror("Listen failed");
		return 1;
	}

	cout << "Прокси-сервер запущен на порту " << PROXY_PORT << endl;
	cout << "Ожидание подключений..." << endl;

	while (true)
	{
		sockaddr_in client_addr;
		socklen_t len = sizeof(client_addr);
		const int client_sock = accept(master_sock, reinterpret_cast<struct sockaddr*>(&client_addr), &len);

		if (client_sock < 0)
		{
			perror("Accept failed");
			continue;
		}

		handle_client(client_sock);
	}

	close(master_sock);
	return 0;
}