// DllInjection.cpp : Defines the entry point for the console application.
//

/*
получить имя экзешника
загрузть его, но не запускать
найти в нем указатель на kernel32.dll
найти в нем LoadLibrary (мб он всегда по одинаковому смещению)
*/

#include "stdafx.h"
#include <Windows.h>
#include <winternl.h>

typedef unsigned int u32_t;

bool CreateSuspendedProcess(_TCHAR* cmd, PROCESS_INFORMATION* pi);
bool ResumeSuspendedProcess(PROCESS_INFORMATION pi);
void WaitForProcess(PROCESS_INFORMATION pi);
void* RemoteAllocateMemory(PROCESS_INFORMATION pi, SIZE_T size);
BOOL RemoteFreeMemory(PROCESS_INFORMATION pi, void* baseAddr);
SIZE_T RemoteWriteMemory(PROCESS_INFORMATION pi, void* baseAddr, void* dataAddr, SIZE_T dataLen);
HANDLE RemoteCreateThread(PROCESS_INFORMATION pi, void* addr);

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2) {
		printf("Usage: %s [cmdline] \n", argv[0]);
		return 0;
	}

	PROCESS_INFORMATION pi;
	if (!CreateSuspendedProcess(argv[1], &pi)) {
		printf("Creating process error: 0x%x \n", GetLastError());
		return 0;
	}

	BOOL wow;
	IsWow64Process(pi.hProcess, &wow);
	if (wow) printf("WOW \n"); // if wow than x86 else x64

	SIZE_T size = 512;
	void* baseAddr = RemoteAllocateMemory(pi, size);
	if (baseAddr == NULL) {
		printf("Allocate remote memory error: 0x%x \n", GetLastError());
		goto err0;
	}

	// example of writing memory
	int data[20];
	SIZE_T ret = RemoteWriteMemory(pi, baseAddr, data, 20 * sizeof(int));
	printf("Written bytes %d \n", ret);
	

err1:
	if (!RemoteFreeMemory(pi, baseAddr)) {
		printf("Free remote memory error: 0x%x \n", GetLastError());
	}
	
err0:
	if (!ResumeSuspendedProcess(pi)) {
		printf("Resuming process error: 0x%x \n", GetLastError());
		return 0;
	}
	WaitForProcess(pi);

	getchar();
	return 0;
}


HANDLE RemoteCreateThread(PROCESS_INFORMATION pi, void* addr)
{
	return CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)addr, NULL, 0, NULL);
}

void* RemoteAllocateMemory(PROCESS_INFORMATION pi, SIZE_T size)
{
	return VirtualAllocEx(pi.hProcess, NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

BOOL RemoteFreeMemory(PROCESS_INFORMATION pi, void* baseAddr)
{
	return VirtualFreeEx(pi.hProcess, baseAddr, 0, MEM_RELEASE);
}

SIZE_T RemoteWriteMemory(PROCESS_INFORMATION pi, void* baseAddr, void* dataAddr, SIZE_T dataLen)
{
	SIZE_T written = 0;
	if (!WriteProcessMemory(pi.hProcess, baseAddr, dataAddr, dataLen, &written))
		printf("Error writing memory to remote process: 0x%x \n", GetLastError());
	return written;
}

bool CreateSuspendedProcess(_TCHAR* cmd, PROCESS_INFORMATION* pi)
{
	STARTUPINFO si;
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(si);
	memset(pi, 0, sizeof(PROCESS_INFORMATION));

	return CreateProcess(NULL,   // No module name (use command line)
		cmd,			// Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		CREATE_SUSPENDED,	// Flag: create suspended process until ResumeThread
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		pi);            // Pointer to PROCESS_INFORMATION structure
}

bool ResumeSuspendedProcess(PROCESS_INFORMATION pi)
{
	return ResumeThread(pi.hThread) != ((DWORD)-1);
}

void WaitForProcess(PROCESS_INFORMATION pi)
{
	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}