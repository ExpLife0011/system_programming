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

#define UNW_FLAG_EHANDLER  0x01
#define UNW_FLAG_UHANDLER  0x02
#define UNW_FLAG_CHAININFO 0x04

typedef union _UNWIND_CODE {
	struct {
		BYTE CodeOffset;
		BYTE UnwindOp : 4;
		BYTE OpInfo : 4;
	};
	USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
	BYTE Version : 3;
	BYTE Flags : 5;
	BYTE SizeOfProlog;
	BYTE CountOfCodes;
	BYTE FrameRegister : 4;
	BYTE FrameOffset : 4;
	UNWIND_CODE UnwindCode[1];
	/*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
	*   union {
	*       OPTIONAL ULONG ExceptionHandler;
	*       OPTIONAL ULONG FunctionEntry;
	*   };
	*   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;

struct PEFileInfo {
	char* base;
	DWORD machine;
	char* sections;
	DWORD num_sect;
	DWORD importRVA;
	DWORD exportRVA;
	DWORD exceptionRVA;
	DWORD exceptionSize;
};


size_t GetPEOffset(char* faddr)
{
	return *(int*)(faddr + 0x3c);
}

int CheckPEMagic(char* faddr, size_t* offset)
{
	char buf[5] = { 0 };
	memcpy(buf, faddr + *offset, 4);
	*offset += 4;
	_tprintf(TEXT("PE header magic: %c%c%c%c => %s \n"), buf[0], buf[1], buf[2], buf[3],
		(memcmp(buf, "PE\0\0", 4)) ? TEXT("FAIL") : TEXT("OK"));
	return 0;
}

int CheckPEHead(IMAGE_FILE_HEADER * hdr, char* faddr, size_t* offset, struct PEFileInfo* peinfo)
{
	memcpy(hdr, faddr + *offset, sizeof(*hdr));
	*offset += sizeof(*hdr);
	_tprintf(TEXT("machine type: 0x%x => %s \n"), hdr->Machine,
		((hdr->Machine == IMAGE_FILE_MACHINE_AMD64) ? TEXT("x64") : ((hdr->Machine == IMAGE_FILE_MACHINE_I386) ? TEXT("x86") : TEXT("Unknown"))));
	peinfo->machine = hdr->Machine;

	_tprintf(TEXT("sections number: %u\n"), hdr->NumberOfSections);
	return 0;
}

int ReadSectionByNumber(char* faddr, int num, struct PEFileInfo* peinfo)
{
	IMAGE_SECTION_HEADER sect_head;
	memcpy(&sect_head, faddr + num * sizeof(sect_head), sizeof(sect_head));
	
	USES_CONVERSION;
	_tprintf(TEXT("%-8.8s -> rawsize: %u\n"), A2T((LPCSTR)sect_head.Name), sect_head.SizeOfRawData);
	return 0;
}

int PrintSections(char* faddr, struct PEFileInfo* peinfo)
{
	size_t offset = GetPEOffset(faddr);
	if (!offset)
		ERR_RET("pe offset getting error");

	if (CheckPEMagic(faddr, &offset))
		ERR_RET("checking pe magic error");

	IMAGE_FILE_HEADER pe_head;
	if (CheckPEHead(&pe_head, faddr, &offset, peinfo))
		ERR_RET("checking pe header error");
	
	IMAGE_DATA_DIRECTORY* pdir;
	int numberDir;
	if (peinfo->machine == IMAGE_FILE_MACHINE_AMD64) {
		IMAGE_OPTIONAL_HEADER64 opt_head;
		memcpy(&opt_head, faddr + offset, sizeof(opt_head));
		pdir = opt_head.DataDirectory;
		numberDir = opt_head.NumberOfRvaAndSizes;
	} else {
		IMAGE_OPTIONAL_HEADER32 opt_head;
		memcpy(&opt_head, faddr + offset, sizeof(opt_head));
		pdir = opt_head.DataDirectory;
		numberDir = opt_head.NumberOfRvaAndSizes;
	}
	peinfo->importRVA = pdir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	peinfo->exportRVA = pdir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if (numberDir >= IMAGE_DIRECTORY_ENTRY_EXCEPTION) {
		peinfo->exceptionSize = pdir[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
		peinfo->exceptionRVA = pdir[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
	}

	offset += pe_head.SizeOfOptionalHeader;
	peinfo->sections = faddr + offset;
	peinfo->num_sect = pe_head.NumberOfSections;

	for (int i = 0; i < pe_head.NumberOfSections; i++) {
		if (ReadSectionByNumber(peinfo->sections, i, peinfo))
			ERR_RET("finding section error");
	}

	return 0;
err:
	return -1;
}

char* ConvertRVA(struct PEFileInfo* peinfo, DWORD rvaddr)
{
	IMAGE_SECTION_HEADER *phead;
	int offset;
	for (int i = 0; i < peinfo->num_sect; i++){
		phead = (IMAGE_SECTION_HEADER*)(peinfo->sections + i * sizeof(*phead));
		offset = rvaddr - phead->VirtualAddress;
		if (offset >= 0 && offset < phead->SizeOfRawData){
			return peinfo->base + phead->PointerToRawData + offset;
		}
	}
	return NULL;
}

int PrintImports(struct PEFileInfo* peinfo)
{
	union THUNK_DATAXX {
		ULONGLONG u64;
		UINT u32;
	} *pthunk;
	IMAGE_IMPORT_DESCRIPTOR desc;
	IMAGE_IMPORT_BY_NAME *pname;
	ULONG64 sz = peinfo->machine == IMAGE_FILE_MACHINE_AMD64 ? 8 : 4;
	char *pentry = ConvertRVA(peinfo, peinfo->importRVA);
	int i = 0;
	USES_CONVERSION;

	while (1){
		memcpy(&desc, pentry, sizeof(desc));
		if (desc.Characteristics == 0)
			break;
		_tprintf(_T("dll name: %s\n"), A2T(ConvertRVA(peinfo, desc.Name)));

		i = 0;
		while (1){
			pthunk = (THUNK_DATAXX*)ConvertRVA(peinfo, desc.OriginalFirstThunk + i * sz);
			if (pthunk->u32 == 0)
				break;
			if ((pthunk->u64 & (1ULL << (sz * 8 - 1)))) //Check for ordinal bit
				_tprintf(_T("  function: %d\n"), pthunk->u32 & 0xffff);
			else {
				pname = (IMAGE_IMPORT_BY_NAME*)ConvertRVA(peinfo, pthunk->u32);
				_tprintf(_T("  function: %s\n"), A2T(pname->Name));
			}
			i++;
		}
		pentry += sizeof(desc);
	}

	return 0;
}

int PrintExports(struct PEFileInfo* peinfo)
{
	IMAGE_EXPORT_DIRECTORY *pdir = (IMAGE_EXPORT_DIRECTORY*)ConvertRVA(peinfo, peinfo->exportRVA);
	int i = 0;
	USES_CONVERSION;
	DWORD *nameTable, *funcTable;
	WORD *ordinalTable;

	_tprintf(_T("dll name: %s, functions: %d, names: %d\n"), A2T(ConvertRVA(peinfo, pdir->Name)),
		pdir->NumberOfFunctions, pdir->NumberOfNames);

	nameTable = (DWORD*)ConvertRVA(peinfo, pdir->AddressOfNames);
	funcTable = (DWORD*)ConvertRVA(peinfo, pdir->AddressOfFunctions);
	ordinalTable = (WORD*)ConvertRVA(peinfo, pdir->AddressOfNameOrdinals);
	for (i = 0; i < pdir->NumberOfNames; i++){
		_tprintf(_T("  %s: ordinal %d, rva 0x%x\n"), A2T(ConvertRVA(peinfo, nameTable[i])), 
			ordinalTable[i] + pdir->Base, funcTable[ordinalTable[i]]);
	}

	return 0;
}

int PrintExceptions(struct PEFileInfo* peinfo)
{
	RUNTIME_FUNCTION *pdir = (RUNTIME_FUNCTION*)ConvertRVA(peinfo, peinfo->exceptionRVA);
	ULONG64 sz = peinfo->machine == IMAGE_FILE_MACHINE_AMD64 ? 8 : 4;
	int i = 0;
	USES_CONVERSION;

	for (i = 0; i < peinfo->exceptionSize / sizeof(RUNTIME_FUNCTION); i++){
		UNWIND_INFO *uinfo = (UNWIND_INFO*)ConvertRVA(peinfo, (pdir++)->UnwindInfoAddress);
		if (uinfo->Flags == UNW_FLAG_EHANDLER){
			ULONG handler = *(ULONG*)&uinfo->UnwindCode[((uinfo->CountOfCodes + 1) & ~1)];
			_tprintf(_T("func (0x%x-0x%x): 0x%x\n"), pdir->BeginAddress, pdir->EndAddress, handler);
		}
	}

	return 0;
}


int PrintFunctionTables(char* faddr, struct PEFileInfo* peinfo)
{
	if (peinfo->importRVA){
		_tprintf(_T("===Import dump===\n"));
		if (PrintImports(peinfo))
			ERR_RET("Import dump error");
		_tprintf(_T("===End import dump===\n"));
	}
	if (peinfo->exportRVA){
		_tprintf(_T("===Export dump===\n"));
		if (PrintExports(peinfo))
			ERR_RET("Export dump error");
		_tprintf(_T("===End export dump===\n"));
	}
	if (peinfo->exceptionRVA){
		_tprintf(_T("===Exception dump===\n"));
		if (PrintExceptions(peinfo))
			ERR_RET("Exception dump error");
		_tprintf(_T("===End exception dump===\n"));
	}

	return 0;
err:
	return -1;
}


int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2) {
		_tprintf(TEXT("Need path argument"));
		return 0;
	}

	struct PEFileInfo peinfo;
	ZeroMemory(&peinfo, sizeof(peinfo));

	Mapper map(argv[1]);
	if (!map.MapFile()) {
		_tprintf(TEXT("Error mapping file \n"));
		goto err;
	}
	peinfo.base = map.GetMapAddress();

	if (PrintSections(map.GetMapAddress(), &peinfo)) {
		_ftprintf(stderr, TEXT("Parser error"));
	}

	if (PrintFunctionTables(map.GetMapAddress(), &peinfo)) {
		_ftprintf(stderr, TEXT("Print tables error"));
	}

	map.UnmapFile();
err:
	getchar();
	return 0;
}
