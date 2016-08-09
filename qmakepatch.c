
/*
Patching utility for QMake executables
Allows changing some hardcoded values in "qmake.exe" (regardless
of executable file format, be it ELF, PE or whatever), like paths
and Qt version string, without relying on external INI files.
*/


#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#	define __USE_GNU
#endif

#include <stddef.h>		/* For `size_t` only. */
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <errno.h>


#ifndef SIZE_C
#	define SIZE_C(X) ((size_t)X)
#endif

#ifndef SIZE_MAX
#	define SIZE_MAX SIZE_C(-1)
#endif


#ifndef BOOL
#	define BOOL int
#endif

#ifndef TRUE
#	define TRUE 1
#endif

#ifndef FALSE
#	define FALSE 0
#endif


/* Sanity limit for reserved area size of a single "qt_xxxxpath" variable. */
#define FIELD_RESERVED_AREA_LIMIT SIZE_C(4096)



typedef enum {	/* RetCode_t */
	Success = 0,
	BadSyntax = 1,
	BadConfig = 2,
	GenericFailure = 3,
	FileFailure = 4,
	DataFailure = 5
} RetCode_t;


typedef struct {	/* ImageInfo_t */
	void* Data;
	size_t Size;
	size_t Offset;	/* Cached search start position for "qt_xxxxpath". */
} ImageInfo_t;



RetCode_t ReadImage(
	ImageInfo_t* poImageInfo,
	const char* pszFileName
) {
	FILE* hFile;
	long int nBytesFile;
	size_t nBytesData;

	hFile = fopen(pszFileName, "rb");
	if (!hFile) {
		fprintf(stderr,
			"Could not open file '%s' for reading.\n",
			pszFileName);
		perror(NULL);
		return FileFailure;
	} /* end if */

	if (fseek(hFile, 0L, SEEK_END) != 0) {
		fprintf(stderr,
			"Could not seek to the end of file '%s'.\n",
			pszFileName);
		perror(NULL);
		return FileFailure;
	} /* end if */

	nBytesFile = ftell(hFile);
	if (!nBytesFile) {
		fprintf(stderr,
			"File '%s' is empty.\n",
			pszFileName);
		return FileFailure;
	} else if (nBytesFile > (SIZE_MAX >> 1)) {
		fprintf(stderr,
			"File '%s' has very large size of %li bytes.\n",
			pszFileName, nBytesFile);
		return FileFailure;
	} /* end if */

	nBytesData = (size_t)nBytesFile;

	poImageInfo->Data = malloc(nBytesData);
	if (!poImageInfo->Data) {
		fprintf(stderr,
			"Could not allocate %zu bytes"
			" to read the contents of file '%s'.\n",
			nBytesData, pszFileName);
		perror(NULL);
		return GenericFailure;
	} /* end if */

	rewind(hFile);
	if (fread(poImageInfo->Data, sizeof(char), nBytesData, hFile)
			!= nBytesData
	) {
		fprintf(stderr,
			"Could not read %zu bytes of file '%s'.\n",
			nBytesData, pszFileName);
		perror(NULL);
		return FileFailure;
	} /* end if */

	poImageInfo->Size = nBytesData;

	fclose(hFile);

	return Success;
} /* end function ReadImage */


RetCode_t WriteImage(
	const ImageInfo_t* poImageInfo,
	const char* pszFileName
) {
	FILE* hFile;

	hFile = fopen(pszFileName, "wb");
	if (!hFile) {
		fprintf(stderr,
			"Could not open file '%s' for writing.\n",
			pszFileName);
		perror(NULL);
		return FileFailure;
	} /* end if */

	rewind(hFile);
	if (fwrite(poImageInfo->Data, sizeof(char), poImageInfo->Size, hFile)
			!= poImageInfo->Size
	) {
		fprintf(stderr,
			"Could not write %zu bytes to file '%s'.\n",
			poImageInfo->Size, pszFileName);
		perror(NULL);
		return FileFailure;
	} /* end if */

	/* TODO: Is truncation necessary in the most general case? */

	if (fclose(hFile) != 0) {
		fprintf(stderr,
			"Could not close file '%s' after writing %zu bytes.\n",
			poImageInfo->Size, pszFileName);
		perror(NULL);
		return FileFailure;
	} /* end if */

	return Success;
} /* end function WriteImage */


