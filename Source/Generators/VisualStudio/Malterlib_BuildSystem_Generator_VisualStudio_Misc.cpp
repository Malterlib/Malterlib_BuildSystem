// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem::NVisualStudio
{
	constexpr CUniversallyUniqueIdentifier const gc_GeneratorGroupUUIDNamespace(0x2A2485EA, 0xA49B, 0x4829, 0xB93D, constant_uint64(0xFD225BE2D065));
	constexpr CUniversallyUniqueIdentifier const gc_GeneratorProjectUUIDNamespace(0x0A5F7138, 0xCEB6, 0x422C, 0xA248, constant_uint64(0x1966A6EB17A1));
	constexpr CUniversallyUniqueIdentifier const gc_GeneratorSolutionUUIDNamespace(0x03AB27C0, 0xC158, 0x4865, 0x8F83, constant_uint64(0x4ACE0C40DE9B));
	constexpr CUniversallyUniqueIdentifier const gc_GeneratorPrefixHeaderUUIDNamespace(0x03AB27C0, 0xC158, 0x4865, 0x8F83, constant_uint64(0x4ACE0C40DE9B));

	CStr const &CGroup::f_GetPath() const
	{
		return TCMap<CStr, CGroup>::fs_GetKey(*this);
	}

	CStr CGroup::f_GetGroupPath() const
	{
		if (m_pParent)
			return m_pParent->f_GetPath();
		return CStr();
	}

	CStr const &CGroup::f_GetGUID()
	{
		if (!mp_GUID.f_IsEmpty())
			return mp_GUID;

		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, gc_GeneratorGroupUUIDNamespace, f_GetPath()).f_GetAsString();
		return mp_GUID;
	}

	CStr const &CProjectFile::f_GetName() const
	{
		return TCMap<CStr, CProjectFile>::fs_GetKey(*this);
	}
	CStr CProjectFile::f_GetGroupPath() const
	{
		if (m_pGroup)
			return m_pGroup->f_GetPath();
		return CStr();
	}

	CStr const &CProjectDependency::f_GetName() const
	{
		return TCMap<CStr, CProjectDependency>::fs_GetKey(*this);
	}

	CStr const &CProject::f_GetName() const
	{
		return TCMap<CStr, CProject>::fs_GetKey(*this);
	}

	CStr CProject::f_GetPath() const
	{
		if (m_pGroup)
			return m_pGroup->f_GetPath() / f_GetName();
		return f_GetName();
	}

	CStr const &CProject::f_GetSolutionTypeGUID() const
	{
		if (m_LanguageType == ELanguageType_Native)
			return gc_ConstString__8BC9CEB8_8B4A_11D0_8D11_00A0C91BC942_;
		else if (m_LanguageType == ELanguageType_CSharp)
			return gc_ConstString__FAE04EC0_301F_11D3_BF4B_00C04F79EFBC_;
		return gc_ConstString_Error;
	}

	CStr const &CProject::f_GetGUID()
	{
		if (!mp_GUID.f_IsEmpty())
			return mp_GUID;

		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, gc_GeneratorProjectUUIDNamespace, f_GetName()).f_GetAsString();
		return mp_GUID;
	}

	CProject::CProject(CSolution *_pSolution)
		: m_pSolution(_pSolution)
		, m_LanguageType(ELanguageType_Native)
	{
	}

	void CProject::fr_FindRecursiveDependencies(CBuildSystem const &_BuildSystem, TCSet<CStr> &_Stack, CProjectDependency const *_pDepend, TCMap<CStr, CProject> const &_Projects) const
	{
		auto Mapped = _Stack(f_GetName());
		auto Cleanup
			= fg_OnScopeExit
			(
				[&]
				{
					_Stack.f_Remove(f_GetName());
				}
			)
		;
		if (!Mapped.f_WasCreated())
		{
			CStr Projects;
			for (auto iStack = _Stack.f_GetIterator(); iStack; ++iStack)
				fg_AddStrSep(Projects, *iStack, ", ");
			_BuildSystem.fs_ThrowError(_pDepend->m_Position, CStr::CFormat("Project dependency loop ({})") << Projects);
		}

		for (auto iDepend = m_Dependencies.f_GetIterator(); iDepend; ++iDepend)
		{
			if (iDepend->m_bExternal)
				continue;

			auto pProject = _Projects.f_FindEqual(iDepend.f_GetKey());
			if (!pProject)
				_BuildSystem.fs_ThrowError(iDepend->m_Position, CStr::CFormat("Dependency {} not found in workspace") << iDepend.f_GetKey());
			pProject->fr_FindRecursiveDependencies(_BuildSystem, _Stack, iDepend, _Projects);
		}
	}

	CStr const &CSolutionFile::f_GetName() const
	{
		return TCMap<CStr, CSolutionFile>::fs_GetKey(*this);
	}
	CStr CSolutionFile::f_GetGroupPath() const
	{
		if (m_pGroup)
			return m_pGroup->f_GetPath();
		return CStr();
	}

	CStr const &CSolution::f_GetName() const
	{
		return m_Name;
	}

	void CSolution::f_FindRecursiveDependencies(CBuildSystem const &_BuildSystem)
	{
		for (auto iProject = m_Projects.f_GetIterator(); iProject; ++iProject)
		{
			TCSet<CStr> Stack;
			iProject->fr_FindRecursiveDependencies(_BuildSystem, Stack, nullptr, m_Projects);
		}
	}
}
