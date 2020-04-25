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
#include "libp/types.h"
#include "libp/stringf.h"
#include "libp/assert.h"
#include "libp/atomic.h"
#include "libp/_windows.h"
#include "libp/_wstring.h"
#include "libp/_elpify.h"
#include "libp/_console.h"
#include "libp/_filesys.h"
#include "libp/_ext_system_api.h"
#include "libp/time.h"

#include "ultra_machine.h"
#include "utils.h"

//
#define HEADER  "Faster folder deleter, ver 0.3, freeware, https://iobureau.com/byenow\n"
#define SYNTAX  "Syntax: byenow.exe [options] <folder>\n" \
                "\n" \
                "  Deletes a folder. Similar to 'rmdir /s ...', but multi-threaded.\n" \
                "\n" \
                "  -p --preview           enumerate contents, but don\'t delete anything\n" \
                "  -s --staged            enumerate contents first, then delete them\n" \
                "\n" \
                "  -1 --one-liner         show progress as a single line\n" \
                "  -e --list-errors       list all errors upon completion\n" \
                "  -y --yes               don't ask to confirm the deletion\n" \
                "\n" \
                "  -t --thread <count>    use specified number of threads\n" \
                "  -n --delete-ntapi      use NtDeleteFile to remove files\n" \
                "\n" \
                "  * By default the thread count is set to the number of CPU cores.\n" \
                "    For local folders it doesn't make sense to go above that, but\n" \
                "    for folders on network shares raising the thread count may be\n" \
                "    a good thing to try, especially for high-latency connections.\n"

//
enum EXIT_CODES
{
	RC_ok               = 0,
	RC_cancelled        = 1,
	RC_whoops_seh       = 2,       // see wmain_seh()
	RC_whoops_cpp       = 3,       // see wmain()
	RC_unlikely         = 4,
	RC_ok_with_errors   = 10,      // ... + log10(error_count)

	// init errors
	RC_no_path          = 50,
	RC_invalid_arg      = 51,
	RC_not_confirmed    = 52,

	// path errors
	RC_path_not_found   = 60,
	RC_path_is_file     = 61,
	RC_path_is_root     = 62,
	RC_path_restricted  = 63,
	RC_path_cant_expand = 64,
	RC_path_cant_check  = 65,
};

//
struct context : ultra_mach_cb
{
	// config

	wstring          path;

	bool             preview;      // scan only
	bool             staged;       // scan, then delete
	bool             confirm;      // confirm delete

	ultra_mach_conf  mach_conf;
	bool             cryptic;
	bool             list_errors;

	// state

	bool             interactive;
	bool             enough;

	folder           root;
	dword            path_attrs;
	api_error_vec    scanner_err;
	api_error_vec    deleter_err;
	usec_t           started;
	usec_t           finished;
	usec_t           reported;

	uint             mode;         // 0x01 - scanning, 0x02 - deleting
	ultra_mach_info  info;

	uint             exit_rc;

	//
	static context * self;

	//
	context();

	void init();
	void parse_args(int argc, wchar_t ** argv);
	void parse_uint(int argc, wchar_t ** argv, size_t & next, size_t & val);

	void syntax(int rc);
	void abort(int rc, const char * format, ...);
	void confirm_it();

	//
	bool on_ultra_mach_tick(const ultra_mach_info & info); // ultra_mach_cb
	void init_progress();
	void update_progress();

	void check_path();
	void process();

	void report();
	void report_errors();

	//
	static BOOL __stdcall on_console_event_proxy(dword type);
	BOOL on_console_event(dword type);
};

//
context * context::self = NULL;

//
context::context()
{ 
	preview = false;
	staged  = false;
	confirm = true;

	cryptic = false;
	list_errors = false;

	interactive = false;
	enough = false;
	path_attrs = 0;

	started.raw = 0;
	finished.raw = 0;
	reported = usec();

	mode = 0x00;

	exit_rc = RC_ok;
}

