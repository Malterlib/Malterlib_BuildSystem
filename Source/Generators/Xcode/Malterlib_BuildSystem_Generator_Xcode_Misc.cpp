// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem::NXcode
{
	CUniversallyUniqueIdentifier g_GeneratorUUIDNamespace("{2A2485EA-A49B-4829-B93D-FD225BE2D065}");

	CStr g_ReservedProductRefGroup("Product Reference");
	CStr g_ReservedConfigurationsGroup("Configurations");
	CStr g_ReservedProjectDependenciesGroup("Target Dependencies");
	CStr g_ReservedGeneratorGroup("~Automatic");

	CStr const& CNativeTarget::f_GetGUID()
	{
		if (!mp_GUID.f_IsEmpty())
			return mp_GUID;

		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "PBXNativeTarget").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_GUID;
	}

	CStr const& CNativeTarget::f_GetSourcesBuildPhaseGUID()
	{
		if (!mp_SourcesBuildPhaseGUID.f_IsEmpty())
			return mp_SourcesBuildPhaseGUID;

		mp_SourcesBuildPhaseGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "PBXSourcesBuildPhase").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_SourcesBuildPhaseGUID;
	}

	CStr const& CNativeTarget::f_GetFrameworksBuildPhaseGUID()
	{
		if (!mp_FrameworksBuildPhaseGUID.f_IsEmpty())
			return mp_FrameworksBuildPhaseGUID;

		mp_FrameworksBuildPhaseGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "PBXFrameworksBuildPhase").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_FrameworksBuildPhaseGUID;
	}

#if 0
	CStr const& CNativeTarget::f_GetHeadersBuildPhaseGUID()
	{
		if (!mp_HeadersBuildPhaseGUID.f_IsEmpty())
			return mp_HeadersBuildPhaseGUID;

		mp_HeadersBuildPhaseGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "PBXHeadersBuildPhase").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_HeadersBuildPhaseGUID;
	}
