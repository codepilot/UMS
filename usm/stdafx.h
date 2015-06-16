// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include <Windows.h>

#define writeFileFormatted1(fmt, arg1) \
	{\
		CHAR dst[1000];\
		size_t charsWritten = sprintf_s(dst, fmt, arg1);\
		DWORD bytesWritten = 0;\
		BOOL ret = WriteFile(stdout_handle, dst, charsWritten * sizeof(CHAR), &bytesWritten, NULL);\
	}

#define writeFileCHARS(dst, charsWritten) \
	{\
		DWORD bytesWritten = 0;\
		BOOL ret = WriteFile(stdout_handle, dst, charsWritten * sizeof(CHAR), &bytesWritten, NULL);\
	}

// TODO: reference additional headers your program requires here