void context::init()
{
	context::self = this;

	init_ext_system_api(NULL);

	if (! (ntdll.NtDeleteFile && ntdll.RtlInitUnicodeString && ntdll.RtlDosPathNameToNtPathName_U) ||
	    ! (ntdll.NtQueryDirectoryFile))
	{
		abort(RC_unlikely, "Failed to locate required NT API entry points.\n");
	}

	interactive = ! is_interactive_console();

	if (interactive)
	{
		SetConsoleCtrlHandler(on_console_event_proxy, TRUE);
		show_console_cursor(false);
	}
}

void context::parse_args(int argc, wchar_t ** argv)
{
	for (size_t i=1; i < (size_t)argc; i++)
	{
		const wchar_t * arg = argv[i];

		if (! wcscmp(arg, L"-p") || ! wcscmp(arg, L"--preview"))
		{
			preview = true;
			continue;
		}

		if (! wcscmp(arg, L"-s") || ! wcscmp(arg, L"--staged"))
		{
			staged = true;
			continue;
		}

		if (! wcscmp(arg, L"-y") || ! wcscmp(arg, L"--yes"))
		{
			confirm = false;
			continue;
		}

		if (! wcscmp(arg, L"-1") || ! wcscmp(arg, L"--one-liner"))
		{
			cryptic = true;
			continue;
		}

		if (! wcscmp(arg, L"-e") || ! wcscmp(arg, L"--list-errors"))
		{
			list_errors = true;
			continue;
		}

		if (! wcscmp(arg, L"-t") || ! wcscmp(arg, L"--threads"))
		{
			parse_uint(argc, argv, i, mach_conf.threads);
			continue;
		}

		if (! wcscmp(arg, L"--scan-buf-kb"))
		{
			parse_uint(argc, argv, i, mach_conf.scanner_buf_size);
			
			if (mach_conf.scanner_buf_size > 64*1024)
				abort(RC_invalid_arg, "Maximum supported scan buffer size is 64MB.");

			mach_conf.scanner_buf_size *= 1024;
			continue;
		}

		if (! wcscmp(arg, L"-n") || ! wcscmp(arg, L"--delete-ntapi"))
		{
			mach_conf.deleter_ntapi = true;
			continue;
		}

		if (! wcscmp(arg, L"--delete-batch"))
		{
			parse_uint(argc, argv, i, mach_conf.deleter_batch);
			continue;
		}

		if (arg[0] == L'-' || arg[0] == L'/')
			syntax(RC_invalid_arg);

		// otherwise it's a path

		if (path.size())
			syntax(RC_invalid_arg);

		path = arg;
	}

	if (path.empty())
		syntax(RC_no_path);

	if (path.size() == 2 && path[1] == L':' ||
	    path.size() == 3 && path[1] == L':' && path[2] == L'\\')
	{
		abort(RC_path_is_root, "Root of a drive is not supported as a target.");
	}

	if (path.back() == L'\\')
		path.pop_back();

	//
	if (_wcsnicmp(path.c_str(), L"C:\\Windows", 10) == 0 ||
	    _wcsnicmp(path.c_str(), L"C:\\Users", 8) == 0)
	{
		abort(RC_path_restricted, "Restricted path - %S\n", path.c_str());
	}
}

void context::parse_uint(int argc, wchar_t ** argv, size_t & i, size_t & val)
{
	if (++i == argc)
		syntax(RC_invalid_arg);

	if (! swscanf(argv[i], L"%zu", &val))
		syntax(RC_invalid_arg);
}

//
void context::syntax(int rc)
{
	printf(HEADER);
	printf(SYNTAX);
	exit(rc);
}

void context::abort(int rc, const char * format, ...)
{
	va_list m;
	printf(HEADER);
	va_start(m, format);
	vprintf(format, m);
	va_end(m);
	exit(rc);
}

void context::confirm_it()
{
	static const char * yes[] = { "y", "yes", "yep", "yup" };
	char line[32];

	if (preview || ! confirm)
		return;

	printf("Remove [%S] and all its contents? ", path.c_str());
	fflush(stdout);

	if (! gets_s(line, sizeof line))
		exit(RC_not_confirmed);

	for (auto & str : yes)
		if (! strcmp(line, str))
			return;

	exit(RC_not_confirmed);
}

