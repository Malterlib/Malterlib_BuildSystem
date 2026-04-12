// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem::NRepository
{
	template <typename tf_FOutput>
	void fg_OutputRepositoryInfo
		(
			EOutputType _OutputType
			, CStr const &_Info
			, EAnsiEncodingFlag _AnsiFlags
			, CStr const &_RepoName
			, umint _MaxRepoWidth
			, tf_FOutput const &_fOutput
		)
	{
		CColors Colors(_AnsiFlags);
		CStr RepoColor = Colors.f_StatusNormal();
		switch (_OutputType)
		{
		case EOutputType_Normal: RepoColor = Colors.f_StatusNormal(); break;
		case EOutputType_Warning: RepoColor = Colors.f_StatusWarning(); break;
		case EOutputType_Error: RepoColor = Colors.f_StatusError(); break;
		}

		CStr RepoName = "{sj*,a-}"_f << _RepoName << _MaxRepoWidth;

		CStr ReplacedRepo = RepoName.f_Replace("/", CStr("{}{}/{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor));
		CStr Indent = "{sf ,sj*}   "_f << "" << _MaxRepoWidth;
		_fOutput
			(
				"{}{}{}   {}\n"_f
				<< RepoColor
				<< ReplacedRepo
				<< Colors.f_Default()
				<< _Info.f_Indent(Indent, false)
			)
		;
	}
}
