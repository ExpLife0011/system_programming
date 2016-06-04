// RemoteCmd.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "ssrv_service.h"

#define addLogMessage(x) _tprintf(_T(x))

_TCHAR *gServiceName = _T("ConsoleVMSvc");
_TCHAR *gServicePath;
SERVICE_STATUS gServiceStatus;
SERVICE_STATUS_HANDLE gStatusHandle;


void ServiceMain(int argc, TCHAR* argv[]);
void ControlHandler(DWORD request);
int InitService();
int StartServiceCmd();
int RemoveService();
int InstallService();
VOID StopService();
int StopServer();
int ServerMain(DWORD *SvcState);


int mainS(int argc, TCHAR* argv[])
{
	if (argc == 1) {
		SERVICE_TABLE_ENTRY ServiceTable[2];
		ServiceTable[0].lpServiceName = gServiceName;
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;

		if (!StartServiceCtrlDispatcher(ServiceTable)) {
			TCHAR msg[80];
			_stprintf_s(msg, _T("Error: Dispatcher: %u"), GetLastError());
			SvcReportError(msg);
		}
		return 0;
	}
	else if ((argc == 4) && (_tcscmp(argv[2], _T("install")) == 0)) {
		gServicePath = argv[3];
		InstallService();
	}
	else if (_tcscmp(argv[2], _T("remove")) == 0) {
		RemoveService();
	}
	else if (_tcscmp(argv[2], _T("start")) == 0){
		StartServiceCmd();
	}
	else if (_tcscmp(argv[2], _T("stop")) == 0){
		StopService();
	}
	else {
		_tprintf(_T("Invalid command %s\n"), argv[argc - 1]);
		return 1;
	}
	return 0;
}

void ServiceMain(int argc, TCHAR* argv[])
{
	int error;
	int i = 0;

	gServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	gServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	gServiceStatus.dwWin32ExitCode = 0;
	gServiceStatus.dwServiceSpecificExitCode = 0;
	gServiceStatus.dwCheckPoint = 0;
	gServiceStatus.dwWaitHint = 0;

	gStatusHandle = RegisterServiceCtrlHandler(gServiceName, (LPHANDLER_FUNCTION)ControlHandler);
	if (gStatusHandle == (SERVICE_STATUS_HANDLE)0) {
		return;
	}

	gServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(gStatusHandle, &gServiceStatus);
	

	while (gServiceStatus.dwCurrentState == SERVICE_RUNNING)
	{
		ServerMain(&gServiceStatus.dwCurrentState);
		i++;
	}

	gServiceStatus.dwWin32ExitCode = 0;
	gServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(gStatusHandle, &gServiceStatus);
	return;
}

void ControlHandler(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP:
		SvcReportInfo(_T("Handler: stop"));

		gServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(gStatusHandle, &gServiceStatus);
		StopServer();
		return;

	case SERVICE_CONTROL_SHUTDOWN:
		SvcReportInfo(_T("Handler: shutdown"));

		gServiceStatus.dwWin32ExitCode = 0;
		gServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(gStatusHandle, &gServiceStatus);
		return;

	default:
		SvcReportInfo(_T("Handler: unknown"));
		break;
	}

	SetServiceStatus(gStatusHandle, &gServiceStatus);
	return;
}


/* 
 * Not work. CreateFile fails. Why?
 */
int InitService()
{
	HANDLE hFile;
	char DataBuffer[] = "This is some test data to write to the file.";
	DWORD dwBytesToWrite = (DWORD)strlen(DataBuffer);
	DWORD dwBytesWritten = 0;
	BOOL bErrorFlag = FALSE;
	_TCHAR *filePath = _T("C:\Dta\svc_tmp\svc.txt");

	hFile = CreateFile(filePath,                // name of the write
		GENERIC_WRITE,          // open for writing
		0,                      // do not share
		NULL,                   // default security
		CREATE_ALWAYS,             // create new file only
		FILE_ATTRIBUTE_NORMAL,  // normal file
		NULL);                  // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		TCHAR msg[80];
		_stprintf_s(msg, _T("IS: create error: %u"), GetLastError());
		SvcReportError(msg);
		return 1;
	}

	bErrorFlag = WriteFile(
		hFile,           // open file handle
		DataBuffer,      // start of data to write
		dwBytesToWrite,  // number of bytes to write
		&dwBytesWritten, // number of bytes that were written
		NULL);            // no overlapped structure

	if (FALSE == bErrorFlag)
	{
		SvcReportError(_T("IS: write error"));
	}
	else
	{
		if (dwBytesWritten != dwBytesToWrite)
		{
			// This is an error because a synchronous write that results in
			// success (WriteFile returns TRUE) should write all data as
			// requested. This would not necessarily be the case for
			// asynchronous writes.
			SvcReportError(_T("IS: write partial"));
		}
		else
		{
			_TCHAR msg[100];
			//swprintf_s(msg, _T("IS: write success: %d"), dwBytesWritten);
			SvcReportInfo(_T("IS: write success"));
		}
	}

	CloseHandle(hFile);
	return 0;
}

