# byenow

This is the source code of `byenow`, a multithreaded folder removal utility for Windows.

https://iobureau.com/byenow

The code is shared to allow anyone interested to see how exactly the program works. The source is **incomplete**, because it depends on several libraries that are not a part of this distribution. However, with an exception of `simple_work_queue`, these libraries are simple Win32 API wrappers and their functionality should be fairly obvious from their usage.

To read through the code, start with `wmain_app()` in `byenow.cpp` and do from there.
