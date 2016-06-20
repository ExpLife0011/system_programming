// DllInjection.cpp : Defines the entry point for the console application.
//

/*
получить имя экзешника
загрузть его, но не запускать
найти в нем указатель на kernel32.dll
найти в нем LoadLibrary (мб он всегда по одинаковому смещению)

RtlAddFunctionTable <- for unwinding
*/

#include "stdafx.h"
#include <Windows.h>
#include <winternl.h>

typedef unsigned int u32_t;

typedef NTSTATUS(WINAPI* FuncNtQueryInformationProcess)(
	HANDLE ProcessHandle,
	PROCESSINFOCLASS ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength);

typedef NTSTATUS(NTAPI *_NtQueryInformationProcess)(
	IN HANDLE ProcessHandle,
	ULONG ProcessInformationClass,
	OUT PVOID ProcessInformation,
	IN ULONG ProcessInformationLength,
	OUT PULONG ReturnLength OPTIONAL
	);

bool CreateSuspendedProcess(_TCHAR* cmd, PROCESS_INFORMATION* pi);
bool ResumeSuspendedProcess(PROCESS_INFORMATION pi);
void WaitForProcess(PROCESS_INFORMATION pi);
void* RemoteAllocateMemory(PROCESS_INFORMATION pi, SIZE_T size);
BOOL RemoteFreeMemory(PROCESS_INFORMATION pi, void* baseAddr);
SIZE_T RemoteWriteMemory(PROCESS_INFORMATION pi, void* baseAddr, void* dataAddr, SIZE_T dataLen);
HANDLE RemoteCreateThread(PROCESS_INFORMATION pi, void* addr);

void* FindKernel32AddressX86(PROCESS_INFORMATION pi); // ok for 32(main) - 32(susp)
void* FindKernel32AddressX64(PROCESS_INFORMATION pi); // ok for 64 - looking for argv
void* FindKernel32AddressSelf(PROCESS_INFORMATION pi); // ok for self, fail for remote (x64)

struct sc_data_t {
	char libName[16];
	void* ploadLibrary;
	UINT32 a, b, c;
	UINT32 reserved;
};

char sc_bytecode[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0xC3 };

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

	// if wow than x86 else x64
	BOOL wow;
	IsWow64Process(pi.hProcess, &wow);
	if (wow) printf("WOW \n");

	// allocating memory in remote process
	SIZE_T size = 512;
	void* baseAddr = RemoteAllocateMemory(pi, size);
	if (baseAddr == NULL) {
		printf("Allocate remote memory error: 0x%x \n", GetLastError());
		goto err0;
	}

	sc_data_t sc_data;
	
	// writing shellcode to remote process
	SIZE_T ret = RemoteWriteMemory(pi, baseAddr, &sc_data, sizeof(sc_data_t));
	printf("Written bytes %d \n", ret);
	ret = RemoteWriteMemory(pi, (char*)baseAddr + sizeof(sc_data_t), &sc_bytecode, sizeof(sc_bytecode));
	printf("Written bytes %d \n", ret);
	
	// creating remote thread for nop-shellode execution
	HANDLE hThr = RemoteCreateThread(pi, (char*)baseAddr + sizeof(sc_data_t));
	if (hThr == NULL) {
		printf("Error creating remote thread: 0x%x \n", GetLastError());
		goto err1;
	}

	// waiting for end of nop-shwllcode execution
	ret = WaitForSingleObject(hThr, INFINITE);
	printf("WFSO ret %d \n", ret);
	ret = CloseHandle(hThr);
	printf("CH ret %d \n", ret);

	// find kernel32.dll base address
	void* kernel32Base = FindKernel32AddressSelf(pi);
	printf("Kernel32 base address: %p \n", kernel32Base);

	printf("LoadLibrary: 0x%x\n", *(unsigned*)((char*)kernel32Base));

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
		0,	// Flag: create suspended process until ResumeThread
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

