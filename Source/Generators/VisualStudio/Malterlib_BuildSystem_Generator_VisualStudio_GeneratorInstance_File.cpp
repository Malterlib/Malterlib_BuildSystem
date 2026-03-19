// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	auto CGeneratorInstance::f_GenerateProjectFile_File(CProject &_Project, CProjectState &_ProjectState) const -> TCUnsafeFuture<TCMap<CStr, CCompileType>>
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		for (auto &File : _Project.m_Files)
		{
			if (File.m_bWasGenerated)
				continue;

			auto &GeneratorSettings = File.m_GeneratorSettings.f_ConstructSettings();

			for (auto &Config : File.m_EnabledConfigs)
				GeneratorSettings[File.m_EnabledConfigs.fs_GetKey(Config)];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_ProjectEntity) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(_ProjectEntity);
					umint nFiles = 0;
					for (auto &File : _Project.m_Files)
					{
						if (File.m_bWasGenerated)
							continue;

						auto &GeneratorSettings = File.m_GeneratorSettings.f_Settings();

						auto *pResult = GeneratorSettings.f_FindEqual(Config);
						if (!pResult)
							continue;

						++nFiles;

						auto &Entity = **File.m_EnabledConfigs.f_FindEqual(Config);

						CGeneratorSettings::fs_PopulateSetting(gc_ConstKey_GeneratorSetting_Compile, EPropertyType_Compile, m_BuildSystem, Entity, *pResult);

						if ((nFiles % 100) == 0)
							co_await g_Yield;
					}

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;
		co_await m_BuildSystem.f_CheckCancelled();

		bool bAbsolutePaths = (m_BuildSystem.f_GetGenerateSettings().m_GenerationFlags & EGenerationFlag_AbsoluteFilePaths) != 0;
		TCMap<CStr, CCompileType> CompileTypes;

		for (auto &File : _Project.m_Files)
		{
			if (File.m_bWasGenerated)
				continue;

			File.m_bWasGenerated = true;

			if (File.m_EnabledConfigs.f_IsEmpty())
				continue;

			auto Type = File.m_GeneratorSettings.f_GetSingleSetting<CStr>("Type");
			auto VSType = File.m_GeneratorSettings.f_GetSingleSetting<CStr>("VSType");
			auto &CompileType = CompileTypes[Type.m_Value];
			CompileType.m_VSType = VSType.m_Value;
			File.m_VSType = VSType.m_Value;
			File.m_VSFile = (*File.m_EnabledConfigs.f_FindSmallest())->f_GetKeyName();

			if (bAbsolutePaths)
				File.m_VSFile = File.m_VSFile.f_ReplaceChar('/', '\\');
			else
			{
				CStr RelativeName = CFile::fs_MakePathRelative(File.m_VSFile, _ProjectState.m_CurrentOutputDir);
				CStr PotentialName = CFile::fs_AppendPath(_ProjectState.m_CurrentOutputDir, RelativeName);
				// Use absolute paths if it is going to save us from hitting _MAX_PATH or if absolute path is shorter than relative path
				if ((PotentialName.f_GetLen() >= 256 && File.m_VSFile.f_GetLen() < PotentialName.f_GetLen()) || File.m_VSFile.f_GetLen() < RelativeName.f_GetLen())
					File.m_VSFile = File.m_VSFile.f_ReplaceChar('/', '\\');
				else
					File.m_VSFile = RelativeName.f_ReplaceChar('/', '\\');
			}

			auto &GeneratorSettings = File.m_GeneratorSettings.f_Settings();

			for (auto &Setting : GeneratorSettings)
			{
				auto &Config = GeneratorSettings.fs_GetKey(Setting);
				auto bDisabled = Setting.f_GetSettingWithoutPositions<bool>("Disabled");

				if (!bDisabled)
					CompileType.m_EnabledConfigs[Config];

				if (_Project.m_LanguageType != ELanguageType_Native)
					continue;

				auto bPrecompilePrefixHeader = Setting.f_GetSettingWithoutPositions<bool>("PrecompilePrefixHeader");

				if (bPrecompilePrefixHeader)
				{
					auto PrefixHeaderValue = Setting.f_GetSetting<TCOptional<CStr>>("PrefixHeader");
					if (PrefixHeaderValue.m_Value && !PrefixHeaderValue.m_Value.f_Get().f_IsEmpty())
					{
						auto Path = PrefixHeaderValue.m_Value.f_Get();
						auto &PrefixHeader = _ProjectState.m_PrefixHeaders[Type.m_Value][Path];
						PrefixHeader.m_bUsed = true;
						PrefixHeader.m_Configurations[Config];
						PrefixHeader.m_Positions.f_AddPositions(PrefixHeaderValue.m_Positions);

						File.m_PrefixHeaders(Config, &PrefixHeader);
					}
				}
			}
		}

		co_return fg_Move(CompileTypes);
	}

	void CGeneratorInstance::f_GenerateProjectFile_AddToXML_File
		(
			CProject &_Project
			, CProjectState &_ProjectState
			, CProjectXMLState &_XMLState
			, TCMap<CStr, CGeneratorSettingsVSType> &_Compile
		) const
	{
		for (auto &File : _Project.m_Files)
		{
			if (File.m_EnabledConfigs.f_IsEmpty())
				continue;

			File.m_ParsedSettings = File.m_GeneratorSettings.f_GetParsedVSSettings<true, true>(&_Compile);
		}

		for (auto &File : _Project.m_Files)
		{
			if (File.m_EnabledConfigs.f_IsEmpty())
				continue;

			CItemState ItemState;
			ItemState.m_pItemElement = CXMLDocument::f_CreateElement(_XMLState.m_pFileItemGroup, File.m_VSType);
			ItemState.m_pCompile = &_Compile;

			CXMLDocument::f_SetAttribute(ItemState.m_pItemElement, gc_ConstString_Include, File.m_VSFile);
			if (_XMLState.m_bIsDotNet)
			{
				CStr GroupPath = File.f_GetGroupPath().f_ReplaceChar('/', '\\');
				if (!GroupPath.f_IsEmpty())
					CXMLDocument::f_AddElementAndText(ItemState.m_pItemElement, gc_ConstKey_Dependency_Link.m_Name, GroupPath + "\\" + CFile::fs_GetFile(File.m_VSFile));
			}

			for (auto &pPrefixHeader : File.m_PrefixHeaders)
			{
				auto &Configuration = File.m_PrefixHeaders.fs_GetKey(pPrefixHeader);
				auto *pSetting = File.m_ParsedSettings.f_Settings().f_FindEqual(Configuration);
				if (!pSetting)
					continue;

				auto &Property = pSetting->m_Properties[gc_ConstKey_Compile_XInternalPrecompiledHeaderOutputFile.m_Name];
				Property.m_VSSettings = {CGeneratorSettings::CVS_Setting_Item{CStr(gc_ConstString_PrecompiledHeaderOutputFile), pPrefixHeader->m_PCHFile}};
			}

			CGeneratorSettings::fs_AddToXMLFiles<true, true>(_XMLState, _Project, fg_Move(File.m_ParsedSettings), &ItemState);
		}
	}
}