/*
Finds the first non-zero byte after `pvBegin` until `pvEnd`.
TODO: Is there any standard (optimized) function for that?
*/
const void* FindNonZeroByte(
	const void* pvBegin,
	const void* pvEnd	/* Start of next block, not inclusive. */
) {
	const char* pcCur;
	pcCur = (const char*)pvBegin;
	while (!*pcCur && ((const void*)pcCur < pvEnd)) {
		++pcCur;
	} /* end if */
	return (const void*)pcCur;
} /* end function FindNonZeroByte */


size_t RewriteField(
	const char* pcFieldName,
	size_t nFieldNameBytes,
	void* pvFieldAreaBeginning,
	void* pvFieldValueBeginning,
	void* pvDataEnd,
	const void* pvFieldReplacement,
	size_t nFieldReplacementBytes
) {
	char* pcFoundEnd;
	size_t nBytesReserved;

	pcFoundEnd = memchr(
		pvFieldValueBeginning,
		'\0',
		(const char*)pvDataEnd - (const char*)pvFieldValueBeginning
	);
	if (!pcFoundEnd) {
		fprintf(stderr,
			"Could not find the end of '%.*s' value in image.\n",
			(int)nFieldNameBytes, pcFieldName);
		return DataFailure;
	} /* end if */

	nBytesReserved = pcFoundEnd - (char*)pvFieldAreaBeginning;
	if (nBytesReserved > FIELD_RESERVED_AREA_LIMIT) {
		fprintf(stderr,
			"Determined size of '%.*s' value in image"
			" is %zu bytes, which is beyond sanity limit"
			" of %zu bytes.\n",
			(int)nFieldNameBytes, pcFieldName,
			nBytesReserved, FIELD_RESERVED_AREA_LIMIT);
		return DataFailure;
	} /* end if */

	pcFoundEnd = (char*)FindNonZeroByte(
		pcFoundEnd + sizeof(char),
		pvDataEnd
	);
	if ((void*)pcFoundEnd >= pvDataEnd) {
		fprintf(stderr,
			"Could not find the end of '%.*s'"
			" reserved area in image.\n",
			(int)nFieldNameBytes, pcFieldName);
		return DataFailure;
	} /* end if */

	nBytesReserved = pcFoundEnd - (char*)pvFieldAreaBeginning;
	if (nBytesReserved > FIELD_RESERVED_AREA_LIMIT) {
		fprintf(stderr,
			"Determined size of '%.*s' reserved area in image"
			" is %zu bytes, which is beyond sanity limit"
			" of %zu bytes.\n",
			(int)nFieldNameBytes, pcFieldName,
			nBytesReserved, FIELD_RESERVED_AREA_LIMIT);
		return DataFailure;
	} /* end if */

	if (nFieldReplacementBytes > nBytesReserved) {
		fprintf(stderr,
			"Determined size of '%.*s' reserved area in image"
			" is %zu bytes, while the new value requires"
			" %zu bytes.\n",
			(int)nFieldNameBytes, pcFieldName,
			nBytesReserved, nFieldReplacementBytes);
		return DataFailure;
	} /* end if */

	memcpy(
		pvFieldAreaBeginning,
		pvFieldReplacement,
		nFieldReplacementBytes
	);
	memset(
		(char*)pvFieldAreaBeginning + nFieldReplacementBytes,
		'\0',
		nBytesReserved - nFieldReplacementBytes
	);

	return Success;
} /* end function RewriteField */


RetCode_t RewriteFieldWithBeacon(
	ImageInfo_t* poImageInfo,
	const char* pszBeacon,
	const char* pszReplacement,
	BOOL fVerbose
) {
	size_t nBeaconChars;
	size_t nBeaconBytes;
	char* pcFoundBeginning;
	char* pvDataBeginning;
	size_t nDataBytes;

	nBeaconChars = strlen(pszBeacon);
	nBeaconBytes = nBeaconChars + sizeof(char);

	pvDataBeginning = (char*)poImageInfo->Data;
	nDataBytes = poImageInfo->Size;

	while (nDataBytes) {

		pcFoundBeginning = (char*)memmem(
			pvDataBeginning,
			nDataBytes,
			pszBeacon,
			nBeaconBytes
		);

		if (!pcFoundBeginning) {
			if (fVerbose) fprintf(stderr,
				"Could not find '%.*s' beacon in image.\n",
				(int)nBeaconChars, pszBeacon);
			return DataFailure;
		} /* end if */

		pcFoundBeginning += nBeaconBytes;

		if (isprint(*pcFoundBeginning)) {
			break;
		} else {
			nDataBytes -= (pcFoundBeginning - pvDataBeginning);
			pvDataBeginning = pcFoundBeginning;
		} /* end if */

	} /* end while */

	if (!nDataBytes) {
		if (fVerbose) fprintf(stderr,
			"Could not find '%.*s' beacon in image.\n",
			(int)nBeaconChars, pszBeacon);
		return DataFailure;
	} /* end if */

	return RewriteField(
		pszBeacon,
		nBeaconBytes,
		pcFoundBeginning,
		pcFoundBeginning,
		(char*)poImageInfo->Data + poImageInfo->Size,
		pszReplacement,
		strlen(pszReplacement) + sizeof(char)
	);

	return Success;
} /* end function RewriteFieldWithBeacon */


