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
#include "Common.h"

#include "BinReaderWriterDefines.h"
#include "../../../XlsxFormat/Xlsx.h"
#include "../../../Common/SimpleTypes_Shared.h"
#include "../../../../Common/Base64.h"
#include "../../../../DesktopEditor/common/Types.h"
#include "../../../../DesktopEditor/raster/ImageFileFormatChecker.h"
#include "../../../../Common/OfficeFileFormats.h"

#ifndef DISABLE_FILE_DOWNLOADER
	#include "../../../../Common/Network/FileTransporter/include/FileTransporter.h"
#endif
#include "../../../../DesktopEditor/common/File.h"

namespace SerializeCommon
{
    std::wstring DownloadImage(const std::wstring& strFile)
	{
#ifndef DISABLE_FILE_DOWNLOADER
        std::wstring strFileName;
		
        NSNetwork::NSFileTransport::CFileDownloader oDownloader(strFile, false);
		if ( oDownloader.DownloadSync() )
		{
			strFileName = oDownloader.GetFilePath();
			
			CImageFileFormatChecker checker;
			if (false == checker.isImageFile(strFileName))
			{
				strFileName.clear();
			}  
		}
		return strFileName;
#else
		return L"";
#endif
	}
    VOID convertBase64ToImage (NSFile::CFileBinary& oFile, std::wstring &pBase64)
	{
		BYTE* pUtf8 = NULL;
		long nUtf8Size;
        NSFile::CUtf8Converter::GetUtf8StringFromUnicode(pBase64.c_str(), pBase64.length(), pUtf8, nUtf8Size);
        std::string sUnicode((char*)pUtf8, nUtf8Size);
		RELEASEARRAYOBJECTS(pUtf8);

		//Remove "data:image/jpg;base64,"
		int nShift = 0;
        int nIndex = sUnicode.find("base64,");
		if(-1 != nIndex)
		{
			nShift = nIndex + 7;
		}
		// Get file size
        LONG lFileSize = sUnicode.length () - nShift;
		INT nDstLength = lFileSize;
		BYTE *pBuffer = new BYTE [lFileSize];
		memset(pBuffer, 0, lFileSize);
        Base64::Base64Decode (sUnicode.c_str() + nShift, lFileSize, pBuffer, &nDstLength);

		CImageFileFormatChecker checker;
		std::wstring detectImageExtension = checker.DetectFormatByData(pBuffer, nDstLength);

		if (false == detectImageExtension.empty())
		{
			oFile.WriteFile(pBuffer, nDstLength);
		}

		RELEASEARRAYOBJECTS (pBuffer);
	}