//
BOOL __stdcall context::on_console_event_proxy(dword type)
{
	assert(context::self);
	return context::self->on_console_event(type);
}

BOOL context::on_console_event(dword type)
{
	if (type == CTRL_C_EVENT)
	{
		printf("Ctrl-C\n");
		enough = true;
		return TRUE;
	}

	if (type == CTRL_CLOSE_EVENT)
		return TRUE; // terminate

//	if (type == CTRL_BREAK_EVENT)
//		// dump stats, return FALSE

	// CTRL_LOGOFF_EVENT, CTRL_SHUTDOWN_EVENT
	return FALSE; // terminate
}

/*
 *
 */
bool context::on_ultra_mach_tick(const ultra_mach_info & _info)
{
	if (enough)
		return false;

	if (mode == 0x01 || // scan
	    mode == 0x03)   // scan & delete
	{
		info = _info;
	}
	else
	if (mode == 0x02) // delete-after-scan
	{
		info.f_deleted = _info.f_deleted;
		info.d_deleted = _info.d_deleted;
		info.done      = _info.done;
	}	
	else
	{
		assert(false);
	}

	if (_info.scanner_err) append(scanner_err, *_info.scanner_err);
	if (_info.deleter_err) append(deleter_err, *_info.deleter_err);

	if (interactive)
		update_progress();

	return true;
}

void context::init_progress()
{
	if (! cryptic)
	{
		printf("%s [%S] %s\n", preview ? "Scanning" : "Deleting", path.c_str(), (staged && ! preview) ? "[staged]" : "");
		printf("\n");
		printf("           %10s  %10s  %10s\n", "Folders", "Files", "Errors");
	}

	if (! interactive)
		return;

	if (! cryptic)
	{
		printf("  Found    %10s  %10s  %10s\n", "-", "-", "-");
		printf("  Deleted  %10s  %10s  %10s\n", "-", "-", "-");
	}
	else
	{
		printf("\n");
	}
}

void context::update_progress()
{
	usec_t now = usec();

	if (now - reported < 100*1000 && ! info.done)
		return;

	/*
	 *	one-liner
	 *
	 * 	%u / %u folders, %u / %u files, %u / %u errors
	 */
	if (cryptic)
	{
		move_console_cursor(0, false, -1, true);
		printf("%zu / %zu folders, %zu / %zu files, %zu / %zu errors", 
			info.d_found, info.d_deleted,
			info.f_found, info.f_deleted,
			scanner_err.size(), deleter_err.size());

		if (info.folders_togo) printf(" - %zu to go", info.folders_togo);

		wipe_console_line();
		printf("\n");
	}
	else
	{
		move_console_cursor(0, false, -2, true);

		printf("  Found    %10zu  %10zu  %10zu", info.d_found, info.f_found, scanner_err.size());
		if (info.folders_togo) printf("    [%zu to go]", info.folders_togo);

		wipe_console_line();
		printf("\n");

		if (! preview)
		{
			printf("  Deleted  %10zu  %10zu  %10zu", info.d_deleted, info.f_deleted, deleter_err.size());
			wipe_console_line();
		}

		printf("\n");
	}

	reported = now;
}

//
void context::check_path()
{
	wstring full;

	if (! get_full_pathname(path, full))
	{
		printf("Error: failed to get full path name for [%S].\n", path.c_str());
		exit(RC_path_cant_expand);
	}

	path = full;

	//
	path_attrs = elp->GetFileAttributes(path.c_str());

	if (path_attrs == -1)
	{
		api_error e;

		e.code = GetLastError();
		if (__not_found(e.code))
		{
			printf("Error: specified path not found - [%S].\n", path.c_str());
			exit(RC_path_not_found);
		}

		e.func = "GetFileAttributes";
		printf("Error: %s\n", error_to_str(e).c_str());
		printf("Path: [%S]\n", path.c_str());
		exit(RC_path_cant_check);
	}

	if (! (path_attrs & FILE_ATTRIBUTE_DIRECTORY))
	{
		printf("Error: specified path points at a file - [%S]\n", path.c_str());
		exit(RC_path_is_file);
	}
}

