#include "stdafx.h"
#include "Mapper.h"

Mapper::Mapper() : m_hFile(0), m_hMapFile(0), m_lpMapAddress(NULL), m_isMaped(false)
{
}

bool Mapper::MapFile(TCHAR * fileName)
{
	if (m_isMaped) {
		_tprintf(TEXT("Mapping - the other file already mapped \n"));
		return false;
	}

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

	m_lpMapAddress = MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 0);
	if (NULL == m_lpMapAddress) {
		_tprintf(TEXT("Mapping - MapViewOfFile with error %d \n"), GetLastError());
		goto err2;
	}

	_tprintf(TEXT("Mapping succeeded \n"));
	m_isMaped = true;
	return true;

err2:
	CloseHandle(m_hMapFile);
err1:
	CloseHandle(m_hFile);
err0:
	return false;
}

bool Mapper::UnmapFile()
{
	UnmapViewOfFile(m_lpMapAddress);
	CloseHandle(m_hMapFile);
	CloseHandle(m_hFile);
	m_isMaped = false;
	return true;
}

LPVOID Mapper::GetMapAddress()
{
	return m_isMaped ? m_lpMapAddress : NULL;
}
