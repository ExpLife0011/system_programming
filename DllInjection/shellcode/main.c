
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <winnt.h>

int min2(int, int);

int _tmain(int argc, _TCHAR* argv[])
{
	_tprintf("%d\n", min2(13, 3));

	return 0;
}