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

#define MAX_VARINT_SIZE 5
#define MAX_STRING_SIZE (32767 * 4 + 3)

// Packet Length + Packet Id + String Length + String
#define MAX_RESP_SIZE (MAX_VARINT_SIZE * 3 + MAX_STRING_SIZE)

#define VARINT_NEED_MORE_DATA -1
#define VARINT_INVALID -2

// Read VarInt from buffer - returns amount of bytes read on success
// https://wiki.vg/Protocol#VarInt_and_VarLong
int readVarInt(unsigned char* buf, ssize_t len, ssize_t* numOut) {
	ssize_t i = -1, num = 0;
	do {
		i++;
		if (i >= 5)
			return VARINT_INVALID;
		else if (i >= len)
			return VARINT_NEED_MORE_DATA;

		num |= (buf[i] & 0x7f) << (7 * i);
	} while (buf[i] & 0x80);
	*numOut = num;
	return i + 1;
}

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

	// Send landshake packet and Request packet
	// https://wiki.vg/Server_List_Ping
	const unsigned char packets[9] = {6, 0, 0, 0, 0, 0, 1, 1, 0};
	send(sockfd, packets, sizeof(packets), 0);

	static unsigned char respBuf[MAX_RESP_SIZE];
	ssize_t respIt = 0, respSize = 0;

	// Receive response packet length
	// https://wiki.vg/Protocol#Packet_format
	ssize_t packetLength;
	while (1) {
		ssize_t got = recv(sockfd, respBuf + respSize, MAX_RESP_SIZE - respSize, 0);
		if (got < 0) {
			fprintf(stderr, "Failed to get packet length (no response)\n");
			return 1;
		} else if (got > 0) {
			respSize += got;
			int bread = readVarInt(respBuf, respSize, &packetLength);
			if (bread > 0) {
				respIt += bread;
				break;
			} else if (bread == VARINT_INVALID) {
				fprintf(stderr, "Failed to get packet length (corrupt)\n");
				return 1;
			}
		}
	}

	// Packet length
	if (packetLength < 2) { // 1 for ID, 1 for string length
		fprintf(stderr, "Invalid packet length\n");
		return 1;
	}

	// Receive rest of packet
	while (respSize - respIt < packetLength) {
		ssize_t got = recv(sockfd, respBuf + respSize, MAX_RESP_SIZE - respSize, 0);
		if (got < 0) {
			fprintf(stderr, "Failed to get packet data (no response)\n");
			return 1;
		}
		respSize += got;
	}

	// Packet ID
	if (respBuf[respIt] != 0x00) {
		fprintf(stderr, "Invalid response packet ID\n");
		return 1;
	}
	respIt++;

	// Get JSON response string length
	ssize_t jsonStrLen = 0;
	int bread = readVarInt(respBuf + respIt, respSize - respIt, &jsonStrLen);
	if (bread < 0) {
		fprintf(stderr, "Invalid string length in response\n");
		return 1;
	}
	respIt += bread;

	// Print response string
	printf("%.*s\n", (int)jsonStrLen, respBuf + respIt);

	#ifdef _WIN32
		closesocket(sockfd);
		WSACleanup();
	#else
		close(sockfd);
	#endif // _WIN32

	return 0;
}
