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
#ifndef _ULTRA_MACHINE_INTERNAL_H_
#define _ULTRA_MACHINE_INTERNAL_H_

#include "ultra_machine.h"
#include "libp/_simple_work_queue.h"

//
struct ultra_mach;

//
struct ultra_task : work_item, fsi_scan_cb, api_error_cb
{
	ultra_task(ultra_mach * mach);

	__no_copying(ultra_task);

	/*
	 *	work_item
	 */
	void execute();
	void do_delete_file(const fsi_item & f);
	void do_delete_self();

	/*
	 *	fsi_scan_cb
	 */
	void on_fsi_open(HANDLE h) { }
	bool on_fsi_scan(const wc_range & name, const fsi_info & info);

	/*
	 *	api_error_cb
	 */
	void on_api_error_x(const api_error & e);

	//
	ultra_mach   * mach;
	folder       * curr;
	int            phase;

	size_t         ph2_first; // delete curr->files[first, first+count-1]
	size_t         ph2_count;

	wstring        path;
	api_error_vec  errors;
};

typedef vector<ultra_task *> ultra_task_vec;

//
struct ultra_task_pool
{
	ultra_task_pool();
	~ultra_task_pool();

	__no_copying(ultra_task_pool);

	//
	ultra_task * get(folder * d, int phase);
	void put(ultra_task * w);

	bool unused() const;

	ultra_mach    * mach;
	ultra_task_vec  cache;
	size_t          allocated;
};

//
struct ultra_mach
{
	ultra_mach_conf    conf;
	ultra_mach_cb    * cb;
	bool               ph1_only;  // aka 'just_scan'

	simple_work_queue  swq;
	ultra_task_pool    pool;
	bool               enough;

	ultra_mach_info    info;
	size_t             ph1_work, ph2_work, ph3_work;
	size_t             ph1_done, ph2_done, ph3_done;

	//
	ultra_mach();
	~ultra_mach();

	bool init(const ultra_mach_conf & conf, ultra_mach_cb * cb);
	void term();

	bool keep_going() const;

	void enqueue_ph1(folder * x);
	void enqueue_ph2(folder * x);
	void enqueue_ph3(folder * x);

	void complete_ph1(ultra_task * w);
	void complete_ph2(ultra_task * w);
	void complete_ph3(ultra_task * w);

	void loop();
};

#endif
