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
		fprintf(stderr, "Failed to find hostname\n");
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

	// Send Handshake packet and Request packet (https://wiki.vg/Server_List_Ping)
	send(sockfd, "\x06\x00\x00\x00\x00\x00\x01\x01\x00", 9, 0);

	// Response - Get packet length (https://wiki.vg/Protocol#Packet_format)
	static char response[MAX_RESP_SIZE];
	int resp_size = 0;
	do {
		int got = recv(sockfd, response + resp_size, 1, 0); // One byte at a time, todo: optimize?
		if (got != 1) {
			fprintf(stderr, "Failed to get packet length\n");
			return 1;
		}
	} while (response[resp_size++] & 0x80); // Check if last byte of VarInt

	// Packet length
	int packet_length = 0;
	for (int i = 0; i < resp_size; i++) {
		packet_length |= (response[i] & 0x7f) << (7 * i);
	}

	if (packet_length < 2) { // 1 for ID, 1 for string length
		fprintf(stderr, "Invalid packet length\n");
		return 1;
	}

	// Response - Get packet data
	resp_size = 0; // Reset response buffer for packet data
	while (resp_size < packet_length) {
		int got = recv(sockfd, response + resp_size, MAX_RESP_SIZE - resp_size, 0);
		resp_size += got;
		if (got < 0) {
			fprintf(stderr, "Failed to get packet data\n");
			return 1;
		}
	}

	// Packet ID
	if (response[0] != 0x00) {
		fprintf(stderr, "Invalid response packet ID\n");
		return 1;
	}

	// Read string length
	int str_len = 0;
	int i = 1; // Skip packet ID byte
	do {
		str_len |= (response[i] & 0x7f) << (7 * (i - 1));
	} while (response[i++] & 0x80);

	// Verify
	if (str_len < 0 || str_len > resp_size - i) {
		fprintf(stderr, "Invalid response string length\n");
		return 1;
	}

	// Print response string
	printf("%.*s\n", str_len, response + i);

	#ifdef _WIN32
		closesocket(sockfd);
		WSACleanup();
	#else
		close(sockfd);
	#endif // _WIN32

	return 0;
}
