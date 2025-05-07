// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>
#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem::NVisualStudio
{
	void CGeneratorInstance::f_GenerateProjectFile_AddPrefixHeaders(CProject &_Project, CProjectState &_ProjectState) const
	{
		CStr PrefixGenDir = CFile::fs_AppendPath(_ProjectState.m_CurrentOutputDir, _Project.f_GetName());
		_ProjectState.f_CreateDirectory(PrefixGenDir);

		for (auto &Headers : _ProjectState.m_PrefixHeaders)
		{
			CStr CompileType = _ProjectState.m_PrefixHeaders.fs_GetKey(Headers);
			for (auto &PrefixHeader : Headers)
			{
				if (!PrefixHeader.m_bUsed || PrefixHeader.m_Configurations.f_IsEmpty())
					continue;

				auto &FullHeaderPath = Headers.fs_GetKey(PrefixHeader);
				TCMap<CConfiguration, CEntityMutablePointer> const *pEnabledConfigs = nullptr;
				CFilePosition FilePos;
				TCPointer<CGroup> pGroup;
				bool bFile = false;

				{
					auto pFile = _Project.m_Files.f_FindLargestLessThanEqual(FullHeaderPath);
					if (pFile && pFile->f_GetName() == FullHeaderPath)
					{
						pEnabledConfigs = &pFile->m_EnabledConfigs;
						FilePos = pFile->m_Position;
						pGroup = pFile->m_pGroup;
						bFile = true;
					}
					else
					{
						pEnabledConfigs = &_Project.m_EnabledProjectConfigs;
						if (!_ProjectState.f_FileExists(FullHeaderPath))
						{
							CBuildSystemUniquePositions Positions;
							if (!PrefixHeader.m_Positions.m_Positions.f_IsEmpty())
								Positions = PrefixHeader.m_Positions;
							else
								Positions.f_AddPosition(_Project.m_Position, "Project");

							m_BuildSystem.fs_ThrowError(Positions, CStr::CFormat("Prefix header '{}' was not found") << FullHeaderPath);
						}
						else
						{
							if (!PrefixHeader.m_Positions.m_Positions.f_IsEmpty())
								FilePos = PrefixHeader.m_Positions.m_Positions.f_GetFirst().m_Key.m_Position;
							else
								FilePos = _Project.m_Position;
						}

					}
				}

				CStr RelativePath = CFile::fs_MakePathRelative(FullHeaderPath, PrefixGenDir);

				CStr GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, gc_GeneratorPrefixHeaderUUIDNamespace, RelativePath + CompileType)
					.f_GetAsString(EUniversallyUniqueIdentifierFormat_Bare)
				;

				CStr FileName = CFile::fs_AppendPath(PrefixGenDir, CStr::CFormat("PP_{}_{}_{nfh,sj8,sf0}.cpp") << CompileType << _Project.m_EntityName << GUID.f_Hash());

				CStr FileContents;
				FileContents += CStr::CFormat("#include {}\r\n") << RelativePath.f_EscapeStr();

				bool bWasCreated = false;
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, FileContents, _Project.m_pSolution->f_GetName(), bWasCreated))
					m_BuildSystem.fs_ThrowError(FilePos, CStr::CFormat("File '{}' already generated with other contents") << FileName);

				if (bWasCreated)
				{
					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
					m_BuildSystem.f_WriteFile(FileData, FileName);
				}

				auto FileMap = _Project.m_Files(FileName);

				auto &File = FileMap.f_GetResult();

				File.m_Position = FilePos;
				File.m_pGroup = pGroup;

				PrefixHeader.m_PCHFile = "$(IntDir){}_{}.pch"_f << _Project.m_EntityName << GUID;
				CStr PCHFile = "RawMSBuild:{}"_f << PrefixHeader.m_PCHFile;

				DCheck(!PrefixHeader.m_Configurations.f_IsEmpty());

				for (auto iConfig = PrefixHeader.m_Configurations.f_GetIterator(); iConfig; ++iConfig)
				{
					CStr Platform = _Project.m_Platforms[iConfig.f_GetKey()];
					auto pFileConfig = pEnabledConfigs->f_FindEqual(iConfig.f_GetKey());
					if (!pFileConfig)
					{
						m_BuildSystem.fs_ThrowError
							(
								FilePos
								, CStr::CFormat("Prefix header is not enabled for config '{} - {}'")
								<< Platform
								<< iConfig.f_GetKey().m_Configuration
							)
						;
					}
					CEntity *pParent;
					if ((*pFileConfig)->f_GetKey().m_Type == EEntityType_File)
						pParent = (*pFileConfig)->m_pParent;
					else
						pParent = &fg_RemoveQualifiers(*(*pFileConfig));

					CEntityKey NewEntityKey;
					NewEntityKey.m_Type = EEntityType_File;
					NewEntityKey.m_Name.m_Value = FileName;
					auto NewEntityMap = pParent->m_ChildEntitiesMap(NewEntityKey, pParent);
					auto &NewEntity = *NewEntityMap;
					auto &NewData = NewEntity.f_DataWritable();
					if (bFile)
					{
						NewData.m_Properties = (*pFileConfig)->f_Data().m_Properties;
						NewData.m_Properties.f_Remove(gc_ConstKey_Compile_Type);
					}
					NewData.m_Position = FilePos;
					TCMap<CPropertyKey, CEJsonSorted> Values;
					Values[gc_ConstKey_Compile_Type] = CompileType;
					m_BuildSystem.f_InitEntityForEvaluationNoEnv(NewEntity, Values, EEvaluatedPropertyType_External);
					{
						NewEntity.f_AddProperty
							(
								gc_ConstKey_Compile_PrecompilePrefixHeader
								, CBuildSystemSyntax::CRootValue{CBuildSystemSyntax::CValue{CEJsonSorted(gc_ConstString_XInternalCreate)}}
								, FilePos
							)
						;
					}
					{
						NewEntity.f_AddProperty
							(
								gc_ConstKey_Compile_XInternalPrecompiledHeaderOutputFile
								, CBuildSystemSyntax::CRootValue{CBuildSystemSyntax::CValue{CEJsonSorted(PCHFile)}}
								, FilePos
							)
						;
					}
					File.m_EnabledConfigs[iConfig.f_GetKey()] = fg_Explicit(&NewEntity);
				}
			}
		}
	}
}
