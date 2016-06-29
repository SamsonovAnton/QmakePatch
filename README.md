
# QmakePatch — a patching utility for QMake executables

Allows changing some hard-coded values in `qmake.exe`
(regardless of executable file format, be it ELF, PE or whatever),
like `QT_INSTALL_PATH` and *Qt* version string, without relying on external
settings in INI files and Windows registry.
This helps to keep different *Qt* installations side by side on a single
machine at the same time, including different flavors of the same version,
which is especially handful for cross-compilation and other custom scenarios.

Patching relies on the fact that `qmake.exe` is usually built with plenty
of spare space reserved specifically for this (although *Qt* authors do not
provide stock tools for patching).
The exact amount of reserved space varies by build and even between values
of different kinds, so patching will only succeed if all replacement values fit
into their destination areas, including at least one null character at the end
of each area to serve as value terminator.
This will not be an issue most of the time, but there is always a possibility
that some adjustments may fail: for example, if we ever need to change
the version string from "4.8.9" to "4.8.10", there probably will be no room
to accommodate that longer string, since no additional space is usually reserved
for that variable.

Worth to note is that distinct *QMake* versions may differ significantly
in the degree of possible customization.
For example, QMake 3.x (for Qt 5.x) only has 3 variables that specify
installation prefixes, and 2 of them are expected to have the same value.
This is in contrast with Qmake 1.x (for Qt 3.x) and QMake 2.x (for Qt 4.x),
which both had a dozen of independently customizable paths.



## Related work