VOID SvcReportEvent(unsigned type, LPTSTR szFunction)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];

	hEventSource = RegisterEventSource(NULL, gServiceName);

	if (NULL != hEventSource)
	{
		_stprintf_s(Buffer, 80, TEXT("%s"), szFunction);

		lpszStrings[0] = gServiceName;
		lpszStrings[1] = Buffer;

		ReportEvent(hEventSource,        // event log handle
			type, // event type
			0,                   // event category
			0x40000103L,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}

VOID SvcPrintf(unsigned type, LPTSTR format, ...)
{
	va_list params;
	TCHAR message[320];
	va_start(params, format);

	_vstprintf_s(message, format, params);
	SvcReportEvent(type, message);
}

int InstallService()
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!hSCManager) {
		addLogMessage("Error: Can't open Service Control Manager");
		return -1;
	}

	SC_HANDLE hService = CreateService(
		hSCManager,
		gServiceName,
		gServiceName,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		gServicePath,
		NULL, NULL, NULL, NULL, NULL
		);

	if (!hService) {
		int err = GetLastError();
		switch (err) {
		case ERROR_ACCESS_DENIED:
			addLogMessage("Error: ERROR_ACCESS_DENIED");
			break;
		case ERROR_CIRCULAR_DEPENDENCY:
			addLogMessage("Error: ERROR_CIRCULAR_DEPENDENCY");
			break;
		case ERROR_DUPLICATE_SERVICE_NAME:
			addLogMessage("Error: ERROR_DUPLICATE_SERVICE_NAME");
			break;
		case ERROR_INVALID_HANDLE:
			addLogMessage("Error: ERROR_INVALID_HANDLE");
			break;
		case ERROR_INVALID_NAME:
			addLogMessage("Error: ERROR_INVALID_NAME");
			break;
		case ERROR_INVALID_PARAMETER:
			addLogMessage("Error: ERROR_INVALID_PARAMETER");
			break;
		case ERROR_INVALID_SERVICE_ACCOUNT:
			addLogMessage("Error: ERROR_INVALID_SERVICE_ACCOUNT");
			break;
		case ERROR_SERVICE_EXISTS:
			addLogMessage("Error: ERROR_SERVICE_EXISTS");
			break;
		default:
			addLogMessage("Error: Undefined");
		}
		CloseServiceHandle(hSCManager);
		return -1;
	}
	CloseServiceHandle(hService);

	CloseServiceHandle(hSCManager);
	addLogMessage("Success install service!\n");
	return 0;
}

int RemoveService()
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!hSCManager) {
		addLogMessage("Error: Can't open Service Control Manager\n");
		addLogMessage("Admin rights are necessary\n");
		return -1;
	}
	SC_HANDLE hService = OpenService(hSCManager, gServiceName, SERVICE_STOP | DELETE);
	if (!hService) {
		addLogMessage("Error: Can't open service\n");
		CloseServiceHandle(hSCManager);
		return -1;
	}

	DeleteService(hService);
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	addLogMessage("Success remove service!\n");
	return 0;
}

int StartServiceCmd()
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!hSCManager) {
		addLogMessage("Error: Can't open Service Control Manager\n");
		addLogMessage("Admin rights are necessary\n");
		return -1;
	}

	SC_HANDLE hService = OpenService(hSCManager, gServiceName, SERVICE_START);
	if (!hService) {
		addLogMessage("Error: Can't open service\n");
		addLogMessage("Service should be installed firstly\n");
		CloseServiceHandle(hSCManager);
		return -1;
	}

	if (!StartService(hService, 0, NULL)) {
		CloseServiceHandle(hSCManager);
		addLogMessage("Error: Can't start service");
		return -1;
	}

	addLogMessage("Success start service!\n");
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	return 0;
}


VOID StopService()
{
	SERVICE_STATUS_PROCESS ssp;
	// Get a handle to the SCM database. 
	SC_HANDLE schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 
	if (NULL == schSCManager)
	{
		addLogMessage("Error: Can't open Service Control Manager\n");
		addLogMessage("Admin rights are necessary\n");
		return;
	}

	// Get a handle to the service.
	SC_HANDLE schService = OpenService(
		schSCManager,         // SCM database 
		gServiceName,            // name of service 
		SERVICE_STOP |
		SERVICE_QUERY_STATUS |
		SERVICE_ENUMERATE_DEPENDENTS);
	if (schService == NULL)
	{
		addLogMessage("Error: Can't open service\n");
		addLogMessage("Service should be installed firstly\n");
		CloseServiceHandle(schSCManager);
		return;
	}

	// Send a stop code to the service.
	if (!ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp))
	{
		addLogMessage("ControlService failed\n");
		addLogMessage("Service may be not started\n");
		goto stop_cleanup;
	}
	addLogMessage("Success stop service!\n");

stop_cleanup:
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}