#endif

	CStr const& CNativeTarget::f_GetProductReferenceGUID()
	{
		if (!mp_ProductReferenceGUID.f_IsEmpty())
			return mp_ProductReferenceGUID;

		mp_ProductReferenceGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "productReference").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_ProductReferenceGUID;
	}

	CStr const& CProjectDependency::f_GetFileRefGUID()
	{
		if (!mp_DependencyFileRefGUID.f_IsEmpty())
			return mp_DependencyFileRefGUID;

		mp_DependencyFileRefGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "dependencyName").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyFileRefGUID;
	}

	CStr const& CProjectDependency::f_GetBuildRefGUID()
	{
		if (!mp_DependencyBuildRefGUID.f_IsEmpty())
			return mp_DependencyBuildRefGUID;

		mp_DependencyBuildRefGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "dependencyBuildName").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyBuildRefGUID;
	}

	CStr const& CProjectDependency::f_GetContainerItemGUID()
	{
		if (!mp_DependencyContainerItemGUID.f_IsEmpty())
			return mp_DependencyContainerItemGUID;

		mp_DependencyContainerItemGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "PBXContainerItemProxy").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyContainerItemGUID;
	}

	CStr const& CProjectDependency::f_GetContainerItemProductGUID()
	{
		if (!mp_DependencyItemProductGUID.f_IsEmpty())
			return mp_DependencyItemProductGUID;

		mp_DependencyItemProductGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "PBXContainerItemProductProxy").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyItemProductGUID;
	}

	CStr const& CProjectDependency::f_GetReferenceProxyGUID()
	{
		if (!mp_DependencyReferenceProxyGUID.f_IsEmpty())
			return mp_DependencyReferenceProxyGUID;

		mp_DependencyReferenceProxyGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "PBXReferenceProxy").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyReferenceProxyGUID;
	}

	CStr const& CProjectDependency::f_GetProductRefGroupGUID()
	{
		if (!mp_DependencyProductRefGroupGUID.f_IsEmpty())
			return mp_DependencyProductRefGroupGUID;

		mp_DependencyProductRefGroupGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "productReference").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyProductRefGroupGUID;
	}
	
	CStr const& CProjectDependency::f_GetTargetGUID()
	{
		if (!mp_DependencyTargetGUID.f_IsEmpty())
			return mp_DependencyTargetGUID;

		mp_DependencyTargetGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "PBXTargetDependency").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_DependencyTargetGUID;
	}

	CStr const& CNativeTarget::f_GetBuildConfigurationListGUID()
	{
		if (!mp_BuildConfigurationListGUID.f_IsEmpty())
			return mp_BuildConfigurationListGUID;

		mp_BuildConfigurationListGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "PBXNativeTargetXCBuildConfigurationList").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_BuildConfigurationListGUID;
	}

	CStr CBuildConfiguration::f_GetFile() const
	{
		return CFile::fs_GetFile(m_Path);
	}
	
	CStr CBuildConfiguration::f_GetFileNoExt() const
	{
		return CFile::fs_GetFileNoExt(m_Path);
	}
	
	CStr const& CBuildConfiguration::f_GetGUID()
	{
		if (!mp_GUID.f_IsEmpty())
			return mp_GUID;

		CStr Project = m_bProject ? "PBXProject" : "PBXNativeTarget";
		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Path + Project + "XCConfiguration").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_GUID;
	}

	CStr const& CBuildConfiguration::f_GetFileRefGUID()
	{
		if (!mp_FileRefGUID.f_IsEmpty())
			return mp_FileRefGUID;

		mp_FileRefGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Path).f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_FileRefGUID;
	}

	CStr const& CBuildScript::f_GetGUID()
	{
		if (!mp_GUID.f_IsEmpty())
			return mp_GUID;

		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, m_Name + "PBXShellScriptBuildPhase").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_GUID;
	}

	CStr const& CBuildScript::f_GetScriptSetting()
	{
		if (!mp_BuildSetting.f_IsEmpty())
			return mp_BuildSetting;
		mp_BuildSetting = CStr::CFormat("\\\"${0}\\\" ; export ErrorReturn=$? ; if [ $ErrorReturn != 0 ] ; then echo Script \\\"${0}\\\" failed with exit code $ErrorReturn ; exit 1 ; fi\\n") << m_Name;
		return mp_BuildSetting;
	}

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

		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetPath()).f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
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
	CStr const& CProjectFile::f_GetFileRefGUID()
	{
		if (!mp_FileRefGUID.f_IsEmpty())
			return mp_FileRefGUID;

		mp_FileRefGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName()).f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_FileRefGUID;
	}
	CStr const& CProjectFile::f_GetBuildRefGUID()
	{
		if (!mp_BuildRefGUID.f_IsEmpty())
			return mp_BuildRefGUID;

		mp_BuildRefGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "BuildRef").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_BuildRefGUID;
	}
	CStr const& CProjectFile::f_GetLastKnownFileType()
	{
		if (!m_LastKnownFileType.f_IsEmpty())
			return m_LastKnownFileType;
			
		m_LastKnownFileType = "text"; // default
		return m_LastKnownFileType;
	}
	CStr const& CProjectFile::f_GetCompileFlagsGUID()
	{
		if (!mp_CompileFlagsGUID.f_IsEmpty())
			return mp_CompileFlagsGUID;

		mp_CompileFlagsGUID = (CStr::CFormat("XFile{}") << f_GetBuildRefGUID()).f_GetStr();
		return mp_CompileFlagsGUID;
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
			return m_pGroup->f_GetPath() + "/" + f_GetName();
		return f_GetName();
	}

	CStr const &CProject::f_GetGUID()
	{
		if (!mp_GUID.f_IsEmpty())
			return mp_GUID;

		mp_GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "ProjectGUID").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_GUID;
	}

	CStr const& CProject::f_GetBuildConfigurationListGUID()
	{
		if (!mp_BuildConfigurationListGUID.f_IsEmpty())
			return mp_BuildConfigurationListGUID;

		mp_BuildConfigurationListGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "PBXProjectXCBuildConfigurationList").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_BuildConfigurationListGUID;
	}

	CStr const &CProject::f_GetMainGroupGUID()
	{
		if (!mp_MainGroupGUID.f_IsEmpty())
			return mp_MainGroupGUID;

		mp_MainGroupGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, f_GetName() + "MainGroupGUID").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_MainGroupGUID;
	}

	CStr const &CProject::f_GetGeneratorGroupGUID()
	{
		if (!mp_GeneratorGroupGUID.f_IsEmpty())
			return mp_GeneratorGroupGUID;

		mp_GeneratorGroupGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, "~Automatic").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_GeneratorGroupGUID;
	}

	CStr const &CProject::f_GetProductRefGroupGUID()
	{
		if (!mp_ProductRefGroupGUID.f_IsEmpty())
			return mp_ProductRefGroupGUID;

		mp_ProductRefGroupGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, "~Automatic/Product Reference").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_ProductRefGroupGUID;
	}

	CStr const &CProject::f_GetProjectDependenciesGroupGUID()
	{
		if (!mp_ProjectDependenciesGroupGUID.f_IsEmpty())
			return mp_ProjectDependenciesGroupGUID;

		mp_ProjectDependenciesGroupGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, "~Automatic/Target Dependencies").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_ProjectDependenciesGroupGUID;
	}

	CStr const &CProject::f_GetConfigurationsGroupGUID()
	{
		if (!mp_ConfigurationsGroupGUID.f_IsEmpty())
			return mp_ConfigurationsGroupGUID;

		mp_ConfigurationsGroupGUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorUUIDNamespace, "~Automatic/Configurations").f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum);
		return mp_ConfigurationsGroupGUID;
	}

	CProject::CProject(CSolution *_pSolution)
		: m_pSolution(_pSolution)
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

		for (auto iDepend = m_DependenciesMap.f_GetIterator(); iDepend; ++iDepend)
		{
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
		return TCMap<CStr, CSolution>::fs_GetKey(*this);
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
