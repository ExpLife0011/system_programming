// PeDump.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include "atlstr.h"
#include <windows.h>
#include "Mapper.h"

#define ERR_RET(desc)		\
		do {				\
			perror(desc);	\
			goto err;		\
		} while (0)

#define MAX_BUF_SIZE 128


size_t GetPEOffset(char* faddr)
{
	return *(int*)(faddr + 0x3c);
}

int FindPEMagic(char* faddr, size_t* offset)
{
	char buf[5] = { 0 };
	memcpy(buf, faddr + *offset, 4);
	*offset += 4;
	_tprintf(TEXT("PE header magic: %c%c%c%c => %s \n"), buf[0], buf[1], buf[2], buf[3],
		(strncmp(buf, "PE", 4)) ? TEXT("FAIL") : TEXT("OK"));
	return 0;
}

int FindPEHead(IMAGE_FILE_HEADER * hdr, char* faddr, size_t* offset)
{
	memcpy(hdr, faddr + *offset, sizeof(*hdr));
	*offset += sizeof(*hdr);
	_tprintf(TEXT("machine type: 0x%x => %s \n"), hdr->Machine,
		((hdr->Machine == 0x8664) ? TEXT("x64") : ((hdr->Machine == 0x14c) ? TEXT("x86") : TEXT("Unknown"))));
	_tprintf(TEXT("sections number: %u\n"), hdr->NumberOfSections);
	return 0;
}

int ReadSectionNumber(char* faddr, int num)
{
	IMAGE_SECTION_HEADER sect_head;
	memcpy(&sect_head, faddr + num * sizeof(sect_head), sizeof(sect_head));
	USES_CONVERSION;
	_tprintf(TEXT("%-8.8s -> rawsize: %u\n"), A2T((LPCSTR)sect_head.Name), sect_head.SizeOfRawData);
	return 0;
}

int PrintPEHeaders(char* faddr)
{
	size_t offset = GetPEOffset(faddr);
	if (!offset)
		ERR_RET("pe offset getting error");

	if (FindPEMagic(faddr, &offset))
		ERR_RET("finding pe magic error");

	IMAGE_FILE_HEADER pe_head;
	if (FindPEHead(&pe_head, faddr, &offset))
		ERR_RET("finding pe header error");

	offset += pe_head.SizeOfOptionalHeader;

	for (int i = 0; i < pe_head.NumberOfSections; i++) {
		if (ReadSectionNumber(faddr + offset, i))
			ERR_RET("finding section error");
	}

	return 0;
err:
	return -1;
}


int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2) {
		_tprintf(TEXT("Need path argument"));
		goto err;
	}

	Mapper map;
	if (!map.MapFile(argv[1])) {
		_tprintf(TEXT("Error mapping file"));
		goto err;
	}

	if (PrintPEHeaders(map.GetMapAddress())) {
		_ftprintf(stderr, TEXT("Parser error"));
	}

	map.UnmapFile();
err:
	getchar();
	return 0;
}
