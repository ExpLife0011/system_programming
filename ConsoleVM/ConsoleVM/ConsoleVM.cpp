// ConsoleVM.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>

int _tmain1(int argc, TCHAR *argv[]);
int __cdecl main2(int argc, char **argv);
int __cdecl main3(void);
void __cdecl mainS(int argc, TCHAR *argv[]);

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc == 2 && !strcmp(argv[1], "cli")) {
		_tprintf(TEXT("Client will be started \n"));
		return main2(argc, argv);
	}
	else if (argc == 2 && !strcmp(argv[1], "srv")) {
		_tprintf(TEXT("Server will be started \n"));
		return main3();
	}
	else if ((argc >= 2 ) && (!strcmp(argv[1], "srv_service"))) {
		if (argc >= 3) {
			_tprintf(TEXT("Service will be started \n"));
			mainS(argc, argv);
		} else {
			_tprintf(TEXT("Need more arguments!\n"));
		}
	}
	else if (argc == 1) {
		mainS(argc, argv);
 	}
	_tprintf(TEXT("End of startings \n"));
	//_tmain1(argc, argv);

	//getchar();
	return 0;
}

