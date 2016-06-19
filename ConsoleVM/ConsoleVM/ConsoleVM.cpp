// ConsoleVM.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>

int __cdecl ClientProcess();
int __cdecl mainS(int argc, TCHAR *argv[]);

void PrintUsage(TCHAR *name)
{
	_tprintf(_T("Usage:\n\
%s cli\n\
%s srv_service <command>\n"), name, name);
	_tprintf(_T("\t<command> could be:\n\
\tinstall <exe_full_path>\n\
\tremove\n\
\tstart\n\
\tstop\n"));
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc == 2 && !_tcscmp(argv[1], _T("--help")))
		PrintUsage(argv[0]);
	else if (argc == 2 && !_tcscmp(argv[1], _T("cli"))) {
		_tprintf(TEXT("Client will be started \n"));
		return ClientProcess();
	}
	else if (argc == 2 && !_tcscmp(argv[1], _T("srv"))) {
		_tprintf(TEXT("Deprecated. Use service!\n"));
		PrintUsage(argv[0]);
		return 1;
	}
	else if ((argc >= 2 ) && (!_tcscmp(argv[1], _T("srv_service")))) {
		if (argc >= 3) {
			if (mainS(argc, argv))
				PrintUsage(argv[0]);
		} else {
			_tprintf(TEXT("Need more arguments!\n"));
			PrintUsage(argv[0]);
		}
	}
	else if (argc == 1) {
		mainS(argc, argv);
	}
	else
		PrintUsage(argv[0]);

	return 0;
}

