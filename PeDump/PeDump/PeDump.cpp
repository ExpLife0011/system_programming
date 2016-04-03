// PeDump.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <windows.h>

#define ERR_RET(desc) do {	\
			perror(#desc);			\
			return 1;			\
		} while (0)

int PrintPeHeaders(FILE *fbin)
{
	char buf[128];
	int i;
	size_t offset;
	IMAGE_FILE_HEADER pe_head;
	IMAGE_SECTION_HEADER sect_head;

	if (fseek(fbin, 0x3c, SEEK_SET))
		ERR_RET("fseek");
	if (!fread(buf, 1, 4, fbin))
		ERR_RET("read offset");

	offset = *(size_t*)buf;
	if (fseek(fbin, offset, SEEK_SET))
		ERR_RET("fseek");
	if (!fread(buf, 1, 4, fbin))
		ERR_RET("read signature");
	printf("PE header magic: %c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);

	if (!fread(&pe_head, 1, sizeof(pe_head), fbin))
		ERR_RET("read pe header");
	printf("machine 0x%x, sections %u\n", pe_head.Machine, pe_head.NumberOfSections);

	if (fseek(fbin, pe_head.SizeOfOptionalHeader, SEEK_CUR))
		ERR_RET("fseek");
	for (i = 0; i < pe_head.NumberOfSections; i++){
		if (!fread(&sect_head, 1, sizeof(sect_head), fbin))
			ERR_RET("read section");
		printf("%-8.8s: rawsize %u\n", sect_head.Name, sect_head.SizeOfRawData);
	}

	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	FILE *fbin = NULL;

	if (argc != 2){
		fprintf(stderr, "Need path argument!\n");
		return 1;
	}

	fbin = _tfopen(argv[1], _T("r"));
	if (!fbin){
		perror("fopen");
		return 1;
	}

	if (PrintPeHeaders(fbin)){
		fprintf(stderr, "Parser error\n");
		return 1;
	}

	fclose(fbin);
	return 0;
}

