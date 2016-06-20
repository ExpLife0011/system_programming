
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
	UINT8* p = min2;
	int offset = ((int*)(&p[1]))[0];
	p = p + offset + 5;
	do {
		printf("0x%x, ", *p);
		p++;
	} while (*p != 0xC3);
	do {
		printf("0x%x, ", *p);
		p++;
	} while (*p != 0xC3);
	printf("0x%x, ", *p);

	printf("\n");
	getchar();
	LoadLibraryA("keyiso.dll");
	return 0;
}
