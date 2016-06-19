#ifndef _MAPPER_H_
#define _MAPPER_H_

#include <windows.h>

class Mapper
{
public:
	Mapper(TCHAR* fileName);
	~Mapper();
	
	// defaults params for readonly mapping full file
	bool MapFile(DWORD accessType = FILE_MAP_READ,
				 DWORD offsetHigh = 0,
				 DWORD offsetLow = 0,
				 SIZE_T bytesNumber = 0);
	bool UnmapFile();
	char* GetMapAddress();

private:
	HANDLE m_hFile;
	HANDLE m_hMapFile;
	LPVOID m_lpMapAddress;
	bool m_isMapperInited;
	bool m_isMapped;
};

#endif // _MAPPER_H_