/*
 *	This file is a part of the source code of "byenow" program.
 *
 *	Copyright (c) 2020- Alexander Pankratov and IO Bureau SA.
 *	All rights reserved.
 *
 *	The source code is distributed under the terms of 2-clause 
 *	BSD license with the Commons Clause condition. See LICENSE
 *	file for details.
 */
#include "utils.h"

#include "libp/string_utils.h"
#include "libp/_system_api.h"

//
#define __KB (1024ULL)
#define __MB (1024ULL*1024)
#define __GB (1024ULL*1024*1024)
#define __TB (1024ULL*1024*1024*1024)
#define __PB (1024ULL*1024*1024*1024*1024)

//
string format_count(uint64_t val, const char * unit)
{
	return stringf("%I64u %s%s", val, unit, (val == 1) ? "" : "s");
}

string format_bytes(uint64_t bytes)
{
	if (bytes < 64*__KB) return stringf("%I64u B", bytes);
	if (bytes <  2*__MB) return stringf("%.1lf KB", bytes/(double)__KB);
	if (bytes <  2*__GB) return stringf("%.1lf MB", bytes/(double)__MB);
	if (bytes <  2*__TB) return stringf("%.1lf GB", bytes/(double)__GB);
	if (bytes <  2*__PB) return stringf("%.1lf TB", bytes/(double)__TB);

	return stringf("%.1lf PB", bytes/(double)__PB);
}

string format_usecs(uint64_t usecs)
{
	if (usecs <= 1000)
		return "1 ms";

	if (usecs < 1000*1000)
		return stringf("%I64u ms", usecs/1000);

//	if (usecs < 60*1000*1000)
	if (usecs < 10*1000*1000)
		return stringf("%.2lf sec", usecs/1000./1000.);

	size_t ms, sec, min, hr;
	
	usecs /= 1000; 
	ms  = usecs % 1000; usecs /= 1000; 
	sec = usecs % 60;   usecs /= 60;
	min = usecs % 60;   usecs /= 60;
	hr  = usecs;

	return stringf("%02zu:%02zu:%02zu.%03zu", hr, min, sec, ms);
}

//
template <class E>
void replace(std::basic_string<E> & str, const E * a, const E * b)
{
	typedef std::basic_string<E>::traits_type E_ops;

	size_t a_len = E_ops::length(a);
	size_t b_len = E_ops::length(b);
	size_t pos = 0;

	for (;;)
	{
		pos = str.find(a, pos);
		if (pos == -1)
			break;

		str.replace(pos, a_len, b, b_len);
		pos += b_len;
	}
}

/*
 *	https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55
 */
bool get_error_desc(dword code, wstring & mesg)
{
	HANDLE mod   = NULL;
	dword  flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER;
	dword  lang  = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
	bool   ntapi = (code & 0xF0000000);
	wchar_t * wstr;

	if (ntapi)
	{
		flags |= FORMAT_MESSAGE_FROM_HMODULE;
		mod = ntdll.mod;
	}

	if (! FormatMessageW(flags, mod, code, lang, (LPWSTR)&wstr, sizeof wstr, NULL))
		return false;

	mesg = wstr;
	LocalFree(wstr);

	// remove line breaks
	replace(mesg, L"\r\n", L"\n");
	replace(mesg, L" \n",  L"\n");
	replace(mesg, L"\n ",  L"\n");
	replace(mesg, L"\n",   L" ");

	if (mesg.size() && mesg.back() == L' ') mesg.pop_back();

	return true;
}

string error_to_str(const api_error & e)
{
	string mesg = e.func + "() " + failed_with(e.code);
	wstring foo;
		
	if (get_error_desc(e.code, foo))
		mesg += ". " + to_utf8(foo);

	return mesg;
}
