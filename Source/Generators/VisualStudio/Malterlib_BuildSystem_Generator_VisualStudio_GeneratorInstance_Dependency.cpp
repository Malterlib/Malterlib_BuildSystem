// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	auto CGeneratorInstance::f_GenerateProjectFile_Dependency(CProject &_Project, CProjectState &_ProjectState) const -> TCFuture<void>
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);

		for (auto &Dependency : _Project.m_Dependencies)
		{
			auto &GeneratorSettings = Dependency.m_GeneratorSettings.f_ConstructSettings();
			for (auto &Config : Dependency.m_EnabledConfigs)
				GeneratorSettings[Dependency.m_EnabledConfigs.fs_GetKey(Config)];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_ProjectEntity) -> TCFuture<void>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(_ProjectEntity);
					mint nDepenencies = 0;
					for (auto &Dependency : _Project.m_Dependencies)
					{
						auto &GeneratorSettings = Dependency.m_GeneratorSettings.f_Settings();

						auto *pResult = GeneratorSettings.f_FindEqual(Config);
						if (!pResult)
							continue;

						++nDepenencies;

						Dependency.m_GeneratorSettings.f_PopulateSetting(gc_ConstKey_GeneratorSetting_Dependency, EPropertyType_Dependency, m_BuildSystem, Dependency.m_EnabledConfigs, *pResult);

						if ((nDepenencies % 100) == 0)
							co_await g_Yield;
					}

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;
		co_await m_BuildSystem.f_CheckCancelled();

		co_return {};
	}

	void CGeneratorInstance::f_GenerateProjectFile_AddToXML_Dependency(CProject &_Project, CProjectState &_ProjectState, CProjectXMLState &_XMLState) const
	{
		_XMLState.m_pDependenciesItemGroup = CXMLDocument::f_CreateElement(_XMLState.m_pProject, "ItemGroup");

		for (auto &Dependency : _Project.m_Dependencies)
		{
			auto ParsedSettings = Dependency.m_GeneratorSettings.f_GetParsedVSSettings<true, true>(nullptr);

			if (Dependency.m_bExternal)
			{
				if (ParsedSettings.m_VSType.f_IsEmpty())
					continue;

				CItemState ItemState;
				ItemState.m_pItemElement = CXMLDocument::f_CreateElement(_XMLState.m_pDependenciesItemGroup, ParsedSettings.m_VSType);

				CGeneratorSettings::fs_AddToXMLFiles<false, true>(_XMLState, _Project, fg_Move(ParsedSettings), &ItemState);

				continue;
			}

			auto const &ProjectDependency = _Project.m_Dependencies.fs_GetKey(Dependency);
			bool bInvalid = false;

			for (auto &Entity : Dependency.m_EnabledConfigs)
			{
				auto &Config = Dependency.m_EnabledConfigs.fs_GetKey(Entity);
				if (!_Project.m_EnabledProjectConfigs.f_FindEqual(Config))
				{
					bInvalid = true;
					break;
				}
			}

			for (auto &Entity : _Project.m_EnabledProjectConfigs)
			{
				auto &Config = Dependency.m_EnabledConfigs.fs_GetKey(Entity);
				if (!Dependency.m_EnabledConfigs.f_FindEqual(Config))
				{
					bInvalid = true;
					break;
				}
			}

			if (bInvalid)
			{
				CStr Configs0;
				for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
					fg_AddStrSep(Configs0, iConfig.f_GetKey().f_GetFullName(), "\n");

				CStr Configs1;
				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
					fg_AddStrSep(Configs1, iConfig.f_GetKey().f_GetFullName(), "\n");

				m_BuildSystem.fs_ThrowError
					(
						Dependency.m_Position
						, fg_Format("Dependencies cannot be varied per configuration ({}):\n{}\n!=\n{}\n", _Project.f_GetName(), Configs0, Configs1)
					)
				;
			}

			auto pDependProject = _Project.m_pSolution->m_Projects.f_FindEqual(ProjectDependency);

			if (!pDependProject)
				m_BuildSystem.fs_ThrowError(Dependency.m_Position, CStr::CFormat("Dependency {} not found in workspace") << ProjectDependency);

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				if (!pDependProject->m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
				{
					m_BuildSystem.fs_ThrowError
						(
							Dependency.m_Position
							, CStr::CFormat("Dependency project does not have required configuration {} - {}")
							<< _Project.m_Platforms[iConfig.f_GetKey()]
							<< iConfig.f_GetKey().m_Configuration
						)
					;
				}
			}

			auto pReference = CXMLDocument::f_CreateElement(_XMLState.m_pDependenciesItemGroup, "ProjectReference");
			CStr DependencyProjectFile = CFile::fs_MakeNiceFilename(pDependProject->f_GetName()) + ".vcxproj";
			CXMLDocument::f_SetAttribute(pReference, gc_ConstString_Include, DependencyProjectFile);
			CXMLDocument::f_AddElementAndText(pReference, "Project", pDependProject->f_GetGUID());
			CXMLDocument::f_AddElementAndText(pReference, gc_ConstString_Name, pDependProject->f_GetName());

			CItemState ItemState;
			ItemState.m_pItemElement = pReference;

			CGeneratorSettings::fs_AddToXMLFiles<false, true>(_XMLState, _Project, fg_Move(ParsedSettings), &ItemState);
		}
	}	
}