void* FindKernel32AddressX86(PROCESS_INFORMATION pi)
{
	FuncNtQueryInformationProcess ntqip = (FuncNtQueryInformationProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQueryInformationProcess");
	PROCESS_BASIC_INFORMATION info;

	NTSTATUS status = ntqip(
		pi.hProcess,				// ProcessHandle
		ProcessBasicInformation,	// ProcessInformationClass
		&info,						// ProcessInformation
		sizeof(info),				// ProcessInformationLength
		0);							// ReturnLength

	if (NT_SUCCESS(status)) {
		_tprintf(TEXT("Success getting process info \n"));
		PPEB peb = (PPEB)info.PebBaseAddress;
		//DWORD k32 = (DWORD)peb->Ldr->InMemoryOrderModuleList.Flink[0].Flink->Flink + 0x10;
		//return *(void **)k32;

		// info about all dlls
		void* Ldr = *((void **)((unsigned char *)peb + 0x0c));
		void* Flink = *((void **)((unsigned char *)Ldr + 0x0c));
		void* p = Flink;
		do
		{
		void* BaseAddress = *((void **)((unsigned char *)p + 0x18));
		void* FullDllName = *((void **)((unsigned char *)p + 0x28));
		wprintf(L"FullDllName is %s\n", FullDllName);
		printf("BaseAddress is %x\n", BaseAddress);
		printf("P is %p \n", p);
		p = *((void **)p);
		} while (Flink != p);
	}
	else {
		_tprintf(TEXT("Error getting process info \n"));
		return NULL;
	}
	return NULL;
}

void* FindKernel32AddressX64(PROCESS_INFORMATION pi)
{

	// open the process
	HANDLE hProcess = pi.hProcess;
	DWORD err = 0;

	// determine if 64 or 32-bit processor
	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);

	// determine if this process is running on WOW64
	BOOL wow;
	IsWow64Process(GetCurrentProcess(), &wow);

	// use WinDbg "dt ntdll!_PEB" command and search for ProcessParameters offset to find the truth out
	DWORD ProcessParametersOffset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x20 : 0x10;
	DWORD CommandLineOffset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x70 : 0x40;

	// read basic info to get ProcessParameters address, we only need the beginning of PEB
	DWORD pebSize = ProcessParametersOffset + 8;
	PBYTE peb = (PBYTE)malloc(pebSize);
	ZeroMemory(peb, pebSize);

	// read basic info to get CommandLine address, we only need the beginning of ProcessParameters
	DWORD ppSize = CommandLineOffset + 16;
	PBYTE pp = (PBYTE)malloc(ppSize);
	ZeroMemory(pp, ppSize);

	PWSTR cmdLine = NULL;

	// we're running as a 32-bit process in a 32-bit OS, or as a 64-bit process in a 64-bit OS
	PROCESS_BASIC_INFORMATION pbi;
	ZeroMemory(&pbi, sizeof(pbi));

	// get process information
	_NtQueryInformationProcess query = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
	err = query(hProcess, 0, &pbi, sizeof(pbi), NULL);
	if (err != 0)
	{
		printf("NtQueryInformationProcess failed\n");
		return NULL;
	}

	// read PEB
	if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, peb, pebSize, NULL))
	{
		printf("ReadProcessMemory PEB failed\n");
		return NULL;
	}

	typedef struct _PEB64 {
		BYTE Reserved1[2];
		BYTE BeingDebugged;
		BYTE Reserved2[21];
		PPEB_LDR_DATA LoaderData;
		PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
		BYTE Reserved3[520];
		PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
		BYTE Reserved4[136];
		ULONG SessionId;
	} PEB64, *PPEB64;

	PPEB ppeb = (PPEB)peb;

	PBYTE* ldr = (PBYTE*)*(LPVOID*)(peb + 24); // address in remote process adress space
	if (!ReadProcessMemory(hProcess, ldr, pp, ppSize, NULL))
	{
		printf("ReadProcessMemory Loader failed %d\n", GetLastError());
		return NULL;
	}

	// read ProcessParameters
	PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + ProcessParametersOffset); // address in remote process adress space
	if (!ReadProcessMemory(hProcess, parameters, pp, ppSize, NULL))
	{
		printf("ReadProcessMemory Parameters failed\n");
		return NULL;
	}

	// read CommandLine
	UNICODE_STRING* pCommandLine = (UNICODE_STRING*)(pp + CommandLineOffset);
	cmdLine = (PWSTR)malloc(pCommandLine->MaximumLength);
	if (!ReadProcessMemory(hProcess, pCommandLine->Buffer, cmdLine, pCommandLine->MaximumLength, NULL))
	{
		printf("ReadProcessMemory Parameters failed\n");
		return NULL;
	}
	printf("%S\n", cmdLine);

	return NULL;
}

void* FindKernel32AddressSelf(PROCESS_INFORMATION pi)
{
	FuncNtQueryInformationProcess ntqip = (FuncNtQueryInformationProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQueryInformationProcess");
	PROCESS_BASIC_INFORMATION info;
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hProcess1 = pi.hProcess;

	NTSTATUS status = ntqip(
		hProcess,					// ProcessHandle
		ProcessBasicInformation,	// ProcessInformationClass
		&info,						// ProcessInformation
		sizeof(info),				// ProcessInformationLength
		0);							// ReturnLength

	if (NT_SUCCESS(status)) {
		_tprintf(TEXT("Success getting process info \n"));
		PPEB ppeb = (PPEB)info.PebBaseAddress;

		struct LDR_MODULE
		{
			LIST_ENTRY e[3];
			HMODULE    base;
			void      *entry;
			UINT       size;
			UNICODE_STRING dllPath;
			UNICODE_STRING dllname;
		};
		int ModuleList = 0x18;
		int ModuleListFlink = 0x18;
		int KernelBaseAddr = 0x10;

		INT_PTR peb = (INT_PTR)ppeb;
		INT_PTR mdllist = *(INT_PTR*)(peb + ModuleList);
		INT_PTR mlink = *(INT_PTR*)(mdllist + ModuleListFlink);
		INT_PTR krnbase = *(INT_PTR*)(mlink + KernelBaseAddr);

		LDR_MODULE *mdl = (LDR_MODULE*)mlink;
		do
		{
			mdl = (LDR_MODULE*)mdl->e[0].Flink;

			if (mdl->base != NULL)
			{
				if (!lstrcmpiW(mdl->dllname.Buffer, L"kernel32.dll"))
				{
					return mdl->base;
				}
			}
		} while (mlink != (INT_PTR)mdl);
	}
	else {
		_tprintf(TEXT("Error getting process info \n"));
		return NULL;
	}
	return NULL;
}