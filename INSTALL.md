
# QmakePatch — installation instructions

The code base consists of one source file without any headers.
Program is written in pure C, but requires some GNU additions:

* `memmem()` for searching binary data (like `strstr()`, but for raw bytes);

* `strchrnul()` for searchng first occurrence of a character or terminator;

* `fprintf()` that supports `"%zu"` type specifier for `size_t`
and `"%.*s"` specifier for fixed-length strings.

Therefore, any C compiler in GNU/Linux system will suffice.
Other environments may be supported by implementing those functions manually
or replacing their calls with other non-standard tools, — feel free to submit
patch requests if your solution may be generalized for common use.



## Building

Usually, the program may be compiled and linked with the following command:

````sh
	cc -o qmakepatch qmakepatch.c
````

If `cc` is not found, try `gcc` or `clang`.

When building on Windows (Cygwin), extend the output name with `.exe` suffix.



## Installation

Once the executable is built, it does not require any specific installation
routine, — it can be run from any directory which allows program execution,
and thus may either be left in the compilation directory or copied
to system-specific location for custom executables, like `/usr/local/bin`.