RetCode_t RewriteVersion(
	ImageInfo_t* poImageInfo,
	const char* pszVersion
) {
	const char* pcFoundSeparator;
	size_t nBytesMajor;

	if (!*pszVersion) return Success;

	pcFoundSeparator = strchrnul(pszVersion, '.');
	nBytesMajor = pcFoundSeparator - pszVersion;

	if ((strncmp(pszVersion, "1", nBytesMajor) == 0) ||
		(strncmp(pszVersion, "2", nBytesMajor) == 0)
	) {
		fprintf(stderr,
			"Qt versions 1.x and 2.x did not have QMake.\n");
		return BadConfig;
	} else if (strncmp(pszVersion, "3", nBytesMajor) == 0) {
		return RewriteFieldWithBeacon(poImageInfo,
			"-version", pszVersion, TRUE);
	} else if (strncmp(pszVersion, "4", nBytesMajor) == 0) {
		BOOL fDone_version;		/* '-version' */
		BOOL fDone_QT_VERSION;		/* 'QT_VERSION' */
		fDone_version = (RewriteFieldWithBeacon(poImageInfo,
			"-version", pszVersion, FALSE) == Success);
		fDone_QT_VERSION = (RewriteFieldWithBeacon(poImageInfo,
			"QT_VERSION", pszVersion, FALSE) == Success);
		if (fDone_version || fDone_QT_VERSION) {
			return Success;
		} else {
			fprintf(stderr,
				"Could not update any of '-version'"
				" or 'QT_VERSION'.\n");
			return DataFailure;
		} /* end if */
	} else if (strncmp(pszVersion, "5", nBytesMajor) == 0) {
		BOOL fDone_version;		/* '--version' */
		BOOL fDone_QMAKE_VERSION;	/* 'QMAKE_VERSION', sic! */
		BOOL fDone_Qt;			/* ') (Qt ' */
		fDone_version = (RewriteFieldWithBeacon(poImageInfo,
			"--version", pszVersion, FALSE) == Success);
		fDone_QMAKE_VERSION = (RewriteFieldWithBeacon(poImageInfo,
			"QMAKE_VERSION", pszVersion, FALSE) == Success);
		fDone_Qt = (RewriteFieldWithBeacon(poImageInfo,
			") (Qt ", pszVersion, FALSE) == Success);
		if (fDone_version || fDone_QMAKE_VERSION || fDone_Qt) {
			return Success;
		} else {
			fprintf(stderr,
				"Could not update any of '--version',"
				" 'QT_VERSION' ('QMAKE_VERSION')"
				" or ') (Qt '.\n");
			return DataFailure;
		} /* end if */
	} else {
		fprintf(stderr,
			"No idea on how to rewrite version string"
			" for Qt major version '%.*s'.\n",
			(int)nBytesMajor, pszVersion);
		return BadConfig;
	} /* end if */

} /* end function RewriteVersion */


RetCode_t RewriteVariable(
	ImageInfo_t* poImageInfo,
	const char* pszNameValuePair
) {
	char* pcFound;
	size_t nBytesLeader;
	size_t nBytesOffset;

	pcFound = strchr(pszNameValuePair, '=');
	if (!pcFound) {
		fprintf(stderr,
			"No equals sign found in '%s'.\n",
			pszNameValuePair);
		return BadConfig;
	} /* end if */

	/* Determine the length of "qt_xxxxpath=" leader. */
	nBytesLeader = pcFound - pszNameValuePair + sizeof(char);

	pcFound = (char*)memmem(
		((const char*)poImageInfo->Data) + poImageInfo->Offset,
		poImageInfo->Size - poImageInfo->Offset,
		pszNameValuePair,
		nBytesLeader
	);

	if (!pcFound && poImageInfo->Offset) {
		pcFound = (char*)memmem(
			poImageInfo->Data,
			poImageInfo->Size,
			pszNameValuePair,
			nBytesLeader
		);
	} /* end if */

	if (!pcFound) {
		fprintf(stderr,
			"Could not find '%.*s' in image.\n",
			(int)nBytesLeader, pszNameValuePair);
		return DataFailure;
	} /* end if */

	nBytesOffset = (pcFound - (char*)poImageInfo->Data)
		& (SIZE_MAX ^ SIZE_C(0xFFFF));
	if ((nBytesOffset < poImageInfo->Offset) || !poImageInfo->Offset) {
		poImageInfo->Offset = nBytesOffset;
	} /* end if */

	return RewriteField(
		pszNameValuePair,
		nBytesLeader - sizeof(char),
		pcFound,
		pcFound + nBytesLeader,
		(char*)poImageInfo->Data + poImageInfo->Size,
		pszNameValuePair,
		strlen(pszNameValuePair) + sizeof(char)
	);
} /* end function RewriteVariable */


