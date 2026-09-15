/*
 * Copyright (C) Ascensio System SIA, 2009-2026
 *
 * This program is a free software product. You can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License (AGPL)
 * version 3 as published by the Free Software Foundation, together with the
 * additional terms provided in the LICENSE file.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. For
 * details, see the GNU AGPL at: https://www.gnu.org/licenses/agpl-3.0.html
 *
 * You can contact Ascensio System SIA by email at info@onlyoffice.com
 * or by postal mail at 20A-6 Ernesta Birznieka-Upisha Street, Riga,
 * LV-1050, Latvia, European Union.
 *
 * The interactive user interfaces in modified versions of the Program
 * are required to display Appropriate Legal Notices in accordance with
 * Section 5 of the GNU AGPL version 3.
 *
 * No trademark rights are granted under this License.
 *
 * All non-code elements of the Product, including illustrations,
 * icon sets, and technical writing content, are licensed under the
 * Creative Commons Attribution-ShareAlike 4.0 International License:
 * https://creativecommons.org/licenses/by-sa/4.0/legalcode
 *
 * This license applies only to such non-code elements and does not
 * modify or replace the licensing terms applicable to the Program's
 * source code, which remains licensed under the GNU Affero General
 * Public License v3.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "../../Common/3dParty/misc/proclimits.h"
#include "../../DesktopEditor/common/StringExt.h"
#include "../../DesktopEditor/common/SystemUtils.h"
#include "ASCConverters.h"
#include "cextracttools.h"
#include "../../DesktopEditor/fontengine/ApplicationFontsWorker.h"

#include <iostream>
#include <exception>
#include <new>
#include <stdio.h>

#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)

using namespace NExtractTools;

#if !defined(_WIN32) && !defined(_WIN64)
static std::wstring utf8_to_unicode(const char *src)
{
	return NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE *)src, (LONG)strlen(src));
}
#endif

// #1359 (second half): x2t caps its own heap.  main() below applies
// X2T_MEMORY_LIMIT - 4GiB by default - with setrlimit(RLIMIT_DATA) (see
// Common/3dParty/misc/proclimits.h; a Job Object on Windows).  Until this
// commit nothing in x2t caught anything, so the std::bad_alloc thrown when a
// file exhausts that cap ran off the top of main, terminate() fired, and the
// process died on a signal.  The editor cannot tell that from a corrupt file:
// it has no exit code to read, only a dead converter, so it shows the generic
// "Something has gone wrong...".
//
// The CSV reader materialises the whole workbook before writing anything, at
// roughly 120x the input size (measured: 8.5MB -> 982MB, 35.5MB -> 2.7GB), so
// the cap is reached by ordinary files - the cliff sits between the #1359
// reporter's 9.5MB CSV, which opens, and their 35.6MB one, which does not.
//
// This does NOT raise or remove the limit, and does not try to carry on: an
// operator new that has already failed leaves the process in a poor state.  It
// turns dying on a signal into returning an error code, and then exits.
//
// AVS_FILEUTILS_ERROR_CONVERT_LIMITS is the code to return, because it is the
// one that already has a user-visible message on the other side:
// getReturnErrorCode turns it into exit 93, sdkjs/common/editorscommon.js:1692
// maps -93 to c_oAscServerError.ConvertLIMITS, :1050 maps that to
// c_oAscError.ID.ConvertationOpenLimitError, and the spreadsheet shell renders
// it as errorFileSizeExceed - "The file size exceeds the limitation...".  It is
// also what ASCConverters.cpp:1758 already returns when checkInputLimits()
// rejects a file up front, which is the same condition reached the slow way.
// AVS_FILEUTILS_ERROR_CONVERT_CELLLIMITS was the other candidate and is wrong
// twice over: sdkjs has no entry for -96 at all, so it would fall through to
// Unknown, and SUCCEEDED_X2T (cextracttools.h:55) counts it as success.
//
// Called from inside catch(...), so `throw;` re-raises the exception being
// handled and the handlers below classify it.  Nothing in here allocates: we
// may be unwinding precisely because allocation just failed, which is why this
// writes with fputs on string literals rather than std::cout << std::string.
static _UINT32 errorCodeForCurrentException()
{
	try
	{
		throw;
	}
	catch (const std::bad_alloc&)
	{
		fputs("x2t: out of memory - conversion aborted. The file needs more than this "
			  "process is allowed to allocate (X2T_MEMORY_LIMIT).\n", stderr);
		return AVS_FILEUTILS_ERROR_CONVERT_LIMITS;
	}
	catch (const std::exception& e)
	{
		fputs("x2t: conversion aborted by an exception: ", stderr);
		fputs(e.what(), stderr);
		fputs("\n", stderr);
		return AVS_FILEUTILS_ERROR_CONVERT;
	}
	catch (...)
	{
		fputs("x2t: conversion aborted by an unknown exception\n", stderr);
		return AVS_FILEUTILS_ERROR_CONVERT;
	}
}

#if !defined(_WIN32) && !defined(_WIN64)
static int mainUnguarded(int argc, char *argv[])
#else
static int mainUnguarded(int argc, wchar_t *argv[])
#endif
{
	// #define __CRTDBG_MAP_ALLOC
	// #include <crtdbg.h>
	// #define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
	// #define new DEBUG_NEW

	// check arguments
	if (argc < 2)
	{
		// print out help topic

		std::cout << std::endl;
		std::cout << std::endl;
		std::cout << "-------------------------------------------------------------------------------" << std::endl;
		std::cout << "\t\tOOX/binary file converter. Version: " VALUE(INTVER) << std::endl;
		std::cout << "-------------------------------------------------------------------------------" << std::endl;
		std::cout << std::endl;
		std::cout << "USAGE: x2t \"path_to_params_xml\"" << std::endl;
		std::cout << "or" << std::endl;
		std::cout << "USAGE: x2t \"path_to_file_1\" \"path_to_file_2\" [\"path_to_font_selection\"]" << std::endl;
		std::cout << "WHERE:" << std::endl;
		std::cout << "\t\"path_to_file_1\" is a path to file to be converted" << std::endl;
		std::cout << "\t\"path_to_file_2\" is a path to the corresponding output file" << std::endl;
		std::cout << "\t\"path_to_font_selection\" is a path to 'font_selection.bin' location" << std::endl << std::endl;
		std::cout << "NOTE: conversion direction will be calculated from file extensions" << std::endl << std::endl;

		return getReturnErrorCode(AVS_FILEUTILS_ERROR_CONVERT_PARAMS);
	}

	// set memory limit
	std::wstring sMemoryLimit = NSSystemUtils::GetEnvVariable(NSSystemUtils::gc_EnvMemoryLimit);
	if (sMemoryLimit.empty())
		sMemoryLimit = NSSystemUtils::gc_EnvMemoryLimitDefault;

#if !defined(_DEBUG) && !defined(__ANDROID__) && !defined(_IOS)
	long long nMemoryLimit;
	if (NSStringExt::FromHumanReadableByteCount(sMemoryLimit, nMemoryLimit) && nMemoryLimit > 0)
		limit_memory((size_t)nMemoryLimit);
#endif
	std::wstring sArg1, sArg2;

#if !defined(_WIN32) && !defined(_WIN64)
	sArg1 = utf8_to_unicode(argv[1]);
	if (argc >= 3)
		sArg2 = utf8_to_unicode(argv[2]);
#else
	sArg1 = std::wstring(argv[1]);
	if (argc >= 3)
		sArg2 = std::wstring(argv[2]);
#endif

	_UINT32 result = 0;
	std::wstring sXmlExt = _T(".xml");
	if (/*argc < 3 && */(sArg1.length() > 3) && (sXmlExt == sArg1.substr(sArg1.length() - sXmlExt.length(), sXmlExt.length())))
	{
		NExtractTools::InputParams oInputParams;
		if (oInputParams.FromXmlFile(sArg1) && (sArg2.empty() || oInputParams.FromXml(sArg2)))
		{
			result = NExtractTools::fromInputParams(oInputParams);
		}
		else
		{
			result = AVS_FILEUTILS_ERROR_CONVERT_PARAMS;
		}
	}
	else
	{
		std::wstring sArg3, sArg4, sArg5;

#if !defined(_WIN32) && !defined(_WIN64)
		if (argc >= 4)
			sArg3 = utf8_to_unicode(argv[3]);
		if (argc >= 5)
			sArg4 = utf8_to_unicode(argv[4]);
		if (argc >= 6)
			sArg5 = utf8_to_unicode(argv[5]);
#else
		if (argc >= 4)
			sArg3 = std::wstring(argv[3]);
		if (argc >= 5)
			sArg4 = std::wstring(argv[4]);
		if (argc >= 6)
			sArg5 = std::wstring(argv[5]);
#endif
		if (sArg1 == L"-detectmacro")
		{
			InputParams oInputParams;
			oInputParams.m_sFileFrom = new std::wstring(sArg2);

			result = NExtractTools::detectMacroInFile(oInputParams);
		}
		else if (sArg1 == L"-create-js-cache")
		{
			NExtractTools::createJSCaches();
			return 0;
		}
		else if (sArg1 == L"-create-js-snapshots")
		{
			NExtractTools::createJSSnapshots();
			return 0;
		}
		else if (sArg1 == L"-create-allfonts")
		{
			if (argc > 2)
			{
				CApplicationFontsWorker oWorker;
				oWorker.m_sDirectory = sArg2;
				oWorker.m_bIsUseSystemFonts = true;
				oWorker.m_bIsNeedThumbnails = false;
				oWorker.m_bIsCleanDirectory = false;

				for (int i = 3; i < argc; ++i)
				{
#if !defined(_WIN32) && !defined(_WIN64)
					std::wstring sFolder = utf8_to_unicode(argv[i]);
#else
					std::wstring sFolder(argv[i]);
#endif
					oWorker.m_arAdditionalFolders.push_back(sFolder);
				}

				NSFonts::IApplicationFonts* pFonts = oWorker.Check();
				RELEASEINTERFACE(pFonts);
			}
		}
		else
		{
			InputParams oInputParams;
			oInputParams.m_sFileFrom = new std::wstring(sArg1);
			oInputParams.m_sFileTo = new std::wstring(sArg2);

			if (argc > 3)
			{
				oInputParams.m_sFontDir = new std::wstring(sArg3);
			}
			if (argc > 4)
			{
				oInputParams.m_sPassword = new std::wstring(sArg4);
				oInputParams.m_sSavePassword = new std::wstring(sArg4);
			}
			result = NExtractTools::fromInputParams(oInputParams);
		}
	}
	//_CrtDumpMemoryLeaks();
	return getReturnErrorCode(result);
}

#if !defined(_WIN32) && !defined(_WIN64)
static int mainGuarded(int argc, char *argv[])
#else
static int mainGuarded(int argc, wchar_t *argv[])
#endif
{
	try
	{
		return mainUnguarded(argc, argv);
	}
	catch (...)
	{
		// Report it and leave.  Do not attempt to keep converting.
		return getReturnErrorCode(errorCodeForCurrentException());
	}
}

#ifdef BUILD_X2T_AS_LIBRARY_DYLIB
#if !defined(_WIN32) && !defined(_WIN64)
int main_lib(int argc, char *argv[])
#else
int wmain_lib(int argc, wchar_t *argv[])
#endif
#endif
#ifndef BUILD_X2T_AS_LIBRARY_DYLIB
#if !defined(_WIN32) && !defined(_WIN64)
	int main(int argc, char *argv[])
#else
	int wmain(int argc, wchar_t *argv[])
#endif
#endif
{
	return mainGuarded(argc, argv);
}
