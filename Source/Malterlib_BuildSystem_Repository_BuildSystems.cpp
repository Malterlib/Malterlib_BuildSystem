// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem::NRepository
{
	namespace
	{
		TCSet<CStr> fg_ReadRepositorySet(CEJsonSorted const &_Json, CStr const &_Name)
		{
			TCSet<CStr> Repositories;
			if (auto *pRepositoriesJson = _Json.f_GetMember(_Name, EEJsonType_Object))
			{
				for (auto &RepositoryJson : pRepositoriesJson->f_Object())
					Repositories[RepositoryJson.f_Name()];
			}

			return Repositories;
		}

		CEJsonSorted fg_RepositorySetJson(TCSet<CStr> const &_Repositories)
		{
			CEJsonSorted Json = EJsonType_Object;
			for (auto &Repository : _Repositories)
				Json[Repository] = 1;

			return Json;
		}

		CRepositoryState fg_ParseRepositoryState(CEJsonSorted const &_Json)
		{
			CRepositoryState State;
			State.m_SeenRepositories = fg_ReadRepositorySet(_Json, "SeenRepositories");
			State.m_LeaveWhenRemoved = fg_ReadRepositorySet(_Json, "LeaveWhenRemoved");

			return State;
		}

		CEJsonSorted fg_RepositoryStateJson(CRepositoryState const &_State)
		{
			CEJsonSorted Json;
			Json["SeenRepositories"] = fg_RepositorySetJson(_State.m_SeenRepositories);
			Json["LeaveWhenRemoved"] = fg_RepositorySetJson(_State.m_LeaveWhenRemoved);

			return Json;
		}
	}

	namespace
	{
		// Older MTools update the states of build systems without the registry, and add build systems in BuildSystem of the
		// checkout, so the states that exist are read again. The registry only remembers the states that are gone.
		void fg_ReadStates(CBuildSystemsRegistry &o_Registry, CStr const &_BaseDir)
		{
			for (auto &Directory : CFile::fs_FindFiles(_BaseDir / "BuildSystem/*", EFileAttrib_Directory))
			{
				if (CFile::fs_FileExists(Directory / "RepositoryState.json"))
					o_Registry.m_BuildSystems[CBuildSystemsRegistry::fs_Name(_BaseDir, Directory)];
			}

			for (auto &State : o_Registry.m_BuildSystems)
			{
				CStr StateFile = CBuildSystemsRegistry::fs_OutputDir(_BaseDir, o_Registry.m_BuildSystems.fs_GetKey(State)) / "RepositoryState.json";
				if (CFile::fs_FileExists(StateFile))
					State = CRepositoryState::fs_Read(StateFile);
			}
		}
	}

	CRepositoryState CRepositoryState::fs_Read(CStr const &_StateFile)
	{
		if (!CFile::fs_FileExists(_StateFile))
			return {};

		return fg_ParseRepositoryState(CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(_StateFile, true), _StateFile));
	}

	CStr CBuildSystemsRegistry::fs_File(CStr const &_BaseDir)
	{
		return _BaseDir / "BuildSystem/BuildSystems.json";
	}

	CStr CBuildSystemsRegistry::fs_Name(CStr const &_BaseDir, CStr const &_OutputDir)
	{
		if (_OutputDir.f_StartsWith(_BaseDir + "/"))
			return CFile::fs_MakePathRelative(_OutputDir, _BaseDir);

		return _OutputDir;
	}

	CStr CBuildSystemsRegistry::fs_OutputDir(CStr const &_BaseDir, CStr const &_Name)
	{
		return CFile::fs_GetExpandedPath(_Name, _BaseDir);
	}

	CBuildSystemsRegistry CBuildSystemsRegistry::fs_Read(CStr const &_BaseDir)
	{
		CBuildSystemsRegistry Registry;

		CStr RegistryFile = fs_File(_BaseDir);
		if (!CFile::fs_FileExists(RegistryFile))
		{
			fg_ReadStates(Registry, _BaseDir);
			return Registry;
		}

		auto RegistryJson = CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(RegistryFile, true), RegistryFile);

		if (auto *pBuildSystemsJson = RegistryJson.f_GetMember("BuildSystems", EEJsonType_Object))
		{
			for (auto &BuildSystemJson : pBuildSystemsJson->f_Object())
				Registry.m_BuildSystems[BuildSystemJson.f_Name()] = fg_ParseRepositoryState(BuildSystemJson.f_Value());
		}

		if (auto *pOrphanedJson = RegistryJson.f_GetMember("Orphaned", EEJsonType_Object))
			Registry.m_Orphaned = fg_ParseRepositoryState(*pOrphanedJson);

		Registry.m_KeptRepositories = fg_ReadRepositorySet(RegistryJson, "KeptRepositories");

		fg_ReadStates(Registry, _BaseDir);

		return Registry;
	}

	void CBuildSystemsRegistry::f_Write(CStr const &_BaseDir) const
	{
		CEJsonSorted RegistryJson;

		auto &BuildSystemsJson = RegistryJson["BuildSystems"];
		BuildSystemsJson = EJsonType_Object;
		for (auto &State : m_BuildSystems)
			BuildSystemsJson[m_BuildSystems.fs_GetKey(State)] = fg_RepositoryStateJson(State);

		RegistryJson["Orphaned"] = fg_RepositoryStateJson(m_Orphaned);
		RegistryJson["KeptRepositories"] = fg_RepositorySetJson(m_KeptRepositories);

		CStr RegistryFile = fs_File(_BaseDir);
		CByteVector FileData;
		CFile::fs_WriteStringToVector(FileData, RegistryJson.f_ToString(), false);
		CFile::fs_CreateDirectory(CFile::fs_GetPath(RegistryFile));
		CFile::fs_CopyFileDiff(FileData, RegistryFile, CTime::fs_NowUTC());
	}
}