RetCode_t PatchQmakeExe(
	const char* pszFileName,
	const char* pszVersion,
	size_t nVarSpecs,
	const char* apszVarSpecs[]
) {
	RetCode_t ordRetCode;
	ImageInfo_t oImageInfo;
	size_t ndxVarSpec;

	oImageInfo.Data = NULL;
	oImageInfo.Size = 0u;
	oImageInfo.Offset = 0u;

	ordRetCode = ReadImage(&oImageInfo, pszFileName);
	if (ordRetCode != Success) return ordRetCode;

	ordRetCode = RewriteVersion(&oImageInfo, pszVersion);
	if (ordRetCode != Success) return ordRetCode;

	for (ndxVarSpec = 0u; ndxVarSpec < nVarSpecs; ++ndxVarSpec) {
		ordRetCode = RewriteVariable(&oImageInfo,
			apszVarSpecs[ndxVarSpec]);
		if (ordRetCode != Success) return ordRetCode;
	} /* end for */

	ordRetCode = WriteImage(&oImageInfo, pszFileName);
	if (ordRetCode != Success) return ordRetCode;

	return Success;
} /* end function PatchQmakeExe */


const char* GetModuleName(
	const char* argv0
) {
	static const char szDefault[] = "qmakepatch";
	const char* pszModulePath;
	const char* pszSeparator;

	if (!argv0) return szDefault;
	pszModulePath = argv0;

	if (!pszModulePath || !*pszModulePath) return szDefault;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
	pszSeparator = strrchr(pszModulePath, '\\');
	if (!pszSeparator) pszSeparator = strrchr(pszModulePath, '/');
#else
	pszSeparator = strrchr(pszModulePath, '/');
#endif
	if (!pszSeparator) return pszModulePath;
	++pszSeparator;
	if (!*pszSeparator) return szDefault;

	return pszSeparator;
} /* end function GetModuleName */


void ShowHelp(
	const char* pszModuleName
) {

	printf(
"Patching utility for QMake executables\n"
"\n"
	);
	printf(
"Syntax:\n"
"	%s {qmake.exe} {version} [name=value ...]\n",
		pszModuleName
	);
	printf(
"where\n"
"	qmake.exe\n"
"		Path to QMake executable file: 'qmake', '/bin/qmake-qt5'\n"
"	version\n"
"		Version string to be written.\n"
"		Pass an empty argument to skip this patch.\n"
"	name=value\n"
"		Variable name and its new value to be written.\n"
"		Ex.: qt_prfxpath=/opt/qt4\n"
"		Note that Qt5 allows to patch just a few of them.\n"
"\n"
	);
	printf(
"Example:\n"
"	%s ./qmake 4.8.4 qt_prfxpath=/opt/qt4 qt_libspath=/opt/qt4/lib\n",
		pszModuleName
	);

	return /* void */;
} /* end function ShowHelp */


int main(
	int argc,
	const char** argv
) {
	RetCode_t ordRetCode;
	int fShowHelp;

	ordRetCode = Success;
	fShowHelp = 0;

	if (argc >= (1 + 1)) {
		if (
			(strcmp(argv[1], "-h") == 0) ||
			(strcmp(argv[1], "-?") == 0) ||
			(strcmp(argv[1], "--help") == 0) ||
			(strcmp(argv[1], "help") == 0)
		) {
			fShowHelp = 1;
		} /* end if */
	} /* end if */

	if (!fShowHelp && (argc < (1 + 2))) {
		ordRetCode = BadSyntax;
		fShowHelp = 1;
	} /* end if */

	if (fShowHelp) {
		ShowHelp(GetModuleName(argc ? argv[0] : NULL));
		return ordRetCode;
	} /* end if */

	ordRetCode = PatchQmakeExe(argv[1], argv[2],
		argc - (1 + 2), argv + (1 + 2));

	return ordRetCode;
} /* end function main */