*Qt patcher* by *Developpez.com*, aka *DVP Qt patcher*
([HTTP](http://ftp-developpez.com/qt/binaires/win32/patcher/QtPatche_src.7z),
[FTP](ftp://developpez.com/qt/binaires/win32/patcher/) —
links may not be visible on GitHub website),
is suited for the same task, but has several shortcomings:

* Implemented as Qt GUI application which requires user interaction
and disallows automated processing.

* Tailored to Qt 4.x, so will not work for Qt 3.x or Qt 5.x.

* Rewrites predefined variables all at once, using hardcoded path suffixes.

* Does not rewrite version string.

* Attempts to patch `mkspecs/default/qmake.conf` as well.

* Minimum to none error checking.

In contrast to that, *QmakePatch* was developed with the following goals
in mind:

* Console application in pure C which may be invoked directly
or by shell scripts for automation and reuse.

* Support all Qt versions known to the date: 3.x (Qmake 1.x), 4.x (QMake 2.x),
5.x (QMake 3.x).

* Rewrite only those variables specified by user,
allowing any replacement values for flexible relocation.

* Rewrite Qt version string if possible.

* Thorough error checking.
Patching will only be completed if every single adjustment succeeds.

* Leave the job of modifying text configs to other tools,
which are much more suited for that.

Obviously, `qmake.exe` may be patched manually with the use of hex editor, —
*QmakePatch* just makes this process easier and faster.



## Invocation

Command line syntax (may be obtained with `-h` or `--help`):

````sh
	qmakepatch {qmake.exe} {version} [name=value ...]
````

where:

* _qmake.exe_ is the path to QMake executable, like `qmake` or `./qmake-qt5`.

* _version_ is the new version string to be written, like `4.8.4`.
Pass an empty argument to skip this patch.

* _name=value_ pairs constitute variable names and their new values,
separated by equals sign `=` with no whitespaces around it,
like `qt_prfxpath=/opt/qt4`.

Any number of name and value pairs may be specified in command line;
if none are specified, then only the version string will be rewritten.
As already said, _version_ is a mandatory positional parameter and thus
must be present at all times, but may be set to an empty string for keeping
the version value intact.
Passing an empty string to a program is usually accomplished with a pair
of single or double quotes, like `''` and `""`.
Similarly, quoting a parameter allows passing values with spaces in them, —
like `"qt_prfxpath=C:\Program Files\Qt4"`, — but using such paths is strongly
discouraged traditionally, as they tend to be error-prone;
more legitimate example (in the techical sense, not in legal one)
would be `"qt_lcnsuser=Open Source"`.

Note that the location of version string differs by QMake (Qt) major release,
so *QmakePatch* uses the supplied value to choose the search method,
and it is therefore only possible to rewrite version strings *within* each
supported branch — *not between* major versions.
And, as with other variables, the replacement value length should not exceed
the reserved area size, which is usually equal to the original value length, —
that is, no extra room is allocated for version string.

The specified QMake executable file will only be rewritten if all specified
variables were found and successfully reassigned to their new values.
Otherwise the file will be left intact, a diagnostic message output to *stderr*,
and an error code returned:

* _0_ — success;
* _1_ — invocation syntax error;
* _2_ — malformed data supplied by user, such as name-value pair or version;
* _3_ — generic failure, such as memory allocation error;
* _4_ — file-related error, such as failure to read or write _qmake.exe_;
* _5_ — failure to find or replace a variable value.

For sanity checking purposes, an empirical limit is placed on how large
a reserved area for any single variable may ever be: this is currenly set
to 4096 bytes, which is definitely beyond typical amounts of 256 to 512 bytes
(which are, in turn, beyond limitations of popular file systems).
If that assumption will become broken some day, patching will fail
and a corresponding diagnostic message will be output;
should that happen, the limit must be changed in the source code, —
refer to [INSTALL.md](INSTALL.md) for compilation instructions afterwards.



## Examples

Changing the version string:

````sh
	qmakepatch ./qmake-qt5 5.5.1
````

Changing the settings file location in Qmake 1.x (Qt 3.x):

````sh
	qmakepatch ./qmake "" qt_cnfpath=/etc/qt-3.3.3
````

Changing the settings file location and installation date in Qmake 2.x (Qt 4.x):

````sh
	qmakepatch ./qmake "" qt_stngpath=/etc/qt-4.8.4 qt_instdate=2016-06-28
````

Complete patching of QMake 2.x to be used with a guest Qt 4.x kit
for cross-compilation:

````sh
	#!/bin/sh

	XC_ROOTDIR='/opt/mcst/fs.e90-v8.2.6.33'		# Guest system root.
	QT_BASEDIR="${XC_ROOTDIR}/opt/qt4"
	QMAKE_FILE="${QT_BASEDIR}/bin/qmake"
	QT_VERSION='4.8.4'

	qmakepatch "$QMAKE_FILE" "$QT_VERSION" \
		"qt_prfxpath=${QT_BASEDIR}" \
		"qt_docspath=${QT_BASEDIR}/doc" \
		"qt_hdrspath=${QT_BASEDIR}/include" \
		"qt_libspath=${QT_BASEDIR}/lib" \
		"qt_plugpath=${QT_BASEDIR}/plugins" \
		"qt_impspath=${QT_BASEDIR}/imports" \
		"qt_datapath=${QT_BASEDIR}" \
		"qt_trnspath=${QT_BASEDIR}/translations" \
		"qt_stngpath=${XC_ROOTDIR}/etc/xdg" \
		"qt_xmplpath=${QT_BASEDIR}/examples" \
		"qt_demopath=${QT_BASEDIR}/demos"
````

As demonstrated in this example, when it comes to cross-compilation, there
usually is no need to alter `QT_INSTALL_BINS` (`qt_binspath`) from its
native value, because other Qt tools may be used right as they are and where
they are, coupled with any foreign build of the same major-minor branch.

An overall list of variable names that are known to be available
for customization is presented in the next section.



## Technical details

Early Qt releases, namely version 1.x and 2.x, did not have QMake at all.

Qmake 1.x, available as a part of Qt 3.x, typically has the following variables:

````
	QT_INSTALL_PREFIX:         qt_nstpath=/usr/lib/qt-3.3.3
	[anonymous]                qt_binpath=.../bin
	[anonymous]                qt_docpath=.../doc
	[anonymous]                qt_hdrpath=.../include
	[anonymous]                qt_libpath=.../lib
	[anonymous]                qt_plgpath=.../plugins
	QT_INSTALL_DATA:           qt_datpath=...
	[anonymous]                qt_trnpath=.../translations
	[anonymous]                qt_cnfpath=/etc/qt-3.3.3
	QMAKE_MKSPECS:             ${QT_INSTALL_DATA}/mkspecs
````

To the left are well-known names, in upper case, which can be referenced
in project and spec files as `$$[QT_INSTALL_PREFIX]` and queried from command
line as `qmake -query QT_INSTALL_PREFIX`; variables marked as anonymous
are only used internally by QMake and cannot be queried.
To the right are internal names, in lower case, with their sample values.
Alternatively, an expression like `${QT_INSTALL_DATA}/mkspecs` denotes
a value computed at run time from another variable plus some hardcoded suffix,
and thus not customizable.

Qmake 2.x, available as a part of Qt 4.x, typically has the following variables:

````
	[anonymous]                qt_lcnsuser=Open Source
	[anonymous]                qt_lcnsprod=Open Source
	[anonymous]                qt_instdate=2014-10-16
	QT_INSTALL_PREFIX:         qt_prfxpath=/opt/qt4
	QT_INSTALL_DOCS:           qt_docspath=.../doc
	QT_INSTALL_HEADERS:        qt_hdrspath=.../include
	QT_INSTALL_LIBS:           qt_libspath=.../lib
	QT_INSTALL_BINS:           qt_binspath=.../bin
	QT_INSTALL_PLUGINS:        qt_plugpath=.../plugins
	QT_INSTALL_IMPORTS:        qt_impspath=.../imports
	QT_INSTALL_DATA:           qt_datapath=...
	QT_INSTALL_TRANSLATIONS:   qt_trnspath=.../translations
	QT_INSTALL_CONFIGURATION:  qt_stngpath=/etc/xdg
	QT_INSTALL_EXAMPLES:       qt_xmplpath=.../examples
	QT_INSTALL_DEMOS:          qt_demopath=.../demos
	QMAKE_MKSPECS:             ${QT_INSTALL_DATA}/mkspecs
````

Qmake 3.x, available as a part of Qt 5.x, typically has the following variables:

````
	QT_INSTALL_PREFIX:         qt_prfxpath=/usr
	[QT_INSTALL_EPREFIX]:      qt_epfxpath=/usr
	QT_HOST_PREFIX:            qt_hpfxpath=/usr

	QT_SYSROOT:                [empty]
	QT_INSTALL_PREFIX:         ${QT_INSTALL_EPREFIX}
	QT_INSTALL_ARCHDATA:       ${QT_INSTALL_EPREFIX}/lib/qt5
	QT_INSTALL_DATA:           ${QT_INSTALL_EPREFIX}/share/qt5
	QT_INSTALL_DOCS:           ${QT_INSTALL_EPREFIX}/share/doc/packages/qt5
	QT_INSTALL_HEADERS:        ${QT_INSTALL_EPREFIX}/include/qt5
	QT_INSTALL_LIBS:           ${QT_INSTALL_EPREFIX}/lib
	QT_INSTALL_LIBEXECS:       ${QT_INSTALL_EPREFIX}/lib/qt5/libexec
	QT_INSTALL_BINS:           ${QT_INSTALL_EPREFIX}/lib/qt5/bin
	QT_INSTALL_TESTS:          ${QT_INSTALL_EPREFIX}/tests
	QT_INSTALL_PLUGINS:        ${QT_INSTALL_EPREFIX}/lib/qt5/plugins
	QT_INSTALL_IMPORTS:        ${QT_INSTALL_EPREFIX}/lib/qt5/imports
	QT_INSTALL_QML:            ${QT_INSTALL_EPREFIX}/lib/qt5/qml
	QT_INSTALL_TRANSLATIONS:   ${QT_INSTALL_EPREFIX}/share/qt5/translations
	QT_INSTALL_CONFIGURATION:  /etc/xdg
	QT_INSTALL_EXAMPLES:       ${QT_INSTALL_EPREFIX}/lib/qt5/examples
	QT_INSTALL_DEMOS:          ${QT_INSTALL_EPREFIX}/lib/qt5/examples
	QT_HOST_DATA:              ${QT_HOST_PREFIX}/lib/qt5
	QT_HOST_BINS:              ${QT_HOST_PREFIX}/usr/lib/qt5/bin
	QT_HOST_LIBS:              ${QT_HOST_PREFIX}/usr/lib
````

As can be seen, Qmake 3.x provides almost two dozens of variables, but only few
of them can be customized by patching the executable file.
Particularly, `QT_SYSROOT` and `QT_INSTALL_CONFIGURATION`, although
not being dependent of other variables, are not easy to search and replace
automatically, — not only because they lack a constant “beacon” before them,
but also because their original value may be an empty string.

As to `QT_INSTALL_PREFIX` and `QT_INSTALL_EPREFIX`, the latter name
is neither official nor directly queryable, and both are supposed to hold
the same value — similarly to `prefix` and `exec-prefix` in configuration
scripts generated by *GNU autoconf*, which all default to either `/usr`
or `/usr/local`, depending on operating system conventions.
Otherwise, if their values differ, an additional “development” flavor
is created for each variable:

````
	QT_INSTALL_PREFIX:         ${QT_INSTALL_EPREFIX}
	QT_INSTALL_PREFIX/dev:     ${QT_INSTALL_PREFIX}
	QT_INSTALL_ARCHDATA:       ${QT_INSTALL_EPREFIX}/lib/qt5
	QT_INSTALL_ARCHDATA/dev:   ${QT_INSTALL_PREFIX}/lib/qt5
	...
	QT_INSTALL_DEMOS:          ${QT_INSTALL_EPREFIX}/lib/qt5/examples
	QT_INSTALL_DEMOS/dev:      ${QT_INSTALL_PREFIX}/lib/qt5/examples
````

Therefore, both values should be kept in sync with each other, and that is
the user's responsibility.