	long Round(double val)
	{
		return (long)(val+ 0.5);
	}
    std::wstring changeExtention(const std::wstring& sSourcePath, const std::wstring& sTargetExt)
	{
        int nIndex = sSourcePath.rfind('.');
		if(-1 != nIndex)
            return sSourcePath.substr(0, nIndex + 1) + sTargetExt;
		return sSourcePath;
	}
    void ReadFileType(const std::wstring& sXMLOptions, BYTE& result, UINT& nCodePage, std::wstring& sDelimiter, BYTE& cSaveFileType, _INT32& Lcid)
	{
		result = BinXlsxRW::c_oFileTypes::XLSX;
		nCodePage = 46;		//default 46 temporarily CP_UTF8
		cSaveFileType = BinXlsxRW::c_oFileTypes::XLSX;// default
        Lcid = -1;// default

		sDelimiter = L","; // default common

		nullable<SimpleTypes::CUnsignedDecimalNumber> csvFormat;
		nullable<SimpleTypes::CUnsignedDecimalNumber> fileType;
		nullable<SimpleTypes::CUnsignedDecimalNumber> codePage;
		nullable<SimpleTypes::CUnsignedDecimalNumber> saveFileType;
        nullable<SimpleTypes::CDecimalNumber> LcidParam;
		nullable<std::wstring> delimiter;
		
		sDelimiter = L","; // default common

		XmlUtils::CXmlLiteReader oReader;
        if (true != oReader.FromString(sXMLOptions) || true != oReader.IsValid())
			return;

		oReader.ReadNextNode(); // XmlOptions
		if (oReader.IsEmptyNode())
			return;

		int nCurDepth = oReader.GetDepth();
		while(oReader.ReadNextSiblingNode(nCurDepth))
		{
			std::wstring sName = oReader.GetName();
			if (L"fileOptions" == sName)
			{
				WritingElement_ReadAttributes_Start(oReader)
					WritingElement_ReadAttributes_Read_if		(oReader, L"fileType", fileType)
					WritingElement_ReadAttributes_Read_else_if	(oReader, L"codePage", codePage)
					WritingElement_ReadAttributes_Read_else_if	(oReader, L"delimiter", delimiter)
					WritingElement_ReadAttributes_Read_else_if	(oReader, L"Lcid", LcidParam)
					WritingElement_ReadAttributes_Read_else_if	(oReader, L"saveFileType", saveFileType)
					WritingElement_ReadAttributes_Read_else_if  (oReader, L"csvFormat", csvFormat)
				WritingElement_ReadAttributes_End(oReader)
				
				if (csvFormat.IsInit())
				{
					if (AVS_OFFICESTUDIO_FILE_SPREADSHEET_TSV == csvFormat->GetValue()) sDelimiter = L"\t";
					else if (AVS_OFFICESTUDIO_FILE_SPREADSHEET_SCSV == csvFormat->GetValue()) sDelimiter = L";";
				}

				if (fileType.IsInit())
					result = (BYTE)fileType->GetValue();
				if (codePage.IsInit())
					nCodePage = (UINT)codePage->GetValue();
				if (saveFileType.IsInit())
					cSaveFileType = (BYTE)saveFileType->GetValue();
                if(LcidParam.IsInit())
                    Lcid = LcidParam->GetValue();
				if (delimiter.IsInit())
					sDelimiter = delimiter.get();
				break;
			}
		}

		return;
	}
    void ReadActiveSheet(const std::wstring& sXMLOptions, _INT32& nActiveSheet)
	{
		nActiveSheet = -1; // default: no sheet was named, keep the stored activeTab

		nullable<SimpleTypes::CDecimalNumber> activeSheet;

		XmlUtils::CXmlLiteReader oReader;
		if (true != oReader.FromString(sXMLOptions) || true != oReader.IsValid())
			return;

		oReader.ReadNextNode(); // XmlOptions
		if (oReader.IsEmptyNode())
			return;

		int nCurDepth = oReader.GetDepth();
		while (oReader.ReadNextSiblingNode(nCurDepth))
		{
			if (L"fileOptions" == oReader.GetName())
			{
				WritingElement_ReadAttributes_Start(oReader)
					WritingElement_ReadAttributes_Read_if	(oReader, L"activeSheet", activeSheet)
				WritingElement_ReadAttributes_End(oReader)

				if (activeSheet.IsInit() && 0 <= activeSheet->GetValue())
					nActiveSheet = activeSheet->GetValue();

				return;
			}
		}
	}
    void ReadTopLeftCells(const std::wstring& sXMLOptions, std::map<_INT32, std::wstring>& mapTopLeftCells)
	{
		mapTopLeftCells.clear();

		nullable_string topLeftCells;

		XmlUtils::CXmlLiteReader oReader;
		if (true != oReader.FromString(sXMLOptions) || true != oReader.IsValid())
			return;

		oReader.ReadNextNode(); // XmlOptions
		if (oReader.IsEmptyNode())
			return;

		int nCurDepth = oReader.GetDepth();
		while (oReader.ReadNextSiblingNode(nCurDepth))
		{
			if (L"fileOptions" == oReader.GetName())
			{
				WritingElement_ReadAttributes_Start(oReader)
					WritingElement_ReadAttributes_Read_if	(oReader, L"topLeftCells", topLeftCells)
				WritingElement_ReadAttributes_End(oReader)

				break;
			}
		}

		if (false == topLeftCells.IsInit())
			return;

		// "<sheet index>:<A1-style ref>" pairs separated by ';', as built by
		// InputParams::getTopLeftCellsFromJsonParams out of the save parameters.
		const std::wstring& sValue = topLeftCells.get();
		std::wstring::size_type nStart = 0;
		while (nStart <= sValue.length())
		{
			std::wstring::size_type nEnd = sValue.find(L';', nStart);
			if (std::wstring::npos == nEnd)
				nEnd = sValue.length();

			const std::wstring sPair = sValue.substr(nStart, nEnd - nStart);
			nStart = nEnd + 1;

			const std::wstring::size_type nColon = sPair.find(L':');
			if (std::wstring::npos == nColon || 0 == nColon || nColon + 1 == sPair.length())
				continue;

			const std::wstring sIndex = sPair.substr(0, nColon);
			if (sIndex.length() > 9 || sIndex.find_first_not_of(L"0123456789") != std::wstring::npos)
				continue;

			mapTopLeftCells[(_INT32)std::stoi(sIndex)] = sPair.substr(nColon + 1);
		}
	}

	CommentData::CommentData()
	{
		bSolved = false;
		bDocument = false;
	}
	CommentData::~CommentData()
	{
		for(size_t i = 0, length = aReplies.size(); i < length; ++i)
			delete aReplies[i];
		aReplies.clear();
	}
}
