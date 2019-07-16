#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501 // WinXP
	#endif // _WIN32_WINNT

	#include <winsock2.h>
	#include <Ws2tcpip.h>

	typedef SOCKET SOCKET_INT;
	#define SOCKET_OPEN_FAILED(fd) (fd == INVALID_SOCKET)
#else
	#include <netdb.h>
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>

	typedef int SOCKET_INT;
	#define SOCKET_OPEN_FAILED(fd) (fd < 0)
#endif // _WIN32

#define DEFAULT_PORT 25565
#define MAX_RESP_SIZE 131072

int main(int argc, char** argv)
{
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "%s <hostname> [port]\n", argv[0]);
		return 1;
	}

	#ifdef _WIN32
		WSADATA wsa_data;
		if (WSAStartup(MAKEWORD(1, 1), &wsa_data)) {
			fprintf(stderr, "Windows socket init failed\n");
			return 1;
		}
	#endif //_WIN32

	SOCKET_INT sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCKET_OPEN_FAILED(sockfd)) {
		fprintf(stderr, "Failed to open socket\n");
		return 1;
	}

	struct hostent* host = gethostbyname(argv[1]);
	if (host == 0) {
		fprintf(stderr, "Failed to get host\n");
		return 1;
	}

	int portnum = argc == 3 ? atoi(argv[2]) : DEFAULT_PORT;
	if (portnum <= 0 || portnum > 65535) {
		fprintf(stderr, "Invalid port number\n");
		return 1;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(portnum),
		.sin_addr = *(struct in_addr*)host->h_addr_list[0]
	};
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "Failed to connect\n");
		return 1;
	}

	// Handshake packet and Request packet (https://wiki.vg/Server_List_Ping)
	send(sockfd, "\x06\x00\x00\x00\x00\x00\x01\x01\x00", 9, 0);

	// Response
	static char response[MAX_RESP_SIZE];
	int resp_size = recv(sockfd, response, MAX_RESP_SIZE, 0);
	if (resp_size < 0) {
		fprintf(stderr, "Failed to get response\n");
		return 1;
	}

	// Packet length
	int i = 0, packet_length = 0;
	do {
		packet_length |= (response[i] & 0x7f) << (7 * i);
	} while (response[i++] & 0x80);
	const int packet_length_i = i;

	// Packet ID
	if (response[i++] != 0x00) {
		fprintf(stderr, "Invalid response packet ID\n");
		return 1;
	}

	// Skip packet string length
	while (response[i++] & 0x80);

	// Calculate response string length
	uint32_t resp_str_len = packet_length + packet_length_i - i;
	if (resp_str_len + i > resp_size) {
		fprintf(stderr, "Invalid response packet size\n");
		return 1;
	}

	// Print response string
	printf("%.*s\n", resp_str_len, response + i);

	#ifdef _WIN32
		closesocket(sockfd);
		WSACleanup();
	#else
		close(sockfd);
	#endif // _WIN32

	return 0;
}
