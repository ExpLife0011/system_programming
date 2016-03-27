
#pragma once

#include <tchar.h>
#include <windows.h>

#define SvcReportError(x, ...) SvcPrintf(EVENTLOG_ERROR_TYPE, (x), __VA_ARGS__)
#define SvcReportInfo(x, ...) SvcPrintf(EVENTLOG_WARNING_TYPE, (x), __VA_ARGS__)


VOID SvcPrintf(unsigned type, LPTSTR format, ...);
