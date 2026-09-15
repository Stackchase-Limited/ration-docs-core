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
#pragma once

#include <string>

//  How a macro assigned to a drawing object survives a round trip through ODF.
//
//  In OOXML the binding is one attribute - xdr:sp/@macro - and for a JSA
//  (ONLYOFFICE) macro its value is "jsaProject_{guid}".  ODF has no standard
//  representation for that guid, so the binding is carried by the ODF element
//  that exists for exactly this purpose - a script bound to an event on a
//  drawing object:
//
//      <office:event-listeners>
//        <script:event-listener script:event-name="dom:click"
//                               script:language="http://schemas.onlyoffice.com/jsa"
//                               xlink:href="vnd.onlyoffice.jsa:jsaProject_{guid}"
//                               xlink:type="simple"/>
//      </office:event-listeners>
//
//  Only two pieces of that are ours, and ODF leaves both to the implementation:
//  script:language is an implementation-defined language identifier (we use a
//  URI in our own namespace, the way LibreOffice uses "ooo:script"), and the
//  xlink:href is an opaque script address in a private URI scheme (the way
//  LibreOffice uses "vnd.sun.star.script:...").  Nothing here overloads a
//  standard ODF value, so a foreign consumer sees a script it cannot run rather
//  than a feature it will misinterpret, and a foreign event-listener - a real
//  Basic macro, say - is ignored by us rather than mistaken for a JSA guid.
//
//  The href carries the OOXML attribute value verbatim after the scheme, so the
//  round trip is literally lossless and neither side has to know how JSA names
//  its projects.

namespace cpdoccore
{
	namespace jsa_macro
	{
		//  the implementation-defined script:language identifier we write
		static const wchar_t * language	= L"http://schemas.onlyoffice.com/jsa";
		//  the private URI scheme the xlink:href uses, including the colon
		static const wchar_t * scheme	= L"vnd.onlyoffice.jsa:";
		//  the event a macro assigned to a shape reacts to
		static const wchar_t * event	= L"dom:click";

		//  OOXML @macro value -> xlink:href
		inline std::wstring to_href(const std::wstring & macro)
		{
			return std::wstring(scheme) + macro;
		}

		//  xlink:href -> OOXML @macro value.  Empty when this listener is not
		//  ours, so a foreign script binding is left alone.
		inline std::wstring from_href(const std::wstring & href, const std::wstring & language_attr)
		{
			const std::wstring prefix(scheme);

			if (href.size() > prefix.size() && 0 == href.compare(0, prefix.size(), prefix))
				return href.substr(prefix.size());

			//  tolerate an href that lost the scheme as long as the language is ours
			if (language_attr == std::wstring(language) && false == href.empty())
				return href;

			return std::wstring();
		}
	}
}
