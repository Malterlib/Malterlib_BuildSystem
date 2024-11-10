// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	auto CGeneratorInstance::f_GenerateProjectFile_Dependency(CProject &_Project, CProjectState &_ProjectState) const -> TCUnsafeFuture<void>
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		for (auto &Dependency : _Project.m_Dependencies)
		{
			if (Dependency.m_EnabledConfigs.f_IsEmpty())
				continue;

			auto &GeneratorSettings = Dependency.m_GeneratorSettings.f_ConstructSettings();
			for (auto &Config : Dependency.m_EnabledConfigs)
				GeneratorSettings[Dependency.m_EnabledConfigs.fs_GetKey(Config)];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_ProjectEntity) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(_ProjectEntity);
					mint nDepenencies = 0;
					for (auto &Dependency : _Project.m_Dependencies)
					{
						if (Dependency.m_EnabledConfigs.f_IsEmpty())
							continue;

						auto &GeneratorSettings = Dependency.m_GeneratorSettings.f_Settings();

						auto *pResult = GeneratorSettings.f_FindEqual(Config);
						if (!pResult)
							continue;

						++nDepenencies;

						auto &Entity = **Dependency.m_EnabledConfigs.f_FindEqual(Config);

						CGeneratorSettings::fs_PopulateSetting
							(
								gc_ConstKey_GeneratorSetting_Dependency
								, EPropertyType_Dependency
								, m_BuildSystem
								, Entity
								, *pResult
							 )
						;

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
			if (Dependency.m_EnabledConfigs.f_IsEmpty())
				continue;

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

			auto pDependProject = _Project.m_pSolution->m_Projects.f_FindEqual(ProjectDependency);

			if (!pDependProject)
				m_BuildSystem.fs_ThrowError(Dependency.m_Position, CStr::CFormat("Dependency {} not found in workspace") << ProjectDependency);

			for (auto &Entity : pDependProject->m_EnabledProjectConfigs)
			{
				auto &Config = pDependProject->m_EnabledProjectConfigs.fs_GetKey(Entity);
				if (!_Project.m_EnabledProjectConfigs.f_FindEqual(Config))
					continue;

				if (!Dependency.m_EnabledConfigs.f_FindEqual(Config))
				{
					bInvalid = true;
					break;
				}
			}

			if (bInvalid)
			{
				CStr ConfigsDependency;
				for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
					fg_AddStrSep(ConfigsDependency, iConfig.f_GetKey().f_GetFullName(), "\n   ");

				CStr ConfigsDependencyDebug;
				for (auto &Debug : Dependency.m_PerConfigDebug.f_Entries())
				{
					fg_AddStrSep
						(
							ConfigsDependencyDebug
							, "{}   Indirect {}   IndirectOrdered {}"_f 
							<< Debug.f_Key().f_GetFullName() 
							<< Debug.f_Value().m_bIndirect 
							<< Debug.f_Value().m_bIndirectOrdered
							, "\n   "
						)
					;
				}

				CStr ConfigsProject;
				for (auto iConfig = pDependProject->m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					if (!_Project.m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
						continue;

					fg_AddStrSep(ConfigsProject, iConfig.f_GetKey().f_GetFullName(), "\n   ");
				}

				m_BuildSystem.fs_ThrowError
					(
						Dependency.m_Position
						, fg_Format
						(
							"Dependencies cannot be varied per configuration ({}:{} depends on {}):\nDependency:\n   {}\nProject:\n   {}\nDependency Debug:\n   {}\n"
							, _Project.m_pSolution->f_GetName()
							, _Project.f_GetName()
							, Dependency.f_GetName()
							, ConfigsDependency
							, ConfigsProject
							, ConfigsDependencyDebug
						)
					)
				;
			}

			for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
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
