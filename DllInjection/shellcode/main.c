
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <winnt.h>
#include <stddef.h>

int* min2(int, int);

char ar[] = { 1, 2, 3 };

struct sc_data_t {
	char libName[16];
	void* ploadLibrary;
	UINT32 a, b, c;
	UINT32 reserved;
};

int _tmain(int argc, _TCHAR* argv[])
{
	int* p = min2(13, 3);
	_tprintf("%p, %x\n", p, *p);
	
	LoadLibraryA("keyiso.dll");
	struct sc_data_t sc_data;
	_tprintf("%d, %d\n", sizeof(struct sc_data_t), offsetof(struct sc_data_t, c));
	LoadLibrary("a");
	return 0;
}
