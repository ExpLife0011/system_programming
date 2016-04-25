// PeDump.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include "atlstr.h"
#include <windows.h>

#define ERR_RET(desc)		\
		do {				\
			perror(desc);	\
			goto err;		\
		} while (0)

#define MAX_BUF_SIZE 128


size_t GetPEOffset(FILE * fbin)
{
	char buf[MAX_BUF_SIZE];

	if (fseek(fbin, 0x3c, SEEK_SET))
		ERR_RET("fseek error");
	if (!fread(buf, 1, 4, fbin))
		ERR_RET("read offset error");

	return *(size_t*)buf;
err:
	return 0;
}

int FindPEMagic(FILE * fbin, size_t offset)
{
	char buf[MAX_BUF_SIZE];

	if (fseek(fbin, offset, SEEK_SET))
		ERR_RET("fseek");
	if (!fread(buf, 1, 4, fbin))
		ERR_RET("read signature");

	_tprintf(TEXT("PE header magic: %c%c%c%c => %s \n"), buf[0], buf[1], buf[2], buf[3],
		(strncmp(buf, "PE", 4)) ? TEXT("FAIL") : TEXT("OK"));
	return 0;
err:
	return -1;
}

int FindPEHead(IMAGE_FILE_HEADER * hdr, FILE * fbin, size_t offset)
{
	if (!fread(hdr, sizeof(*hdr), 1, fbin))
		ERR_RET("read pe header");

	_tprintf(TEXT("machine type: 0x%x => %s \n"), hdr->Machine,
		((hdr->Machine == 0x8664) ? TEXT("x64") : ((hdr->Machine == 0x14c) ? TEXT("x86") : TEXT("Unknown"))));
	_tprintf(TEXT("sections number: %u\n"), hdr->NumberOfSections);
	return 0;
err:
	return -1;
}

int FindSection(FILE * fbin)
{
	IMAGE_SECTION_HEADER sect_head;
	if (!fread(&sect_head, sizeof(sect_head), 1, fbin))
		ERR_RET("read section error");

	USES_CONVERSION;
	_tprintf(TEXT("%-8.8s -> rawsize: %u\n"), A2T((LPCSTR)sect_head.Name), sect_head.SizeOfRawData);
	return 0;
err:
	return -1;
}

int PrintPEHeaders(FILE * fbin)
{
	size_t offset = GetPEOffset(fbin);
	if (!offset)
		ERR_RET("pe offset getting error");

	if (FindPEMagic(fbin, offset))
		ERR_RET("finding pe magic error");

	IMAGE_FILE_HEADER pe_head;
	if (FindPEHead(&pe_head, fbin, offset))
		ERR_RET("finding pe header error");

	if (fseek(fbin, pe_head.SizeOfOptionalHeader, SEEK_CUR))
		ERR_RET("fseeking optional header error");

	for (int i = 0; i < pe_head.NumberOfSections; i++) {
		if (FindSection(fbin))
			ERR_RET("finding section error");
	}

	return 0;
err:
	return -1;
}


int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2)
		ERR_RET("Need path argument");

	FILE *fbin = NULL;
	errno_t err = _tfopen_s(&fbin, argv[1], _T("r"));
	if (err != 0)
		ERR_RET("Error opening file");

	if (PrintPEHeaders(fbin)) {
		_ftprintf(stderr, TEXT("Parser error"));
	}

	fclose(fbin);
err:
	return 0;
}
