// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	void CGeneratorInstance::f_GenerateSolutionFile(CSolution &_Solution, CStr const &_OutputDir, mint _MaxSolutionNameLength) const
	{
		CTimer Timer;
		Timer.f_Start();
		CStr OutputDir = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Solution.f_GetName())));
		fg_ParallellForEach
			(
				_Solution.m_Projects
				, [&](CProject &_Project)
				{
					CStr OutputDir = CFile::fs_AppendPath(CFile::fs_AppendPath(_OutputDir, "Files"), CStr(CFile::fs_MakeNiceFilename(_Solution.f_GetName())));
					f_GenerateProjectFile(_Project, OutputDir);
				}
			)
		;

		CStr OutputFile = OutputDir + ".sln";

		CStr FileData;

		FileData += "\r\n";
		if (m_Version == 2012)
		{
			FileData += "Microsoft Visual Studio Solution File, Format Version 12.00\r\n";
			FileData += CStr::CFormat("# Visual Studio {}\r\n") << m_Version;
		}
		else if (m_Version == 2013)
		{
			FileData += "Microsoft Visual Studio Solution File, Format Version 12.00\r\n";
			FileData += CStr::CFormat("# Visual Studio {}\r\n") << m_Version;
			FileData += "VisualStudioVersion = 12.0.20617.1 PREVIEW\r\n";
			FileData += "MinimumVisualStudioVersion = 10.0.40219.1\r\n";

		}
		else if (m_Version == 2015)
		{
			FileData += "Microsoft Visual Studio Solution File, Format Version 12.00\r\n";
			FileData += "# Visual Studio 14\r\n";
			FileData += "VisualStudioVersion = 14.0.22823.1\r\n";
			FileData += "MinimumVisualStudioVersion = 10.0.40219.1\r\n";
		}
		else if (m_Version == 2017)
		{
			FileData += "Microsoft Visual Studio Solution File, Format Version 12.00\r\n";
			FileData += "# Visual Studio 15\r\n";
			FileData += "VisualStudioVersion = 15.0.26206.0\r\n";
			FileData += "MinimumVisualStudioVersion = 10.0.40219.1\r\n";
		}
		else
			DError("Implement this");

		for (auto iProject = _Solution.m_Projects.f_GetIterator(); iProject; ++iProject)
		{
			CStr ProjectPath = CFile::fs_MakePathRelative(iProject->m_FileName, _OutputDir).f_ReplaceChar('/', '\\');
			
			FileData 
				+= CStr::CFormat("Project(\"{}\") = \"{}\", \"{}\", \"{}\"\r\n")
				<< iProject->f_GetSolutionTypeGUID()
				<< iProject->f_GetName()
				<< ProjectPath
				<< iProject->f_GetGUID()
			;
			if (m_bEnableSourceControl)
			{
				FileData += "	GlobalSection(PerforceSourceControlProviderSolutionProperties) = preSolution\r\n";
				FileData += "		SolutionIsControlled = True\r\n";
				FileData += "	EndGlobalSection\r\n";
			}

			FileData += "EndProject\r\n";
		}
		TCMap<CGroup *, TCLinkedList<CSolutionFile *>> GroupToFile;
		for (auto iFile = _Solution.m_SolutionFiles.f_GetIterator(); iFile; ++iFile)
		{
			GroupToFile[iFile->m_pGroup].f_Insert(iFile);
		}
		CStr SolutionDir = CFile::fs_GetPath(OutputFile); 

		for (auto iGroup = _Solution.m_Groups.f_GetIterator(); iGroup; ++iGroup)
		{
			FileData += CStr::CFormat("Project(\"{{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"{0}\", \"{0}\", \"{1}\"\r\n") << iGroup->m_Name << iGroup->f_GetGUID();
			if (m_bEnableSourceControl)
			{
				FileData += "	GlobalSection(PerforceSourceControlProviderSolutionProperties) = preSolution\r\n";
				FileData += "		SolutionIsControlled = True\r\n";
				FileData += "	EndGlobalSection\r\n";
			}

			auto pFiles = GroupToFile.f_FindEqual(&(*iGroup));
			if (pFiles)
			{
				FileData += "	ProjectSection(SolutionItems) = preProject\r\n";
				for (auto iFile = pFiles->f_GetIterator(); iFile; ++iFile)
				{
					CStr RelativeDir = CFile::fs_MakePathRelative((*iFile)->f_GetName(), SolutionDir).f_ReplaceChar('/', '\\');
					FileData += CStr::CFormat("		{0} = {0}\r\n") << RelativeDir;
				}
				FileData += "	EndProjectSection\r\n";
			}

			FileData += "EndProject\r\n";
		}

		FileData += "Global\r\n";
		// Source control
		if (m_bEnableSourceControl)
		{
			FileData += "	GlobalSection(PerforceSourceControlProviderSolutionProperties) = preSolution\r\n";
			FileData += "		SolutionIsControlled = True\r\n";
			FileData += "	EndGlobalSection\r\n";
		}

		// Platforms
		{
			FileData += "	GlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n";
			TCVector<CStr> Rows;
			for (auto iConfig = _Solution.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
				Rows.f_Insert(CStr::CFormat("		{0}|{1} = {0}|{1}\r\n") << iConfig->m_Config << iConfig->m_Platform);
			Rows.f_Sort();
			for (auto iRow = Rows.f_GetIterator(); iRow; ++iRow) 
				FileData += *iRow;
			FileData += "	EndGlobalSection\r\n";
		}

		// Configs in platforms
		{
			FileData += "	GlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n";
			TCVector<CStr> Rows;
			for (auto iProject = _Solution.m_Projects.f_GetIterator(); iProject; ++iProject)
			{
				for (auto iConfig = _Solution.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					auto pProjectConfig = iProject->m_EnabledConfigs.f_FindEqual(iConfig.f_GetKey());
					bool bEnabled = !!pProjectConfig;
					
					if (!pProjectConfig)
						pProjectConfig = iProject->m_EnabledConfigs.f_FindSmallest();

					if (!pProjectConfig)
						continue;

					auto &ProjectConfig = iProject->m_EnabledConfigs.fs_GetKey(*pProjectConfig);

					Rows.f_Insert
						(
							CStr::CFormat("		{}.{1}|{2}.ActiveCfg = {3}|{4}\r\n") 
							<< iProject->f_GetGUID()
							<< iConfig->m_Config
							<< iConfig->m_Platform
							<< ProjectConfig.m_Configuration 
							<< iProject->m_Platforms[ProjectConfig]
						)
					;

					if (!bEnabled)
						continue;

					CStr Value = m_BuildSystem.f_EvaluateEntityProperty(**pProjectConfig, EPropertyType_Target, "Disabled");
					if (Value != "true")
					{
						Rows.f_Insert
							(
								CStr::CFormat("		{}.{1}|{2}.Build.0 = {3}|{4}\r\n") 
								<< iProject->f_GetGUID()
								<< iConfig->m_Config
								<< iConfig->m_Platform
								<< ProjectConfig.m_Configuration 
								<< iProject->m_Platforms[ProjectConfig]
							)
						;
					}
				}
			}
			//Rows.f_Sort();
			for (auto iRow = Rows.f_GetIterator(); iRow; ++iRow) 
				FileData += *iRow;
			FileData += "	EndGlobalSection\r\n";
		}

		// Hide
		{
			FileData += "	GlobalSection(SolutionProperties) = preSolution\r\n"
						"		HideSolutionNode = FALSE\r\n"
						"	EndGlobalSection\r\n"
			;

		}

		// Group parenting
		{
			FileData += "	GlobalSection(NestedProjects) = preSolution\r\n";
			for (auto iGroup = _Solution.m_Groups.f_GetIterator(); iGroup; ++iGroup)
			{
				if (iGroup->m_pParent)
					FileData += CStr::CFormat("		{} = {}\r\n") << iGroup->f_GetGUID() << iGroup->m_pParent->f_GetGUID();
			}
			for (auto iProject = _Solution.m_Projects.f_GetIterator(); iProject; ++iProject)
			{
				if (iProject->m_pGroup)
					FileData += CStr::CFormat("		{} = {}\r\n") << iProject->f_GetGUID() << iProject->m_pGroup->f_GetGUID();
			}
			FileData += "	EndGlobalSection\r\n";
		}

		FileData += "EndGlobal\r\n";

		bool bWasCreated;
		if (!m_BuildSystem.f_AddGeneratedFile(OutputFile, FileData, _Solution.f_GetName(), bWasCreated, false))
			DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << OutputFile));

		if (bWasCreated)
		{
			TCVector<uint8> FileDataVector;
			CFile::fs_WriteStringToVector(FileDataVector, CStr(FileData));
			m_BuildSystem.f_WriteFile(FileDataVector, OutputFile);
		}

		Timer.f_Stop();
		
		DConOut("Generated workspace: {sl*,a-} {fe2} s{\n}", _Solution.f_GetName() << _MaxSolutionNameLength << Timer.f_GetTime());
	}
}
