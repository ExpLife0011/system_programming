// PeDump.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <windows.h>

#define PRINTERR(str, ...) fprintf(stderr, str, __VA_ARGS__)


int _tmain(int argc, _TCHAR* argv[])
{
	FILE *fbin = NULL;
	TCHAR *path;
	char buf[128];
	int i;

	if (argc != 2){
		PRINTERR("Need path argument!\n");
		return 1;
	}
	path = argv[1];

	fbin = _tfopen(path, _T("r"));
	if (!fbin){
		perror("fopen");
		return 1;
	}

	fseek(fbin, 0x3c, SEEK_SET);
	fread(buf, 1, 4, fbin);
	size_t offset = *(size_t*)buf;
	fseek(fbin, offset, SEEK_SET);
	fread(buf, 1, 4, fbin);
	printf("PE header magic: %c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);

	IMAGE_FILE_HEADER pe_head;
	fread(&pe_head, 1, sizeof(pe_head), fbin);
	printf("machine 0x%x, sections %u\n", pe_head.Machine, pe_head.NumberOfSections);

	IMAGE_SECTION_HEADER sect_head;
	fseek(fbin, pe_head.SizeOfOptionalHeader, SEEK_CUR);
	for (i = 0; i < pe_head.NumberOfSections; i++){
		fread(&sect_head, 1, sizeof(sect_head), fbin);
		printf("%-8.8s: rawsize %u\n", sect_head.Name, sect_head.SizeOfRawData);
	}

	fclose(fbin);
	return 0;
}

