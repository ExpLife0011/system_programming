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

typedef NTSTATUS(WINAPI* FuncNtQueryInformationProcess)(
	HANDLE ProcessHandle,
	PROCESSINFOCLASS ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength);

// NtQueryInformationProcess for pure 32 and 64-bit processes
typedef NTSTATUS(NTAPI *_NtQueryInformationProcess)(
	IN HANDLE ProcessHandle,
	ULONG ProcessInformationClass,
	OUT PVOID ProcessInformation,
	IN ULONG ProcessInformationLength,
	OUT PULONG ReturnLength OPTIONAL
	);

typedef NTSTATUS(NTAPI *_NtReadVirtualMemory)(
	IN HANDLE ProcessHandle,
	IN PVOID BaseAddress,
	OUT PVOID Buffer,
	IN SIZE_T Size,
	OUT PSIZE_T NumberOfBytesRead);

// NtQueryInformationProcess for 32-bit process on WOW64
typedef NTSTATUS(NTAPI *_NtWow64ReadVirtualMemory64)(
	IN HANDLE ProcessHandle,
	IN PVOID64 BaseAddress,
	OUT PVOID Buffer,
	IN ULONG64 Size,
	OUT PULONG64 NumberOfBytesRead);

// PROCESS_BASIC_INFORMATION for 32-bit process on WOW64
// The definition is quite funky, as we just lazily doubled sizes to match offsets...
typedef struct _PROCESS_BASIC_INFORMATION_WOW64 {
	PVOID Reserved1[2];
	PVOID64 PebBaseAddress;
	PVOID Reserved2[4];
	ULONG_PTR UniqueProcessId[2];
	PVOID Reserved3[2];
} PROCESS_BASIC_INFORMATION_WOW64;

typedef struct _UNICODE_STRING_WOW64 {
	USHORT Length;
	USHORT MaximumLength;
	PVOID64 Buffer;
} UNICODE_STRING_WOW64;


bool CreateSuspendedProcess(_TCHAR* cmd, PROCESS_INFORMATION* pi);
bool ResumeSuspendedProcess(PROCESS_INFORMATION pi);
void WaitForProcess(PROCESS_INFORMATION pi);
void* FindKernel32AddressX86_X86(PROCESS_INFORMATION pi);
void* FindKernel32AddressX86(PROCESS_INFORMATION pi);
void* FindKernel32AddressX64(PROCESS_INFORMATION pi);
void* FindKernel32AddressSelf(PROCESS_INFORMATION pi);

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
	if (wow) printf("WOW \n");
	// if wow than x86 else x64

	//void* kernel32Base = FindKernel32AddressSelf(pi);
	void* kernel32Base = FindKernel32AddressX86(pi);

	//void* kernel32Base = (wow) ? (FindKernel32AddressX86(pi)) : (FindKernel32AddressX64(pi));
	if (!kernel32Base) {
		printf("Finding kernel32.dll error \n");
		goto err;
	}
	printf("Kernel32.dll base: 0x%p \n", kernel32Base);
	
err:
	if (!ResumeSuspendedProcess(pi)) {
		printf("Resuming process error: 0x%x \n", GetLastError());
		return 0;
	}
	WaitForProcess(pi);

	getchar();
	return 0;
}

void* FindKernel32AddressX4(PROCESS_INFORMATION pi)
{
	FuncNtQueryInformationProcess zwqip = (FuncNtQueryInformationProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQueryInformationProcess");
	PROCESS_BASIC_INFORMATION info;

	NTSTATUS status = zwqip(
		pi.hProcess,				// ProcessHandle
		ProcessBasicInformation,	// ProcessInformationClass
		&info,						// ProcessInformation
		sizeof(info),				// ProcessInformationLength
		0);							// ReturnLength

	if (NT_SUCCESS(status)) {
		_tprintf(TEXT("Success getting process info \n"));
		struct LDR_MODULE
		{
			LIST_ENTRY e[3];
			HMODULE    base;
			void      *entry;
			UINT       size;
			UNICODE_STRING dllPath;
			UNICODE_STRING dllname;
		};
		int offset = 0x60;
		int ModuleList = 0x18;
		int ModuleListFlink = 0x18;
		int KernelBaseAddr = 0x10;

		INT_PTR peb = (INT_PTR)((char*)&info + 8);
		INT_PTR mdllist = *(INT_PTR*)(peb + ModuleList);
		INT_PTR mlink = *(INT_PTR*)(mdllist + ModuleListFlink);
		INT_PTR krnbase = *(INT_PTR*)(mlink + KernelBaseAddr);
		LDR_MODULE *mdl = (LDR_MODULE*)mlink;
		do
		{
			mdl = (LDR_MODULE*)mdl->e[0].Flink;

			if (mdl->base != NULL)
			{
				if (!lstrcmpiW(mdl->dllname.Buffer, L"kernel32.dll")) //сравниваем имя библиотеки в буфере с необходимым
				{
					break;
				}
			}
		} while (mlink != (INT_PTR)mdl);

	} else {
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

void* FindKernel32AddressX86(PROCESS_INFORMATION pi)
{
	// we're running as a 32-bit process in a 64-bit OS
	PROCESS_BASIC_INFORMATION pbi;
	ZeroMemory(&pbi, sizeof(pbi));

	// get process information from 64-bit world
	FuncNtQueryInformationProcess ntqip = (FuncNtQueryInformationProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQueryInformationProcess");
	DWORD err = ntqip(pi.hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);
	if (err != 0)
	{
		printf("NtWow64QueryInformationProcess64 failed with err 0x%x adn 0x%x\n", GetLastError(), err);
		return NULL;
	}

	// read PEB from 64-bit address space
	_NtWow64ReadVirtualMemory64 read = (_NtWow64ReadVirtualMemory64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64ReadVirtualMemory64");
	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);
	DWORD ProcessParametersOffset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x20 : 0x10;
	DWORD pebSize = ProcessParametersOffset + 8;
	PBYTE peb = (PBYTE)malloc(pebSize);
	err = read(pi.hProcess, pbi.PebBaseAddress, peb, pebSize, NULL);
	if (err != 0)
	{
		printf("NtWow64ReadVirtualMemory64 PEB failed\n");
		return NULL;
	}

	int ModuleList = 0x18;
	int ModuleListFlink = 0x18;
	INT_PTR mdllist = *(INT_PTR*)(peb + ModuleList);
	INT_PTR mlink = *(INT_PTR*)(mdllist + ModuleListFlink);

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
				if (!lstrcmpiW(mdl->dllname.Buffer, L"kernel32.dll")) //сравниваем имя библиотеки в буфере с необходимым
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

void* FindKernel32AddressX86_X86(PROCESS_INFORMATION pi)
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
		DWORD k32 = (DWORD)peb->Ldr->InMemoryOrderModuleList.Flink[0].Flink->Flink + 0x10;
		return *(void **)k32;

		// info about all dlls
		/*void* Ldr = *((void **)((unsigned char *)peb + 0x0c));
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
		} while (Flink != p);*/
	}
	else {
		_tprintf(TEXT("Error getting process info \n"));
		return NULL;
	}
	return NULL;
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