void context::process()
{
	folder root;

	started = usec();
	root.self.name = path;
	root.self.info.attrs = path_attrs;

	init_progress();

	if (preview)
	{
		mode = 0x01;

		if (! ultra_mach_scan(root, mach_conf, this))
			exit(enough ? RC_unlikely : RC_cancelled);
	}
	else
	if (staged)
	{
		mode = 0x01;
		if (! ultra_mach_scan(root, mach_conf, this))
			exit(enough ? RC_unlikely : RC_cancelled);

		mode = 0x02;
		if (! ultra_mach_delete(root, true, mach_conf, this)) // prescanned
			exit(enough ? RC_unlikely : RC_cancelled);
	}
	else
	{
		mode = 0x03;

		if (! ultra_mach_delete(root, false, mach_conf, this)) // scan & delete
			exit(enough ? RC_unlikely : RC_cancelled);
	}

	finished = usec();
}

void context::report()
{
	string elapsed   = format_usecs(finished - started);
	size_t err_count = scanner_err.size() + deleter_err.size();

	if (interactive)
	{
		if (cryptic)
		{
			move_console_cursor(0, false, -1, true);
			printf("%zu / %zu folders, %zu / %zu files, %zu / %zu errors, done in %s\n", 
				info.d_found, info.d_deleted,
				info.f_found, info.f_deleted,
				scanner_err.size(), deleter_err.size(),
				elapsed.c_str());
		}
		else
		{
			printf("\n");
			if (err_count && ! list_errors)
				printf("Completed in %s. To list errors use '--list-errors'.\n", elapsed.c_str());
			else
				printf("Completed in %s.\n", elapsed.c_str());
		}
	}
	else
	{
		if (cryptic)
		{
			printf("%zu / %zu folders, %zu / %zu files, %zu / %zu errors, done in %s\n", 
				info.d_found, info.d_deleted,
				info.f_found, info.f_deleted,
				scanner_err.size(), deleter_err.size(),
				elapsed.c_str());
		}
		else
		{
			printf("  Found    %10zu  %10zu  %10zu\n", info.d_found, info.f_found, scanner_err.size());
			printf("  Deleted  %10zu  %10zu  %10zu\n", info.d_deleted, info.f_deleted, deleter_err.size());
			printf("\n");
			printf("Completed in %s.\n", elapsed.c_str());
		}
	}

	if (err_count)
	{
		if (list_errors)
			report_errors();

		for (exit_rc = RC_ok_with_errors; err_count >= 10; err_count /= 10)
			exit_rc++;
	}
}

//
bool operator < (const api_error & a, const api_error & b)
{
	if (a.code != b.code)
		return a.code < b.code;

	if (a.args != b.args)
		return a.args < b.args;

	return a.func < b.func;
}

void context::report_errors()
{
	set<api_error> all;
	dword current = -1;

	for (auto & e : scanner_err) all.insert(e);
	for (auto & e : deleter_err) all.insert(e);

	printf("Errors:\n");

	for (auto & e : all)
	{
		if (e.code != current)
		{
			const char * fmt = (e.code < 0x10000000) ? "  Code %lu - %S\n" : "  Code %08lx - %S\n";
			wstring desc;

			if (! get_error_desc(e.code, desc))
				desc = L"<no description available>";

			printf(fmt, e.code, desc.c_str());

			current = e.code;
		}

		printf("    %s\n", e.args.c_str());
	}
}

/*
 *
 */
int wmain_app(int argc, wchar_t ** argv)
{
	context x;

	x.init();

	x.parse_args(argc, argv);

	x.check_path();

	x.confirm_it();

	x.process();

	x.report();

	return x.exit_rc;
}