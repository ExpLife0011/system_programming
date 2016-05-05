#ifndef _MAPPER_H_
#define _MAPPER_H_

#include <windows.h>

class Mapper
{
public:
	Mapper();
	
	bool MapFile(TCHAR * fileName);
	bool UnmapFile();
	char* GetMapAddress();

private:
	HANDLE m_hFile;
	HANDLE m_hMapFile;
	LPVOID m_lpMapAddress;
	bool m_isMaped;
};

#endif // _MAPPER_H_