#include "stdafx.h"
#include "Mapper.h"

Mapper::Mapper(TCHAR* fileName) : 
	m_hFile(0), 
	m_hMapFile(0), 
	m_lpMapAddress(NULL), 
	m_isMapperInited(false),
	m_isMapped(false)
{
	m_hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (INVALID_HANDLE_VALUE == m_hFile) {
		_tprintf(TEXT("Mapping - CreateFile with error %d \n"), GetLastError());
		goto err0;
	}

	m_hMapFile = CreateFileMapping(m_hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (INVALID_HANDLE_VALUE == m_hMapFile) {
		_tprintf(TEXT("Mapping - CreateFileMapping with error %d \n"), GetLastError());
		goto err1;
	}

	m_isMapperInited = true;
	return;

err1:
	CloseHandle(m_hFile);
err0:
	return;
}

Mapper::~Mapper()
{
	if (!m_isMapperInited)
		return;

	UnmapFile();
	CloseHandle(m_hMapFile);
	CloseHandle(m_hFile);
}

bool Mapper::MapFile(DWORD accessType, DWORD offsetHigh,
					 DWORD offsetLow, SIZE_T bytesNumber)
{
	if (!m_isMapperInited) {
		_tprintf(TEXT("Mapping - mapper not inited \n"));
		return false;
	}

	if (m_isMapped) {
		_tprintf(TEXT("Mapping - some part is already mapped \n"));
		return false;
	}

	m_lpMapAddress = MapViewOfFile(m_hMapFile, accessType, offsetHigh, offsetLow, bytesNumber);
	if (NULL == m_lpMapAddress) {
		_tprintf(TEXT("Mapping - MapViewOfFile with error %d \n"), GetLastError());
		goto err;
	}

	_tprintf(TEXT("Mapping succeeded \n"));
	m_isMapped = true;
	return true;

err:
	return false;
}

bool Mapper::UnmapFile()
{
	if (!m_isMapped)
		return true;

	UnmapViewOfFile(m_lpMapAddress);
	m_isMapped = false;
	return true;
}

char* Mapper::GetMapAddress()
{
	return m_isMapped ? (char*)m_lpMapAddress : NULL;
}
