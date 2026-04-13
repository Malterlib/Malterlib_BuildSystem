// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/CommandLine/TableRenderer>

namespace NMib::NBuildSystem
{
	using namespace NRepository;
	using namespace NStr;

	namespace
	{
		struct CCommentStyleConfig
		{
			CStr m_LinePrefix;
			CStr m_LineSuffix;
			bool m_bDetectShebang = false;
			bool m_bIsPython = false;
			bool m_bIsXml = false;
		};

		struct CCommentStyleLookup
		{
			struct CPatternEntry
			{
				TCVector<CStr> m_PathPatterns;
				CCommentStyleConfig m_Style;
			};

			CCommentStyleLookup(CEJsonSorted const &_License)
			{
				// Built-in styles

				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"// ">}
						,
						{
							gc_Str<"cpp">
							, gc_Str<"h">
							, gc_Str<"hpp">
							, gc_Str<"c">
							, gc_Str<"cxx">
							, gc_Str<"hxx">
							, gc_Str<"inl">
							, gc_Str<"dox">
							, gc_Str<"js">
							, gc_Str<"mjs">
							, gc_Str<"cjs">
							, gc_Str<"ts">
							, gc_Str<"tsx">
							, gc_Str<"jsx">
							, gc_Str<"swift">
							, gc_Str<"java">
							, gc_Str<"kt">
							, gc_Str<"scala">
							, gc_Str<"go">
							, gc_Str<"rs">
							, gc_Str<"cs">
							, gc_Str<"m">
							, gc_Str<"mm">
							, gc_Str<"rc">
						}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"// ">}
						,
						{
							gc_Str<"MSettings">
							, gc_Str<"MHeader">
							, gc_Str<"MTarget">
							, gc_Str<"MRepo">
							, gc_Str<"MBuildSystem">
							, gc_Str<"MConfig">
							, gc_Str<"MRepoState">
							, gc_Str<"MGeneratorSettings">
							, gc_Str<"MInclude">
							, gc_Str<"MLBuildOptions">
						}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"# ">}
						,
						{
							gc_Str<"doxygen">
						}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"# ">, .m_bDetectShebang = true, .m_bIsPython = true}
						, {gc_Str<"py">}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"# ">, .m_bDetectShebang = true}
						,
						{
							gc_Str<"sh">
							, gc_Str<"bash">
							, gc_Str<"zsh">
							, gc_Str<"cmake">
							, gc_Str<"toml">
							, gc_Str<"yml">
							, gc_Str<"yaml">
							, gc_Str<"rb">
							, gc_Str<"pl">
							, gc_Str<"r">
							, gc_Str<"tcl">
							, gc_Str<"conf">
						}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"; ">}
						, {gc_Str<"ini">, gc_Str<"asm">, gc_Str<"nasm">}
					)
				;
				// Use @REM instead of REM so the license header stays silent even when
				// inserted before @echo off (@ suppresses echo regardless of echo state).
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"@REM ">}
						, {gc_Str<"bat">, gc_Str<"cmd">}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"/* ">, .m_LineSuffix = gc_Str<" */">}
						, {gc_Str<"css">, gc_Str<"scss">, gc_Str<"less">}
					)
				;
				fp_AddBuiltin
					(
						{.m_LinePrefix = gc_Str<"<!-- ">, .m_LineSuffix = gc_Str<" -->">, .m_bIsXml = true}
						, {gc_Str<"xml">, gc_Str<"xsl">, gc_Str<"xslt">, gc_Str<"xsd">, gc_Str<"svg">, gc_Str<"xhtml">, gc_Str<"plist">, gc_Str<"html">, gc_Str<"htm">, gc_Str<"natvis">}
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<"CMakeLists.txt">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<"Makefile">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">, .m_bDetectShebang = true}
						, gc_Str<"Dockerfile">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<".clangd">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<".clang-format">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<".clang-tidy">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<".gitignore">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<".lldbinit">
					)
				;
				fp_AddBuiltinFileName
					(
						{.m_LinePrefix = gc_Str<"# ">}
						, gc_Str<".gitattributes">
					)
				;

				// Parse config-defined styles
				auto const *pStyles = _License.f_GetMember("CommentStyles");
				if (pStyles && pStyles->f_IsArray())
				{
					for (auto &Style : pStyles->f_Array())
					{
						auto &Out = mp_PatternStyles.f_Insert();

						auto const *pPaths = Style.f_GetMember("Path");
						if (pPaths && pPaths->f_IsArray())
							Out.m_PathPatterns = pPaths->f_StringArray();

						auto const *pPrefix = Style.f_GetMember("LinePrefix");
						if (pPrefix && pPrefix->f_IsString())
							Out.m_Style.m_LinePrefix = pPrefix->f_String();

						auto const *pSuffix = Style.f_GetMember("LineSuffix");
						if (pSuffix && pSuffix->f_IsString())
							Out.m_Style.m_LineSuffix = pSuffix->f_String();

						auto const *pShebang = Style.f_GetMember("DetectShebang");
						if (pShebang && pShebang->f_IsBoolean())
							Out.m_Style.m_bDetectShebang = pShebang->f_Boolean();

						auto const *pPython = Style.f_GetMember("IsPython");
						if (pPython && pPython->f_IsBoolean())
							Out.m_Style.m_bIsPython = pPython->f_Boolean();

						auto const *pXml = Style.f_GetMember("IsXml");
						if (pXml && pXml->f_IsBoolean())
							Out.m_Style.m_bIsXml = pXml->f_Boolean();
					}
				}
			}

			CCommentStyleConfig const *f_GetStyle(CStr const &_RelativePath, CStr const &_Extension, CStr const &_FileName) const
			{
				// Check config-defined path pattern styles first
				for (auto &Entry : mp_PatternStyles)
				{
					for (auto &Pattern : Entry.m_PathPatterns)
					{
						if (fg_StrMatchWildcard(_RelativePath.f_GetStr(), Pattern.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							return &Entry.m_Style;
					}
				}

				// Fall back to built-in styles by extension
				if (auto const *pStyle = mp_ExtensionMap.f_FindEqual(_Extension))
					return *pStyle;

				// Fall back to built-in styles by filename
				if (auto const *pStyle = mp_FileNameMap.f_FindEqual(_FileName))
					return *pStyle;

				return nullptr;
			}

		private:
			void fp_AddBuiltin(CCommentStyleConfig _Style, std::initializer_list<CStr> _Extensions)
			{
				auto &Out = mp_BuiltinStyles.f_Insert(fg_Move(_Style));

				for (auto &Ext : _Extensions)
					mp_ExtensionMap[Ext] = &Out;
			}

			void fp_AddBuiltinFileName(CCommentStyleConfig _Style, CStr const &_FileName)
			{
				auto &Out = mp_BuiltinStyles.f_Insert(fg_Move(_Style));
				mp_FileNameMap[_FileName] = &Out;
			}

			TCVector<CPatternEntry> mp_PatternStyles;
			TCLinkedList<CCommentStyleConfig> mp_BuiltinStyles;
			TCMap<CStr, CCommentStyleConfig const *> mp_ExtensionMap;
			TCMap<CStr, CCommentStyleConfig const *> mp_FileNameMap;
		};

		struct CHeaderLookup
		{
			struct COverrideEntry
			{
				TCVector<CStr> m_PathPatterns;
				TCVector<CStr> m_HeaderLines;
			};

			CHeaderLookup(CEJsonSorted const &_License)
			{
				auto const *pHeader = _License.f_GetMember("Header");
				if (pHeader && pHeader->f_IsArray() && !pHeader->f_Array().f_IsEmpty())
				{
					m_DefaultHeaderLines = pHeader->f_StringArray();
					m_bHasDefault = true;
				}

				auto const *pOverrides = _License.f_GetMember("HeaderOverrides");
				if (pOverrides && pOverrides->f_IsArray())
				{
					for (auto &Override : pOverrides->f_Array())
					{
						auto &Entry = mp_Overrides.f_Insert();

						auto const *pPaths = Override.f_GetMember("Path");
						if (pPaths && pPaths->f_IsArray())
							Entry.m_PathPatterns = pPaths->f_StringArray();

						auto const *pHeaderLines = Override.f_GetMember("Header");
						if (pHeaderLines && pHeaderLines->f_IsArray())
							Entry.m_HeaderLines = pHeaderLines->f_StringArray();
					}
				}
			}

			TCVector<CStr> const *f_GetHeaderLines(CStr const &_RelativePath) const
			{
				for (auto &Entry : mp_Overrides)
				{
					for (auto &Pattern : Entry.m_PathPatterns)
					{
						if (fg_StrMatchWildcard(_RelativePath.f_GetStr(), Pattern.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							return &Entry.m_HeaderLines;
					}
				}

				if (m_bHasDefault)
					return &m_DefaultHeaderLines;

				return nullptr;
			}

			bool m_bHasDefault = false;
			TCVector<CStr> m_DefaultHeaderLines;

		private:
			TCVector<COverrideEntry> mp_Overrides;
		};

		CStr fg_FormatHeader(TCVector<CStr> const &_HeaderLines, CCommentStyleConfig const &_Style, CStr const &_LineEnding)
		{
			CStr Result;
			for (auto &Line : _HeaderLines)
			{
				Result += _Style.m_LinePrefix;
				Result += Line;
				Result += _Style.m_LineSuffix;
				Result += _LineEnding;
			}
			return Result;
		}

		CStr fg_DetectLineEnding(CStr const &_Content)
		{
			auto iCR = _Content.f_Find("\r");
			if (iCR != -1 && iCR + 1 < (aint)_Content.f_GetLen() && _Content[iCR + 1] == '\n')
				return "\r\n";
			return "\n";
		}

		// Check if a line is a PEP 263 encoding declaration.
		// Valid forms: "# -*- coding: ... -*-", "# coding: ...", "# coding=...", "# vim: set fileencoding=..."
		// The line must be a comment containing "coding" followed by ":" or "=".
		bool fg_IsPythonEncodingCookie(CStr const &_Line)
		{
			if (!_Line.f_StartsWith("#"))
				return false;
			auto iCoding = _Line.f_Find("coding");
			if (iCoding == -1)
				return false;
			// PEP 263 requires "coding" to be followed by ":" or "="
			umint iAfter = iCoding + 6;
			if (iAfter >= _Line.f_GetLen())
				return false;
			ch8 Next = _Line[iAfter];
			return Next == ':' || Next == '=';
		}

		// Find where the header should be inserted (after shebang and encoding lines)
		umint fg_FindHeaderInsertLine(TCVector<CStr> const &_Lines, CCommentStyleConfig const &_StyleConfig)
		{
			umint iLine = 0;

			if (_Lines.f_IsEmpty())
				return 0;

			// Skip shebang line
			if (_StyleConfig.m_bDetectShebang && _Lines[0].f_StartsWith("#!"))
			{
				iLine = 1;
				// For Python, also skip encoding declaration (PEP 263)
				if (_StyleConfig.m_bIsPython && _Lines.f_GetLen() > 1 && fg_IsPythonEncodingCookie(_Lines[1]))
					iLine = 2;
			}
			// For Python without shebang, skip encoding declaration on first line (PEP 263).
			// PEP 263 restricts the cookie to physical line 1 or 2, and line 2 is only
			// valid when line 1 is a shebang.  Therefore, without a shebang, a cookie on
			// any line other than line 1 (index 0) is already ignored by the interpreter
			// and does not need to be preserved.
			else if (_StyleConfig.m_bIsPython && fg_IsPythonEncodingCookie(_Lines[0]))
			{
				iLine = 1;
			}

			// Skip XML declaration (<?xml ...?>)
			if (_StyleConfig.m_bIsXml && _Lines[0].f_Trim().f_StartsWith("<?xml"))
				iLine = 1;

			return iLine;
		}

		// Check if a line is a copyright or license comment line.
		// Note: This uses a broad substring match on "Copyright" which could in theory match
		// unrelated comments (e.g. "/* Copyright utilities */"). In practice this is not a
		// problem because the tool only looks at the first few lines of a file, and any
		// false positive would be visible in the git diff after --fix, allowing manual correction.
		bool fg_IsCopyrightOrLicenseLine(CStr const &_Line)
		{
			if (_Line.f_Find("Copyright") != -1)
				return true;
			if (_Line.f_Find("SPDX-License-Identifier") != -1)
				return true;
			if (_Line.f_Find("Distributed under the") != -1 && _Line.f_Find("license") != -1)
				return true;
			return false;
		}

		// Check if a line is a comment in the given style
		bool fg_IsCommentLine(CStr const &_Line, CCommentStyleConfig const &_Style)
		{
			CStr Trimmed = _Line.f_Trim();
			if (Trimmed.f_IsEmpty())
				return false;
			if (!Trimmed.f_StartsWith(_Style.m_LinePrefix.f_Trim()))
				return false;
			if (!_Style.m_LineSuffix.f_IsEmpty() && !Trimmed.f_EndsWith(_Style.m_LineSuffix.f_Trim()))
				return false;
			return true;
		}

		enum class EHeaderStatus
		{
			mc_Correct,
			mc_Missing,
			mc_Wrong
		};

		struct CHeaderCheckResult
		{
			EHeaderStatus m_Status = EHeaderStatus::mc_Missing;
			umint m_HeaderStartLine = 0;
			umint m_HeaderEndLine = 0; // exclusive
		};

		CHeaderCheckResult fg_CheckHeader
			(
				TCVector<CStr> const &_Lines
				, TCVector<CStr> const &_HeaderLines
				, CCommentStyleConfig const &_StyleConfig
				, CStr const &_LineEnding
			)
		{
			CHeaderCheckResult Result;
			Result.m_HeaderStartLine = fg_FindHeaderInsertLine(_Lines, _StyleConfig);

			if (Result.m_HeaderStartLine >= _Lines.f_GetLen())
			{
				Result.m_Status = EHeaderStatus::mc_Missing;
				Result.m_HeaderEndLine = Result.m_HeaderStartLine;
				return Result;
			}

			// Look at lines starting from the insert position
			umint iLine = Result.m_HeaderStartLine;
			bool bFoundCopyright = false;
			bool bFoundLicense = false;
			bool bInBlockComment = false;
			bool bBlockHasCopyright = false;

			// Scan through comment lines that might be a header
			while (iLine < _Lines.f_GetLen() && iLine < Result.m_HeaderStartLine + 10)
			{
				CStr const &Line = _Lines[iLine];
				CStr Trimmed = Line.f_Trim();

				bool bIsComment = fg_IsCommentLine(Line, _StyleConfig);

				// Track block comments (/* ... */ and <!-- ... -->).
				// Note: if the closing delimiter shares a line with real content (e.g. "--><svg>"),
				// that content will be included in the header range and removed by --fix. This is
				// acceptable because such formatting is pathological; the user can spot it in the
				// diff and correct manually.
				bool bBlockStart = Trimmed.f_StartsWith("/*") || (_StyleConfig.m_bIsXml && Trimmed.f_StartsWith("<!--"));
				if (!bInBlockComment && bBlockStart)
				{
					// If we've already found copyright/license and this line
					// is not a valid comment in the expected style (e.g. a multi-line
					// block comment following // line comments), stop scanning -
					// it's separate content that should be preserved
					if ((bFoundCopyright || bFoundLicense) && !bIsComment)
						break;

					bInBlockComment = true;
					bBlockHasCopyright = false;
				}

				if (bInBlockComment)
					bIsComment = true;

				if (bInBlockComment && fg_IsCopyrightOrLicenseLine(Trimmed))
					bBlockHasCopyright = true;

				bool bBlockCommentEnded = false;
				if (bInBlockComment && (Trimmed.f_Find("*/") != -1 || (_StyleConfig.m_bIsXml && Trimmed.f_Find("-->") != -1)))
				{
					bInBlockComment = false;
					bBlockCommentEnded = true;
				}

				// Stop at non-comment, non-empty lines (but allow blank lines within header)
				if (!Trimmed.f_IsEmpty() && !bIsComment)
					break;

				if (Trimmed.f_Find("Copyright") != -1)
					bFoundCopyright = true;

				if (Trimmed.f_Find("SPDX-License-Identifier") != -1)
					bFoundLicense = true;

				if (fg_IsCopyrightOrLicenseLine(Trimmed) || (bBlockCommentEnded && bBlockHasCopyright))
					Result.m_HeaderEndLine = iLine + 1;

				++iLine;
			}

			if (!bFoundCopyright && !bFoundLicense)
			{
				Result.m_Status = EHeaderStatus::mc_Missing;
				Result.m_HeaderEndLine = Result.m_HeaderStartLine;
				return Result;
			}

			// Extend end line to include trailing comment lines that are part of the
			// header block (e.g. an empty comment line or additional text after the
			// copyright/license lines). Without this, headers with trailing lines would
			// always be classified as wrong and --fix would leave orphaned old lines.
			while
			(
				Result.m_HeaderEndLine < _Lines.f_GetLen()
				&& Result.m_HeaderEndLine < Result.m_HeaderStartLine + 10
			)
			{
				CStr Trimmed = _Lines[Result.m_HeaderEndLine].f_Trim();
				if (Trimmed.f_IsEmpty() || !fg_IsCommentLine(_Lines[Result.m_HeaderEndLine], _StyleConfig))
					break;
				++Result.m_HeaderEndLine;
			}

			// Check if the existing header is correct
			CStr ExpectedHeader = fg_FormatHeader(_HeaderLines, _StyleConfig, _LineEnding);
			CStr ActualHeader;
			for (umint i = Result.m_HeaderStartLine; i < Result.m_HeaderEndLine; ++i)
			{
				ActualHeader += _Lines[i];
				ActualHeader += _LineEnding;
			}

			if (ActualHeader == ExpectedHeader)
				Result.m_Status = EHeaderStatus::mc_Correct;
			else
				Result.m_Status = EHeaderStatus::mc_Wrong;

			return Result;
		}

		CStr fg_FixHeader
			(
				TCVector<CStr> const &_Lines
				, CHeaderCheckResult const &_CheckResult
				, TCVector<CStr> const &_HeaderLines
				, CCommentStyleConfig const &_StyleConfig
				, CStr const &_LineEnding
			)
		{
			CStr Header = fg_FormatHeader(_HeaderLines, _StyleConfig, _LineEnding);
			CStr Result;

			// Lines before the header (e.g. shebang)
			for (umint i = 0; i < _CheckResult.m_HeaderStartLine; ++i)
			{
				Result += _Lines[i];
				Result += _LineEnding;
			}

			// Insert new header
			Result += Header;

			// Add blank line after header if the next line isn't blank
			umint iAfterHeader = _CheckResult.m_HeaderEndLine;
			if (iAfterHeader < _Lines.f_GetLen() && !_Lines[iAfterHeader].f_Trim().f_IsEmpty())
				Result += _LineEnding;

			// Rest of the file
			for (umint i = iAfterHeader; i < _Lines.f_GetLen(); ++i)
			{
				if (i > iAfterHeader)
					Result += _LineEnding;
				Result += _Lines[i];
			}

			return Result;
		}

		bool fg_FileMatchesWildcards(CStr const &_RelativePath, TCVector<CStr> const &_Wildcards)
		{
			for (auto &Wildcard : _Wildcards)
			{
				if (fg_StrMatchWildcard(_RelativePath.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					return true;
				// Also match just the filename
				CStr FileName = CFile::fs_GetFile(_RelativePath);
				if (fg_StrMatchWildcard(FileName.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					return true;
			}
			return false;
		}

		// Include/Exclude defaults and CLicenseConfig type are defined in Core/Build/Shared_RepositoryLicense.MSettings
		bool fg_FileInScope(CStr const &_RelativePath, CEJsonSorted const &_License)
		{
			auto const *pInclude = _License.f_GetMember("Include");
			auto const *pExclude = _License.f_GetMember("Exclude");

			// Check include patterns (if specified, file must match at least one)
			if (pInclude && pInclude->f_IsArray() && !pInclude->f_Array().f_IsEmpty())
			{
				auto IncludePatterns = pInclude->f_StringArray();
				if (!fg_FileMatchesWildcards(_RelativePath, IncludePatterns))
					return false;
			}

			// Check exclude patterns
			if (pExclude && pExclude->f_IsArray() && !pExclude->f_Array().f_IsEmpty())
			{
				auto ExcludePatterns = pExclude->f_StringArray();
				if (fg_FileMatchesWildcards(_RelativePath, ExcludePatterns))
					return false;
			}

			return true;
		}

		bool fg_FileIsCoveredByReuse(CStr const &_RelativePath, CEJsonSorted const &_License)
		{
			auto const *pAnnotations = _License.f_GetMember("ReuseAnnotations");
			if (!pAnnotations || !pAnnotations->f_IsArray())
				return false;

			for (auto &Annotation : pAnnotations->f_Array())
			{
				auto const *pPaths = Annotation.f_GetMember("Path");
				if (!pPaths || !pPaths->f_IsArray())
					continue;

				for (auto &PathPattern : pPaths->f_Array())
				{
					if (fg_StrMatchWildcard(_RelativePath.f_GetStr(), PathPattern.f_String().f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						return true;
				}
			}

			return false;
		}

		// Convert Malterlib wildcard patterns to REUSE glob patterns.
		// Malterlib * matches across path separators (like glob **).
		CStr fg_ToReuseGlob(CStr const &_Pattern)
		{
			return _Pattern.f_Replace("*", "**");
		}

		CStr fg_GenerateReuseToml(CEJsonSorted const &_License)
		{
			auto const *pAnnotations = _License.f_GetMember("ReuseAnnotations");
			if (!pAnnotations || !pAnnotations->f_IsArray() || pAnnotations->f_Array().f_IsEmpty())
				return {};

			CStr Result = "version = 1\n";

			for (auto &Annotation : pAnnotations->f_Array())
			{
				Result += "\n[[annotations]]\n";

				auto const *pPaths = Annotation.f_GetMember("Path");
				if (pPaths && pPaths->f_IsArray())
				{
					if (pPaths->f_Array().f_GetLen() == 1)
						Result += "path = \"{}\"\n"_f << fg_ToReuseGlob(pPaths->f_Array()[0].f_String());
					else
					{
						Result += "path = [";
						bool bFirst = true;
						for (auto &Path : pPaths->f_Array())
						{
							if (!bFirst)
								Result += ", ";
							Result += "\"{}\""_f << fg_ToReuseGlob(Path.f_String());
							bFirst = false;
						}
						Result += "]\n";
					}
				}

				auto const *pPrecedence = Annotation.f_GetMember("Precedence");
				if (pPrecedence && pPrecedence->f_IsString() && !pPrecedence->f_String().f_IsEmpty())
					Result += "precedence = \"{}\"\n"_f << pPrecedence->f_String();

				auto const *pCopyright = Annotation.f_GetMember("CopyrightText");
				if (pCopyright && pCopyright->f_IsString())
					Result += "SPDX-FileCopyrightText = \"{}\"\n"_f << pCopyright->f_String();

				auto const *pLicense = Annotation.f_GetMember("LicenseIdentifier");
				if (pLicense && pLicense->f_IsString())
					Result += "SPDX-License-Identifier = \"{}\"\n"_f << pLicense->f_String();
			}

			return Result;
		}

		struct CLicenseStats
		{
			umint m_nFilesChecked = 0;
			umint m_nFilesFixed = 0;
			umint m_nFilesNeedFix = 0;
			umint m_nFilesSkipped = 0;
			umint m_nFilesNoCommentStyle = 0;
			umint m_nErrors = 0;
			umint m_nLicenseFilesIssues = 0;
			umint m_nReuseTomlIssues = 0;
			umint m_nDisallowedFiles = 0;
			umint m_nFilesCoveredByReuse = 0;

			CLicenseStats &operator += (CLicenseStats const &_Other)
			{
				m_nFilesChecked += _Other.m_nFilesChecked;
				m_nFilesFixed += _Other.m_nFilesFixed;
				m_nFilesNeedFix += _Other.m_nFilesNeedFix;
				m_nFilesSkipped += _Other.m_nFilesSkipped;
				m_nFilesNoCommentStyle += _Other.m_nFilesNoCommentStyle;
				m_nErrors += _Other.m_nErrors;
				m_nLicenseFilesIssues += _Other.m_nLicenseFilesIssues;
				m_nReuseTomlIssues += _Other.m_nReuseTomlIssues;
				m_nDisallowedFiles += _Other.m_nDisallowedFiles;
				m_nFilesCoveredByReuse += _Other.m_nFilesCoveredByReuse;
				return *this;
			}
		};

		struct CRepoLicenseResult
		{
			CStr m_RepoName;
			bool m_bCheckLicense = false;
			TCVector<CStr> m_Messages;
			CLicenseStats m_Stats;
		};
	}

	// Called from Tool module: Malterlib_Tool_App_MTool_Malterlib_CheckLicense.cpp (check-license CLI command)
	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_CheckLicense
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, CBuildSystem::ECheckLicenseFlag _Flags
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_IncludeLicense);

		bool bFix = fg_IsSet(_Flags, CBuildSystem::ECheckLicenseFlag::mc_Fix);
		bool bShowAll = fg_IsSet(_Flags, CBuildSystem::ECheckLicenseFlag::mc_ShowAll);

		CGitLaunches Launches{f_GetGitLaunchOptions("check-license"), "Checking licenses"};
		auto DestroyLaunches = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		struct CProcessRepo
		{
			CRepository *m_pRepository;
			TCPromise<CRepoLicenseResult> m_Promise;
		};

		TCFutureVector<CRepoLicenseResult> RepoFutures;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCVector<CProcessRepo> ToProcess;

			for (auto &pRepo : Repos)
			{
				TCPromiseFuturePair<CRepoLicenseResult> Promise;
				ToProcess.f_Insert(CProcessRepo{.m_pRepository = pRepo, .m_Promise = fg_Move(Promise.m_Promise)});
				fg_Move(Promise.m_Future) > RepoFutures;
			}

			co_await fg_ParallelForEach
				(
					ToProcess
					, [&](CProcessRepo &_Process) -> TCUnsafeFuture<void>
					{
						co_await ECoroutineFlag_CaptureMalterlibExceptions;

						CRepoLicenseResult Result;
						auto &Repo = *_Process.m_pRepository;
						Result.m_RepoName = Launches.f_GetRepoName(Repo);
						Result.m_bCheckLicense = Repo.m_bCheckLicense;

						auto fDone = [&]()
							{
								Launches.f_RepoDone();
								_Process.m_Promise.f_SetResult(fg_Move(Result));
							}
						;

						if (!Repo.m_bCheckLicense)
						{
							fDone();
							co_return {};
						}

						auto &License = Repo.m_License;

						CHeaderLookup HeaderLookup(License);
						CCommentStyleLookup StyleLookup(License);

						// Get tracked and untracked files via git ls-files
						auto LsFilesResult = co_await Launches.f_Launch(Repo, {"ls-files", "--cached", "--others", "--exclude-standard"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
						if (LsFilesResult.m_ExitCode != 0)
						{
							Result.m_Messages.f_Insert("{}Error:{} Failed to list files"_f << AnsiEncoding.f_StatusError() << AnsiEncoding.f_Default());
							++Result.m_Stats.m_nErrors;
							fDone();
							co_return {};
						}

						auto TrackedFiles = LsFilesResult.f_GetStdOut().f_SplitLine<true>();

						// Check/copy license files
						auto const *pLicenseFiles = License.f_GetMember("LicenseFiles");
						if (pLicenseFiles && pLicenseFiles->f_IsObject())
						{
							for (auto &Entry : pLicenseFiles->f_Object())
							{
								CStr Destination = Entry.f_Key();
								auto &Value = Entry.f_Value();
								CStr DestPath = CFile::fs_AppendPath(Repo.m_Location, Destination);

								// Build expected content: either single source (string) or amalgamation (array)
								CStr ExpectedContent;
								bool bHasError = false;

								if (Value.f_IsString())
								{
									// Single source file copy
									CStr SourcePath = Value.f_String();
									if (!CFile::fs_IsPathAbsolute(SourcePath))
									{
										Result.m_Messages.f_Insert
											(
												"{}Error:{} Source path must be absolute (use ->MakeAbsolute()): {}"_f
												<< AnsiEncoding.f_StatusError()
												<< AnsiEncoding.f_Default()
												<< SourcePath
											)
										;
										++Result.m_Stats.m_nErrors;
										continue;
									}
									if (!CFile::fs_FileExists(SourcePath))
									{
										Result.m_Messages.f_Insert("{}Error:{} Source file not found: {}"_f << AnsiEncoding.f_StatusError() << AnsiEncoding.f_Default() << SourcePath);
										++Result.m_Stats.m_nErrors;
										continue;
									}
									auto BlockingActorCheckout = fg_BlockingActor();
									ExpectedContent = co_await
										(
											g_Dispatch(BlockingActorCheckout) / [SourcePath]() -> CStr
											{
												return CFile::fs_ReadStringFromFile(SourcePath, true);
											}
										)
									;
								}
								else if (Value.f_IsArray())
								{
									// Amalgamation: concatenate sources with optional Name headers
									for (auto &Entry : Value.f_Array())
									{
										CStr SourcePath = Entry["Source"].f_String();
										auto const *pName = Entry.f_GetMember("Name");

										if (!CFile::fs_IsPathAbsolute(SourcePath))
										{
											Result.m_Messages.f_Insert
												(
													"{}Error:{} Source path must be absolute (use ->MakeAbsolute()): {}"_f
													<< AnsiEncoding.f_StatusError()
													<< AnsiEncoding.f_Default()
													<< SourcePath
												)
											;
											++Result.m_Stats.m_nErrors;
											bHasError = true;
											break;
										}
										if (!CFile::fs_FileExists(SourcePath))
										{
											Result.m_Messages.f_Insert("{}Error:{} Source file not found: {}"_f << AnsiEncoding.f_StatusError() << AnsiEncoding.f_Default() << SourcePath);
											++Result.m_Stats.m_nErrors;
											bHasError = true;
											break;
										}

										auto BlockingActorCheckout = fg_BlockingActor();
										CStr SourceContent = co_await
											(
												g_Dispatch(BlockingActorCheckout) / [SourcePath]() -> CStr
												{
													return CFile::fs_ReadStringFromFile(SourcePath, true);
												}
											)
										;

										if (!ExpectedContent.f_IsEmpty())
											ExpectedContent += "\n";

										if (pName && pName->f_IsString() && !pName->f_String().f_IsEmpty())
										{
											ExpectedContent += "--- ";
											ExpectedContent += pName->f_String();
											ExpectedContent += " ---\n\n";
										}

										ExpectedContent += SourceContent;
									}
								}

								if (bHasError)
									continue;

								// Compare with existing
								auto BlockingActorCheckout = fg_BlockingActor();
								bool bMatch = co_await
									(
										g_Dispatch(BlockingActorCheckout) / [DestPath, ExpectedContent]() -> bool
										{
											if (!CFile::fs_FileExists(DestPath))
												return false;
											CStr DestContent = CFile::fs_ReadStringFromFile(DestPath, true);
											return DestContent == ExpectedContent;
										}
									)
								;

								if (!bMatch)
								{
									++Result.m_Stats.m_nLicenseFilesIssues;
									if (bFix)
									{
										auto BlockingActorCheckout2 = fg_BlockingActor();
										co_await
											(
												g_Dispatch(BlockingActorCheckout2) / [DestPath, ExpectedContent]()
												{
													CFile::fs_CreateDirectoryForFile(DestPath);
													CFile::fs_WriteStringToFile(DestPath, ExpectedContent, false);
												}
											)
										;
										Result.m_Messages.f_Insert("{}Fixed:{} Updated license file {}"_f << AnsiEncoding.f_StatusNormal() << AnsiEncoding.f_Default() << Destination);
									}
									else
									{
										Result.m_Messages.f_Insert
											(
												"{}Mismatch:{} License file {} needs updating"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default() << Destination
											)
										;
									}
								}
							}
						}

						// Check/generate REUSE.toml
						CStr ExpectedReuseToml = fg_GenerateReuseToml(License);
						if (!ExpectedReuseToml.f_IsEmpty())
						{
							CStr ReuseTomlPath = CFile::fs_AppendPath(Repo.m_Location, "REUSE.toml");

							auto BlockingActorCheckout = fg_BlockingActor();
							bool bReuseMatch = co_await
								(
									g_Dispatch(BlockingActorCheckout) / [ReuseTomlPath, ExpectedReuseToml]()
									{
										if (!CFile::fs_FileExists(ReuseTomlPath))
											return false;
										CStr Existing = CFile::fs_ReadStringFromFile(ReuseTomlPath, true);
										return Existing == ExpectedReuseToml;
									}
								)
							;

							if (!bReuseMatch)
							{
								++Result.m_Stats.m_nReuseTomlIssues;
								if (bFix)
								{
									auto BlockingActorCheckout2 = fg_BlockingActor();
									co_await
										(
											g_Dispatch(BlockingActorCheckout2) / [ReuseTomlPath, ExpectedReuseToml]()
											{
												CFile::fs_WriteStringToFile(ReuseTomlPath, ExpectedReuseToml, false);
											}
										)
									;
									Result.m_Messages.f_Insert("{}Fixed:{} Generated REUSE.toml"_f << AnsiEncoding.f_StatusNormal() << AnsiEncoding.f_Default());
								}
								else
									Result.m_Messages.f_Insert("{}Mismatch:{} REUSE.toml needs updating"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default());
							}
						}
						else if (auto const *pAnnotations = License.f_GetMember("ReuseAnnotations"); pAnnotations && pAnnotations->f_IsArray() && pAnnotations->f_Array().f_IsEmpty())
						{
							// ReuseAnnotations is an explicitly empty array — flag any
							// leftover REUSE.toml as stale.  When the key is absent,
							// undefined, or a non-array type the repo may have a
							// hand-maintained REUSE.toml that should be left alone.
							CStr ReuseTomlPath = CFile::fs_AppendPath(Repo.m_Location, "REUSE.toml");

							auto BlockingActorCheckout = fg_BlockingActor();
							bool bStaleExists = co_await
								(
									g_Dispatch(BlockingActorCheckout) / [ReuseTomlPath]()
									{
										return CFile::fs_FileExists(ReuseTomlPath);
									}
								)
							;

							if (bStaleExists)
							{
								++Result.m_Stats.m_nReuseTomlIssues;
								if (bFix)
								{
									auto BlockingActorCheckout2 = fg_BlockingActor();
									co_await
										(
											g_Dispatch(BlockingActorCheckout2) / [ReuseTomlPath]()
											{
												CFile::fs_DeleteFile(ReuseTomlPath);
											}
										)
									;
									Result.m_Messages.f_Insert("{}Fixed:{} Removed stale REUSE.toml"_f << AnsiEncoding.f_StatusNormal() << AnsiEncoding.f_Default());
								}
								else
									Result.m_Messages.f_Insert("{}Stale:{} REUSE.toml exists but ReuseAnnotations is empty"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default());
							}
						}

						// Check for disallowed files
						TCSet<CStr> DisallowedFiles;
						auto const *pDisallowed = License.f_GetMember("Disallowed");
						if (pDisallowed && pDisallowed->f_IsArray() && !pDisallowed->f_Array().f_IsEmpty())
						{
							auto DisallowedPatterns = pDisallowed->f_StringArray();
							for (auto &RelativePath : TrackedFiles)
							{
								if (RelativePath.f_IsEmpty())
									continue;

								if (!fg_FileMatchesWildcards(RelativePath, DisallowedPatterns))
									continue;

								CStr FullPath = CFile::fs_AppendPath(Repo.m_Location, RelativePath);
								if (!CFile::fs_FileExists(FullPath, EFileAttrib_File))
									continue;

								DisallowedFiles.f_Insert(RelativePath);
								++Result.m_Stats.m_nDisallowedFiles;
								if (bFix)
								{
									auto BlockingActorCheckout = fg_BlockingActor();
									co_await
										(
											g_Dispatch(BlockingActorCheckout) / [FullPath]()
											{
												CFile::fs_DeleteFile(FullPath);
											}
										)
									;
									Result.m_Messages.f_Insert("{}Deleted:{} Disallowed file {}"_f << AnsiEncoding.f_StatusNormal() << AnsiEncoding.f_Default() << RelativePath);
								}
								else
									Result.m_Messages.f_Insert("{}Disallowed:{} {}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default() << RelativePath);
							}
						}

						// Process tracked files for license headers
						for (auto &RelativePath : TrackedFiles)
						{
							if (RelativePath.f_IsEmpty())
								continue;

							if (DisallowedFiles.f_FindEqual(RelativePath))
								continue;

							if (!fg_FileInScope(RelativePath, License))
							{
								++Result.m_Stats.m_nFilesSkipped;
								continue;
							}

							if (fg_FileIsCoveredByReuse(RelativePath, License))
							{
								++Result.m_Stats.m_nFilesCoveredByReuse;
								continue;
							}

							CStr FullPath = CFile::fs_AppendPath(Repo.m_Location, RelativePath);

							auto BlockingActorCheckout = fg_BlockingActor();
							auto FileContent = co_await
								(
									g_Dispatch(BlockingActorCheckout) / [FullPath]() -> TCOptional<CStr>
									{
										if (!CFile::fs_FileExists(FullPath, EFileAttrib_File))
											return {};
										auto Attribs = CFile::fs_GetAttributesOnLink(FullPath);
										if (Attribs & EFileAttrib_Link)
											return {};
										return CFile::fs_ReadStringFromFile(FullPath, true);
									}
								)
							;

							if (!FileContent)
								continue;

							CStr FileName = CFile::fs_GetFile(RelativePath);
							CStr Extension = CFile::fs_GetExtension(RelativePath);
							CCommentStyleConfig const *pStyle = StyleLookup.f_GetStyle(RelativePath, Extension, FileName);
							if (!pStyle)
							{
								Result.m_Messages.f_Insert("{}Warning:{} No comment style for: {}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default() << RelativePath);
								++Result.m_Stats.m_nFilesNoCommentStyle;
								continue;
							}

							auto const *pHeaderLines = HeaderLookup.f_GetHeaderLines(RelativePath);
							if (!pHeaderLines)
							{
								Result.m_Messages.f_Insert
									(
										"{}Warning:{} CheckLicense enabled but no Header configured: {}"_f
										<< AnsiEncoding.f_StatusWarning()
										<< AnsiEncoding.f_Default()
										<< RelativePath
									)
								;
								++Result.m_Stats.m_nErrors;
								continue;
							}

							++Result.m_Stats.m_nFilesChecked;

							auto LineEnding = fg_DetectLineEnding(*FileContent);
							auto Lines = (*FileContent).f_SplitLine<false>();
							auto CheckResult = fg_CheckHeader(Lines, *pHeaderLines, *pStyle, LineEnding);

							if (CheckResult.m_Status == EHeaderStatus::mc_Correct)
								continue;

							++Result.m_Stats.m_nFilesNeedFix;

							if (bFix)
							{
								CStr FixedContent = fg_FixHeader(Lines, CheckResult, *pHeaderLines, *pStyle, LineEnding);

								auto BlockingActorCheckout2 = fg_BlockingActor();
								co_await
									(
										g_Dispatch(BlockingActorCheckout2) / [FullPath, FixedContent]()
										{
											auto Attribs = CFile::fs_GetAttributes(FullPath);
											CFile::fs_WriteStringToFile(FullPath, FixedContent, false, Attribs);
										}
									)
								;
								++Result.m_Stats.m_nFilesFixed;
								CStr StatusStr = CheckResult.m_Status == EHeaderStatus::mc_Missing ? "missing" : "wrong";
								Result.m_Messages.f_Insert("{}Fixed {} header:{} {}"_f << AnsiEncoding.f_StatusNormal() << StatusStr << AnsiEncoding.f_Default() << RelativePath);
							}
							else
							{
								CStr StatusStr = CheckResult.m_Status == EHeaderStatus::mc_Missing ? "Missing" : "Wrong";
								Result.m_Messages.f_Insert("{}{} header:{} {}"_f << AnsiEncoding.f_StatusWarning() << StatusStr << AnsiEncoding.f_Default() << RelativePath);
							}
						}

						fDone();
						co_return {};
					}
					, mp_bSingleThreaded
				)
			;
		}

		// Collect all results
		auto AllResults = co_await fg_AllDone(RepoFutures);

		// Sort results by repo name
		AllResults.f_Sort
			(
				[](CRepoLicenseResult const &_Left, CRepoLicenseResult const &_Right)
				{
					return _Left.m_RepoName <=> _Right.m_RepoName;
				}
			)
		;

		// Render results table (only repos with output)
		{
			CColors Colors(mp_AnsiFlags);

			auto TableRenderer = _pCommandLine->f_TableRenderer();
			TableRenderer.f_SetOptions(CTableRenderHelper::EOption_AvoidRowSeparators | CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_NoExtraLines);
			TableRenderer.f_AddHeadings("Repository", "Status");

			bool bHasRows = false;
			for (auto &Result : AllResults)
			{
				bool bHasIssues = !Result.m_Messages.f_IsEmpty();

				if (!bHasIssues && !bShowAll)
					continue;

				if (!Result.m_bCheckLicense && !bShowAll)
					continue;

				CStr RepoColor;
				if (bHasIssues)
				{
					bool bHasErrors = Result.m_Stats.m_nErrors > 0;
					RepoColor = bHasErrors ? Colors.f_StatusError() : Colors.f_StatusWarning();
				}
				else
					RepoColor = Colors.f_StatusNormal();

				CStr ColoredRepo = RepoColor + Result.m_RepoName.f_Replace("/", "{}{}/{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor ^ 1) + Colors.f_Default();

				if (bHasIssues)
					TableRenderer.f_AddRow(ColoredRepo, CStr::fs_Join(Result.m_Messages, "\n"));
				else if (!Result.m_bCheckLicense)
					TableRenderer.f_AddRow(ColoredRepo, "Not configured");
				else
					TableRenderer.f_AddRow(ColoredRepo, "{} files checked"_f << Result.m_Stats.m_nFilesChecked);

				bHasRows = true;
			}

			if (bHasRows)
				TableRenderer.f_Output(CTableRenderHelper::EOutputType_HumanReadable);
		}

		// Compute totals
		CLicenseStats Totals;
		for (auto &Result : AllResults)
			Totals += Result.m_Stats;

		// Render summary table
		{
			auto TableRenderer = _pCommandLine->f_TableRenderer();
			TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_NoExtraLines);
			TableRenderer.f_AddHeadings("Metric", "Count");
			TableRenderer.f_SetAlignRight(1);

			TableRenderer.f_AddRow("Files checked", "{}"_f << Totals.m_nFilesChecked);
			TableRenderer.f_AddRow("Files covered by REUSE", "{}"_f << Totals.m_nFilesCoveredByReuse);
			TableRenderer.f_AddRow("Files skipped", "{}"_f << Totals.m_nFilesSkipped);
			if (Totals.m_nFilesNoCommentStyle > 0)
			{
				TableRenderer.f_AddRow
					(
						"{}Files with no comment style{}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default()
						, "{}{}{}"_f << AnsiEncoding.f_StatusWarning() << Totals.m_nFilesNoCommentStyle << AnsiEncoding.f_Default()
					)
				;
			}

			if (bFix)
				TableRenderer.f_AddRow("Files fixed", "{}"_f << Totals.m_nFilesFixed);
			else if (Totals.m_nFilesNeedFix > 0)
			{
				TableRenderer.f_AddRow
					(
						"{}Files needing fix{}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default()
						, "{}{}{}"_f << AnsiEncoding.f_StatusWarning() << Totals.m_nFilesNeedFix << AnsiEncoding.f_Default()
					)
				;
			}

			if (Totals.m_nLicenseFilesIssues > 0)
			{
				if (bFix)
					TableRenderer.f_AddRow("License files updated", "{}"_f << Totals.m_nLicenseFilesIssues);
				else
					TableRenderer.f_AddRow
						(
							"{}License files needing update{}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default()
							, "{}{}{}"_f << AnsiEncoding.f_StatusWarning() << Totals.m_nLicenseFilesIssues << AnsiEncoding.f_Default()
						)
					;
			}

			if (Totals.m_nReuseTomlIssues > 0)
			{
				if (bFix)
					TableRenderer.f_AddRow("REUSE.toml files updated", "{}"_f << Totals.m_nReuseTomlIssues);
				else
					TableRenderer.f_AddRow
						(
							"{}REUSE.toml files needing update{}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default()
							, "{}{}{}"_f << AnsiEncoding.f_StatusWarning() << Totals.m_nReuseTomlIssues << AnsiEncoding.f_Default()
						)
					;
			}

			if (Totals.m_nDisallowedFiles > 0)
			{
				if (bFix)
					TableRenderer.f_AddRow("Disallowed files deleted", "{}"_f << Totals.m_nDisallowedFiles);
				else
					TableRenderer.f_AddRow
						(
							"{}Disallowed files{}"_f << AnsiEncoding.f_StatusWarning() << AnsiEncoding.f_Default()
							, "{}{}{}"_f << AnsiEncoding.f_StatusWarning() << Totals.m_nDisallowedFiles << AnsiEncoding.f_Default()
						)
					;
			}

			if (Totals.m_nErrors > 0)
			{
				TableRenderer.f_AddRow
					(
						"{}Errors{}"_f << AnsiEncoding.f_StatusError() << AnsiEncoding.f_Default()
						, "{}{}{}"_f << AnsiEncoding.f_StatusError() << Totals.m_nErrors << AnsiEncoding.f_Default()
					)
				;
			}

			TableRenderer.f_Output(CTableRenderHelper::EOutputType_HumanReadable);
		}

		bool bHasIssues
			= Totals.m_nFilesNeedFix > 0
			|| Totals.m_nErrors > 0
			|| Totals.m_nLicenseFilesIssues > 0
			|| Totals.m_nReuseTomlIssues > 0
			|| Totals.m_nDisallowedFiles > 0
			|| Totals.m_nFilesNoCommentStyle > 0
		;

		// When --fix is active, return success even if non-fixable issues remain (e.g.
		// missing comment styles or missing header config).  The warnings are already
		// printed above so the user can address them manually; failing here would make
		// --fix unusable in workflows that have known unfixable files.
		if (!bFix && bHasIssues)
		{
			*_pCommandLine += "\nRun with {}--fix{} to automatically fix these issues.\n"_f << AnsiEncoding.f_StatusNormal() << AnsiEncoding.f_Default();

			co_return DMibErrorInstance("License check failed");
		}

		co_return ERetry_None;
	}
}
