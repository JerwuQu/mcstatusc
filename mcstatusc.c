#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 // WinXP

#include <winsock2.h>
#include <Ws2tcpip.h>

typedef SOCKET sock_handle_t;
#define SOCKET_OPEN_SUCCESS(fd) ((fd) != INVALID_SOCKET)

#else // _WIN32

#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

typedef int sock_handle_t;
#define SOCKET_OPEN_SUCCESS(fd) ((fd) >= 0)

#endif // _WIN32

#define DEFAULT_PORT 25565
#define DEFAULT_PROTOCOL_VERSION 0

#define MAX_VARINT_SZ 5
#define MAX_STRING_SZ (32767 * 4 + 3)

// Handshake: Packet Length + Packet ID + Protocol Version + String + Short + Next State
// Request: Packet Length + Packet ID
#define MAX_REQ_SZ (MAX_VARINT_SZ * 5 + MAX_STRING_SZ + 2 + MAX_VARINT_SZ * 2)

// Packet Length + Packet ID + String Length + String
#define MAX_RESP_SZ (MAX_VARINT_SZ * 3 + MAX_STRING_SZ)

static inline void err(const char *fmt, ...)
{
	va_list va;
	fputs("error: ", stderr);
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fputc('\n', stderr);
	exit(1);
}

// Read VarInt from buffer
// Returns amount of bytes read on success or -1 if more data is needed
// Spec: https://wiki.vg/Protocol#VarInt_and_VarLong
static inline int readVarInt(const uint8_t *buf, size_t len, int32_t *numOut) {
	size_t i = 0;
	int32_t num = 0;
	do {
		if (i >= len) {
			return -1;
		} else if (i >= 5) {
			err("invalid varint (data corruption)");
		} else {
			num |= (buf[i] & 0x7f) << (7 * i);
		}
		i++;
	} while (buf[i - 1] & 0x80);
	*numOut = num;
	return i;
}

// Write VarInt to buffer (has to be at least 5 bytes in size)
// Returns amount of bytes written to buffer
// Spec: https://wiki.vg/Protocol#VarInt_and_VarLong
static inline size_t writeVarInt(int32_t value, uint8_t *buf) {
	size_t i = 0;
	do {
		uint8_t temp = value & 0x7f;
		value >>= 7;
		if (value) {
			temp |= 0x80;
		}
		buf[i++] = temp;
	} while (value);
	return i;
}

static inline void failsafeSend(sock_handle_t sockfd, const void *buf, ssize_t sz)
{
	if (send(sockfd, buf, sz, 0) != sz) {
		err("failed to send data");
	}
}

// https://wiki.vg/Server_List_Ping
static inline void sendServerListPing(const sock_handle_t sockfd,
		const char *hostname, uint16_t port, uint32_t protocolVersion)
{
	uint8_t packetBuf[MAX_REQ_SZ];
	size_t bufIt = 0;

	// Packet ID
	packetBuf[bufIt++] = 0;

	// Protocol version
	bufIt += writeVarInt(protocolVersion, packetBuf + bufIt);

	// Server address
	const int hostnameLen = strlen(hostname);
	bufIt += writeVarInt(hostnameLen, packetBuf + bufIt);
	memcpy(packetBuf + bufIt, hostname, hostnameLen);
	bufIt += hostnameLen;

	// Server port
	packetBuf[bufIt++] = port >> 8;
	packetBuf[bufIt++] = port & 0xff;

	// Next state
	packetBuf[bufIt++] = 1;

	// Send handshake packet length
	uint8_t handshakePacketVarInt[MAX_VARINT_SZ];
	failsafeSend(sockfd, handshakePacketVarInt, writeVarInt(bufIt, handshakePacketVarInt));

	// Prepare request packet as well
	packetBuf[bufIt++] = 1; // Packet size
	packetBuf[bufIt++] = 0; // Packet ID

	// Send both packets (handshake + request)
	failsafeSend(sockfd, packetBuf, bufIt);
}

static inline void printReceiveResponse(const sock_handle_t sockfd)
{
	uint8_t respBuf[MAX_RESP_SZ];
	ssize_t respIt = 0, respSize = 0;

	// Receive response packet length
	// https://wiki.vg/Protocol#Packet_format
	int32_t packetLength;
	while (1) {
		const ssize_t got = recv(sockfd, respBuf + respSize, MAX_RESP_SZ - respSize, 0);
		if (got < 0) {
			err("no response");
		} else if (got > 0) {
			respSize += got;
			const int bread = readVarInt(respBuf, respSize, &packetLength);
			if (bread > 0) {
				respIt += bread;
				break;
			}
		}
	}

	// Packet length
	if (packetLength < 2) { // 1 for ID, 1 for string length
		err("invalid packet length");
	}

	// Receive rest of packet
	while (respSize - respIt < packetLength) {
		const ssize_t got = recv(sockfd, respBuf + respSize, MAX_RESP_SZ - respSize, 0);
		if (got < 0) {
			err("expected more data");
		}
		respSize += got;
	}

	// Packet ID
	if (respBuf[respIt] != 0x00) {
		err("invalid response packet id");
	}
	respIt++;

	// Get JSON response string length
	int32_t jsonStrLen = 0;
	const int bread = readVarInt(respBuf + respIt, respSize - respIt, &jsonStrLen);
	if (bread < 0) {
		err("missing string length in response");
	}
	respIt += bread;

	// Make sure string length is reasonable
	if (jsonStrLen > respSize - respIt) {
		err("invalid string length in response");
	}

	// Print response string
	printf("%.*s\n", jsonStrLen, respBuf + respIt);
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 4) {
		err("%s <hostname> [port] [protocol version]", argv[0]);
	}

	const char *hostname = argv[1];
	const int portnum = argc >= 3 ? atoi(argv[2]) : DEFAULT_PORT;
	if (portnum < 1 || portnum > 65535) {
		err("invalid port number");
	}

	const int protocolVersion = argc >= 4 ? atoi(argv[3]) : DEFAULT_PROTOCOL_VERSION;
	if (protocolVersion < 0 || protocolVersion >= 0x7fffffff) {
		err("invalid protocol version");
	}

#ifdef _WIN32
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(1, 1), &wsa_data)) {
		err("windows socket init failed");
	}
#endif //_WIN32

	const sock_handle_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (!SOCKET_OPEN_SUCCESS(sockfd)) {
		err("socket open failed");
	}

	struct hostent *host = gethostbyname(hostname);
	if (!host) {
		err("hostname lookup failed");
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(portnum),
		.sin_addr = *(struct in_addr*)host->h_addr_list[0]
	};
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0) {
		err("failed to connect");
	}

	sendServerListPing(sockfd, hostname, portnum, protocolVersion);
	printReceiveResponse(sockfd);

#ifdef _WIN32
	closesocket(sockfd);
	WSACleanup();
#else // _WIN32
	close(sockfd);
#endif // _WIN32

	return 0;
}
