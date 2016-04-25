#define WIN32_LEAN_AND_MEAN

#include "stdafx.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "27015"
#define IP_ADDRESS "127.0.0.1"

int getServerInfo(struct addrinfo ** info);
int connectToServer(SOCKET * connSock, addrinfo * srvInfo);
int sendToServer(SOCKET ConnectSocket);
int recvFromServer(SOCKET ConnectSocket);

int __cdecl ClientProcess()
{
	int exitValue = 1;

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed \n");
		goto err0;
	}

	struct addrinfo *servInfo = NULL;
	if (getServerInfo(&servInfo) != 0) {
		printf("getServerInfo failed \n");
		goto err05;
	}

	SOCKET ConnectSocket = INVALID_SOCKET;
	if (connectToServer(&ConnectSocket, servInfo) != 0) {
		printf("connectToServer failed \n");
		goto err1;
	}

	/*char *sendbuf;
	if ((sendbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char))) == NULL) {
		printf("Allocating memory for send buffer error \n");
		goto err2;
	}

	char *recvbuf;
	if ((recvbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char))) == NULL) {
		printf("Allocating memory for recv buffer error \n");
		goto err25;
	}

	int recvRet = 0;
	do {
		// TODO: divide on two threads for send and recv 
		printf("Enter command: ");
		if (gets_s(sendbuf, DEFAULT_BUFLEN) == NULL)
			break;

		if (send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0) == SOCKET_ERROR) {
			printf("send failed with error: %d \n", WSAGetLastError());
			goto err3;
		}

		memset(recvbuf, 0, DEFAULT_BUFLEN);
		recvRet = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);

		if (recvRet > 0)
			printf("%s \n", recvbuf);
		else if (recvRet == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d \n", WSAGetLastError());

	} while (recvRet > 0);*/

	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvFromServer, (LPVOID)ConnectSocket, 0, NULL);
	if (hThread == NULL) {
		printf("CreateThread");
		goto err3;
	}

	sendToServer(ConnectSocket);

	WaitForSingleObject(hThread, INFINITE);

	if (shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		goto err3;
	}

	exitValue = 0;

err3:
	//free(recvbuf);
err25:
	//free(sendbuf);
err2:
	closesocket(ConnectSocket);
err1:
	freeaddrinfo(servInfo);
err05:
	WSACleanup();
err0:
	return exitValue;
}

int sendToServer(SOCKET ConnectSocket)
{
	char *sendbuf;
	if ((sendbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char))) == NULL) {
		printf("Allocating memory for send buffer error \n");
		goto err0;
	}

	do {
		printf("Enter command: ");
		if (gets_s(sendbuf, DEFAULT_BUFLEN) == NULL)
			break;

		if (send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0) == SOCKET_ERROR) {
			printf("send failed with error: %d \n", WSAGetLastError());
			goto end;
		}
	} while (true);

end:
	free(sendbuf);
	return 0;
err0:
	return -1;
}

int recvFromServer(SOCKET ConnectSocket)
{
	int recvRet;
	char *recvbuf;
	if ((recvbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char))) == NULL) {
		printf("Allocating memory for recv buffer error \n");
		goto err0;
	}

	do {
		memset(recvbuf, 0, DEFAULT_BUFLEN);
		recvRet = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);

		if (recvRet > 0) {
			printf("%s \n", recvbuf);
		} else if (recvRet == 0) {
			printf("Connection closed\n");
			goto end;
		} else {
			printf("recv failed with error: %d \n", WSAGetLastError());
			goto end;
		}
	} while (recvRet > 0);

end:
	free(recvbuf);
	return 0;
err0:
	return -1;
}

int connectToServer(SOCKET * connSock, addrinfo * srvInfo)
{
	*connSock = INVALID_SOCKET;
	struct addrinfo *info = NULL;
	for (info = srvInfo; info != NULL; info = info->ai_next) {
		*connSock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
		if (*connSock == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			goto err;
		}

		if (connect(*connSock, info->ai_addr, (int)info->ai_addrlen) == SOCKET_ERROR) {
			closesocket(*connSock);
			*connSock = INVALID_SOCKET;
			continue;
		}
		break;
	}

	if (*connSock == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		goto err;
	}

	return 0;
err:
	return 1;
}

int getServerInfo(struct addrinfo ** info)
{
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	return getaddrinfo(IP_ADDRESS, DEFAULT_PORT, &hints, info);
}