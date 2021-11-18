/*
 *	This file is a part of the source code of "byenow" program.
 *
 *	Copyright (c) 2020 Alexander Pankratov and IO Bureau SA.
 *	All rights reserved.
 *
 *	The source code is distributed under the terms of 2-clause 
 *	BSD license with the Commons Clause condition. See LICENSE
 *	file for details.
 */
#include "libp/_windows.h"
#include "libp/enforce.h"

/*
 *
 */
#define __try_cpp_exceptions__    try
#define __catch_cpp_exceptions__  catch ( std::exception & ) { printf("\nWhoops - std::exception\n"); }

//
#define __try_seh_exceptions__    __try
#define __catch_seh_exceptions__  __except ( EXCEPTION_EXECUTE_HANDLER ) { printf("\nWhoops - seh::exception\n"); }

//
void on_assert(const char * exp, const char * file, const char * func, int line)
{
	printf("\nWhoops - assertion failed - line %d\n", line);
	exit(1);
}

/*
 *
 */
int wmain_app(int argc, wchar_t ** argv);

int wmain_seh(int argc, wchar_t ** argv)
{
	int r = 3; // RC_whoops_seh

	__try_seh_exceptions__
	{
		r = wmain_app(argc, argv);
	}
	__catch_seh_exceptions__

	return r;
}

int wmain(int argc, wchar_t ** argv)
{
	int r = 4; // RC_whoops_cpp

	__try_cpp_exceptions__
	{
		r = wmain_seh(argc, argv);
	}
	__catch_cpp_exceptions__

	return r;
}

//
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
