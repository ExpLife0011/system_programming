// ConsoleVM.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>

int _tmain1(int argc, TCHAR *argv[]);
int __cdecl ClientProcess();
void __cdecl mainS(int argc, TCHAR *argv[]);

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc == 2 && !_tcscmp(argv[1], _T("cli"))) {
		_tprintf(TEXT("Client will be started \n"));
		return ClientProcess();
	}
	else if (argc == 2 && !_tcscmp(argv[1], _T("srv"))) {
		_tprintf(TEXT("Deprecated. Use service!\n"));
		return 1;
	}
	else if ((argc >= 2 ) && (!_tcscmp(argv[1], _T("srv_service")))) {
		if (argc >= 3) {
			_tprintf(TEXT("Service will be started \n"));
			mainS(argc, argv);
		} else {
			_tprintf(TEXT("Need more arguments!\n"));
		}
	}
	else if (argc == 1) {
		mainS(argc, argv);
		// usage
 	}
	_tprintf(TEXT("End of startings \n"));
	//_tmain1(argc, argv);

	return 0;
}

