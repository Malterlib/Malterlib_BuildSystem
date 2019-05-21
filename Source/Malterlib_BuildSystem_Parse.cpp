// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_ParseData(CEntity &_RootEntity, CRegistryPreserveAll &_Registry, TCMap<CStr, CConfigurationType> *_pConfigurations) const
	{
		bool bFilterWorkspace = !mp_GenerateWorkspace.f_IsEmpty();
		bFilterWorkspace = false; // This does not work because the name of the workspace can be different from the name of the entity
		if (bFilterWorkspace)
		{
			TCLinkedList<CRegistryPreserveAll *> ToRemove;

			for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
			{
				CRegistryPreserveAll &Registry = *iReg;
				CStr Name = Registry.f_GetName();
				CStr Value = Registry.f_GetThisValue();
				ch8 Character = Name[0];
				switch (Character)
				{
				case '*':
					{
					}
					break;
				case '%':
					{
						if (Name == "%Workspace")
						{
							if (Value.f_Find("@(") < 0 && Value.f_Find("|") < 0 && Value != mp_GenerateWorkspace)
								ToRemove.f_Insert(&Registry);
							else
							{
								fp_ParseEntity(_RootEntity, Registry);
/*
								This does not work with dependencies that can add targets to the workspace

								auto pWorkspace = fp_ParseEntity(_RootEntity, Registry);
								mp_ValidTargetsValid = true;

								TCFunction<void (CEntity &_Entity)> flr_FindTargets;
								auto fl_FindTargets
									= [&](CEntity &_Entity)
									{
										for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
										{
											auto &ChildEntity = *iEntity;
											switch (ChildEntity.m_Key.m_Type)
											{
											case EEntityType_Group:
												{
													flr_FindTargets(ChildEntity);
												}
												break;
											case EEntityType_Target:
												{
													if (ChildEntity.m_Key.m_Name.f_Find("@(") >= 0)
														mp_ValidTargetsValid = false;
													mp_ValidTargets[ChildEntity.m_Key.m_Name];
													flr_FindTargets(ChildEntity);
												}
												break;
											case EEntityType_Dependency:
												{
													if (ChildEntity.m_Key.m_Name.f_Find("@(") >= 0)
														mp_ValidTargetsValid = false;
													mp_ValidTargets[ChildEntity.m_Key.m_Name];
												}
												break;
											}
										}
									}
								;

								flr_FindTargets = fl_FindTargets;

								fl_FindTargets(*pWorkspace);
*/
							}
						}
					}
					break;
				default:
					{
					}
					break;
				}
			}
			for (auto iRemove = ToRemove.f_GetIterator(); iRemove; ++iRemove)
			{
				delete *iRemove;
			}
		}
		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CRegistryPreserveAll &Registry = *iReg;
			CStr Name = Registry.f_GetName();
			CStr Value = Registry.f_GetThisValue();
			ch8 Character = Name[0];
			switch (Character)
			{
			case '*':
				{
					// Configuration
					if (!Value.f_IsEmpty())
						fsp_ThrowError(Registry, "Configuration operator cannot specify a value");
					if (_pConfigurations)
						fp_ParseConfigurationType(Name.f_Extract(1), Registry, *_pConfigurations);
					else
						fsp_ThrowError(Registry, "Configuration tuples cannot be specified in imports");
				}
				break;
			case '%':
				{
					if (bFilterWorkspace)
					{
						if (Name == "%Workspace")
							continue;
					}
					fp_ParseEntity(_RootEntity, Registry);
				}
				break;
			default:
				{
					if (fg_CharIsAlphabetical(Character) || fg_CharIsNumber(Character) || Character == 0 || Character == '_')
					{
						if ((Name == "Import" || Name == "Include") && !Value.f_IsEmpty())
							continue; // Error recovery to allow repositories to be handled
						TCLinkedList<CEntity *> Entities;
						Entities.f_Insert(&_RootEntity);
						fp_ParseProperty(Entities, Registry);
					}
					else
						fsp_ThrowError(Registry, CStr::CFormat("Unrecognized operator '{}'") << Name.f_Left(1));
				}
				break;
			}
//					CStr Name = Prop.f_GetName();
		}
		//_RootEntity.f_CheckParents();
	}

	void CBuildSystem::fp_ParseConfigurationConditions(CRegistryPreserveAll &_Registry, CBuildSystemConfiguration &_Configuration) const
	{
		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CRegistryPreserveAll &Registry = *iReg;
			CStr Name = Registry.f_GetName();
			CStr Value = Registry.f_GetThisValue();
			ch8 Character = Name[0];
			switch (Character)
			{
			case '&':
			case '|':
			case '!':
				CCondition::fs_ParseCondition(Registry, _Configuration.m_Condition);
				break;
			default:
				fsp_ThrowError(Registry, "Configurations only support conditions |, ! and & at root level");
				break;
			}
		}
	}

	void CBuildSystem::fp_ParseConfigurationType(CStr const &_Name, CRegistryPreserveAll &_Registry, TCMap<CStr, CConfigurationType> &o_Configurations) const
	{
		auto &Type = o_Configurations[_Name];

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CRegistryPreserveAll &Registry = *iReg;
			CStr Name = Registry.f_GetName();
			CStr Value = Registry.f_GetThisValue();
			if (Value.f_IsEmpty())
				fsp_ThrowError(Registry, "Configuration needs to specify description");
			auto &Configuration = Type.m_Configurations[Name];
			Configuration.m_Description = Value;
			Configuration.m_Position = Registry;

			fp_ParseConfigurationConditions(Registry, Configuration);
		}
	}

	void CBuildSystem::fp_ParsePropertyValue
		(
			EPropertyType _Type
			, CStr const &_PropertyName
			, TCLinkedList<CEntity *> &_Entities
			, CRegistryPreserveAll &_Registry
			, CCondition const &_Conditions
		) const
	{
		CStr PropertyName = _PropertyName;
		CStr PropertyValue = _Registry.f_GetThisValue();
		CProperty Property;
		Property.m_Key.m_Type = _Type;
		Property.m_Key.m_Name = PropertyName;
		Property.m_Value = PropertyValue;
		Property.m_Position = _Registry;
		Property.m_pRegistry = &_Registry;
		CCondition *pDestinationCondition = &Property.m_Condition;
		if (!_Conditions.m_Children.f_IsEmpty())
		{
			auto &Condition = Property.m_Condition.m_Children.f_Insert(fg_Construct());
			Condition.m_Type = EConditionType_And;
			Condition.m_Position = _Conditions.m_Position;

			Condition.m_Children.f_Insert(fg_Construct()) = _Conditions;
			pDestinationCondition = &Condition.m_Children.f_Insert(fg_Construct());
		}

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CRegistryPreserveAll &Registry = *iReg;
			CStr Name = Registry.f_GetName();
			CStr Value = Registry.f_GetThisValue();
			ch8 Character = Name[0];
			switch (Character)
			{
			case '&':
			case '|':
			case '!':
				{
					CCondition::fs_ParseCondition(Registry, *pDestinationCondition);
				}
				break;
			case '#':
				{
					Property.m_Debug = Value;
				}
				break;
			default:
				fsp_ThrowError(Registry, "Only conditions can be specified for a property");
				break;
			}
		}

		// Add to entities
		for (auto iEntity = _Entities.f_GetIterator(); iEntity; ++iEntity)
		{
			auto &NewProperty = (*iEntity)->m_Properties[Property.m_Key].f_Insert(Property);
			(*iEntity)->m_PropertiesEvalOrder.f_Insert(NewProperty);
		}
	}

	void CBuildSystem::fp_ParseProperty(TCLinkedList<CEntity *> &_Entities, CRegistryPreserveAll &_Registry) const
	{
		CStr PropertyType = _Registry.f_GetName();
		if (PropertyType.f_FindChar('.') >= 0)
		{
			CStr Name = PropertyType;
			PropertyType = fg_GetStrSep(Name, ".");

			EPropertyType Type = fg_PropertyTypeFromStr(PropertyType);
			if (PropertyType.f_IsEmpty() || Type == EPropertyType_Invalid)
				fsp_ThrowError(_Registry, CStr::CFormat("Unrecognized property '{}'") << PropertyType);

			CCondition Conditions;
			fp_ParsePropertyValue(Type, Name, _Entities, _Registry, Conditions);
			return;
		}
		if (!_Registry.f_GetThisValue().f_IsEmpty())
			fsp_ThrowError(_Registry, "Property types cannot have a value");

		EPropertyType Type = fg_PropertyTypeFromStr(PropertyType);
		if (PropertyType.f_IsEmpty() || Type == EPropertyType_Invalid)
			fsp_ThrowError(_Registry, CStr::CFormat("Unrecognized property '{}'") << PropertyType);

		CCondition Conditions;

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CRegistryPreserveAll &Registry = *iReg;
			CStr Name = Registry.f_GetName();
			ch8 Character = Name[0];
			if (fg_CharIsAlphabetical(Character) || fg_CharIsNumber(Character) || Character == 0 || Character == '_')
			{
				fp_ParsePropertyValue(Type, Name, _Entities, Registry, Conditions);
			}
			else
			{
				switch (Character)
				{
				case '&':
				case '|':
				case '!':
					{
						CCondition::fs_ParseCondition(Registry, Conditions);
					}
					break;
				default:
					fsp_ThrowError(Registry, CStr::CFormat("Unrecognized operator '{}'") << Name.f_Left(1));
					break;
				}

			}
		}
	}

	CEntity *CBuildSystem::fp_ParseEntity(CEntity &_Parent, CRegistryPreserveAll &_Registry) const
	{
		CStr EntityType = _Registry.f_GetName().f_Extract(1);
		CStr EntityName = _Registry.f_GetThisValue();

		EEntityType Type = fg_EntityTypeFromStr(EntityType);
		bool bAllowChildren = false;
		bool bMustHaveName = true;
		if (Type == EEntityType_Target)
		{
			if (mp_ValidTargetsValid)
			{
				if (!mp_ValidTargets.f_FindEqual(EntityName))
					return nullptr;
			}
			bAllowChildren = true;
			if (_Parent.m_Key.m_Type == EEntityType_Group)
			{
				auto *pParent = &_Parent;
				while
					(
						pParent->m_pParent
						&& pParent->m_pParent->m_Key.m_Type != EEntityType_Root
					)
				{
					pParent = pParent->m_pParent;
					if (pParent->m_Key.m_Type == EEntityType_Target)
						break;
				}

				if (pParent->m_Key.m_Type == EEntityType_Target)
					fsp_ThrowError(_Registry, "A target can not be specified inside another target");
			}
			else if (_Parent.m_Key.m_Type != EEntityType_Root && _Parent.m_Key.m_Type != EEntityType_Workspace && _Parent.m_Key.m_Type != EEntityType_Import)
				fsp_ThrowError(_Registry, "A target can only be specified at root, group, workspace or import scope");
		}
		else if (Type == EEntityType_Group)
		{
			bAllowChildren = true;
			if
				(
					_Parent.m_Key.m_Type != EEntityType_Target
					&& _Parent.m_Key.m_Type != EEntityType_Root
					&& _Parent.m_Key.m_Type != EEntityType_Workspace
					&& _Parent.m_Key.m_Type != EEntityType_Group
					&& _Parent.m_Key.m_Type != EEntityType_Import
				)
			{
				fsp_ThrowError(_Registry, "A group can only be specified in target, workspace, root, group or import scope");
			}
		}
		else if (Type == EEntityType_Workspace)
		{
			bAllowChildren = true;
			if (_Parent.m_Key.m_Type != EEntityType_Root && _Parent.m_Key.m_Type != EEntityType_Import)
				fsp_ThrowError(_Registry, "A workspace can only be specified at root or import scope");
		}
		else if (Type == EEntityType_File)
		{
			bAllowChildren = false;
			if (_Parent.m_Key.m_Type == EEntityType_Group)
			{
			}
			else if (_Parent.m_Key.m_Type != EEntityType_Target)
				fsp_ThrowError(_Registry, "A file can only be specified in target or group scope");
		}
		else if (Type == EEntityType_Dependency)
		{
			bAllowChildren = false;
			if (_Parent.m_Key.m_Type != EEntityType_Target && _Parent.m_Key.m_Type != EEntityType_Group)
				fsp_ThrowError(_Registry, "A dependency can only be specified in target or group scope");
		}
		else if (Type == EEntityType_GeneratorSetting)
		{
			bAllowChildren = true;
			bMustHaveName = false;
			if (_Parent.m_Key.m_Type != EEntityType_Root && _Parent.m_Key.m_Type != EEntityType_GeneratorSetting && _Parent.m_Key.m_Type != EEntityType_Import)
				fsp_ThrowError(_Registry, "Generator settings can only be specified at root, import or recursively");
		}
		else if (Type == EEntityType_Import)
		{
			bAllowChildren = true;
			if (_Parent.m_Key.m_Type != EEntityType_Root)
				fsp_ThrowError(_Registry, "Dynamic import can only be specified at root");
		}
		else if (Type == EEntityType_Repository)
		{
			bAllowChildren = true;
			if (_Parent.m_Key.m_Type != EEntityType_Root)
				fsp_ThrowError(_Registry, "Repositories can only be specified at root");
		}
		else if (Type == EEntityType_CreateTemplate)
		{
			bAllowChildren = true;
			if (_Parent.m_Key.m_Type != EEntityType_CreateTemplate)
				fsp_ThrowError(_Registry, "Create templates can only be specified at root");
		}
		else if (Type == EEntityType_GenerateFile)
		{
			bAllowChildren = false;
			if (_Parent.m_Key.m_Type == EEntityType_Group)
			{
			}
			else if (_Parent.m_Key.m_Type != EEntityType_Target && _Parent.m_Key.m_Type != EEntityType_Root)
				fsp_ThrowError(_Registry, "A file generation can only be specified in target, root or group scope");
		}
		else
		{
			fsp_ThrowError(_Registry, CStr::CFormat("Unrecognized entity type {}") << EntityType);
		}

		// Entity
		if (EntityName.f_IsEmpty() && bMustHaveName)
			fsp_ThrowError(_Registry, "An entity must have a name");

		CEntity *pEntity = nullptr;

		TCLinkedList<CEntity *> Entities;

		CEntityKey Key;
		Key.m_Name = EntityName;
		Key.m_Type = Type;
		CEntity *pRetEntity = nullptr;
		bool bMergeEntity = false;
		CEntity TempEntity(nullptr);
		switch (Type)
		{
		case EEntityType_Group:
		case EEntityType_Target:
		case EEntityType_Dependency:
		case EEntityType_GeneratorSetting:
		case EEntityType_GenerateFile:
		case EEntityType_Workspace:
		case EEntityType_Import:
		case EEntityType_Repository:
		case EEntityType_CreateTemplate:
			{
				auto pOldEntity = _Parent.m_ChildEntitiesMap.f_FindEqual(Key);

				if (pOldEntity)
				{
					if (Type == EEntityType_Import || Type == EEntityType_Repository)
					{
						pRetEntity = pOldEntity;
						pEntity = &TempEntity;
						TempEntity.m_Key = Key;
						bMergeEntity = true;
						break;
					}
					fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared") << EntityName);
				}

				auto &NewEntity = _Parent.m_ChildEntitiesMap(Key, &_Parent).f_GetResult();
				_Parent.m_ChildEntitiesOrdered.f_Insert(NewEntity);
				NewEntity.m_Key = Key;
				pEntity = &NewEntity;
				Entities.f_Insert(&NewEntity);
				NewEntity.m_Position = _Registry;
				pRetEntity = &NewEntity;
			}
			break;
		case EEntityType_File:
			{
				auto &FullFileName = EntityName;
				Key.m_Name = FullFileName;
				auto pOldEntity = _Parent.m_ChildEntitiesMap.f_FindEqual(Key);

				if (pOldEntity)
				{
					_Parent.m_ChildEntitiesMap.f_Remove(pOldEntity);
					//fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared (when adding pattern {})") << FullFileName << SearchPath);
				}

				auto &NewEntity = _Parent.m_ChildEntitiesMap(Key, &_Parent).f_GetResult();
				_Parent.m_ChildEntitiesOrdered.f_Insert(NewEntity);
				NewEntity.m_Key = Key;
				NewEntity.m_Position = _Registry;
				Entities.f_Insert(&NewEntity);
			}
			break;
		}

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CRegistryPreserveAll &Registry = *iReg;
			CStr Name = Registry.f_GetName();
			CStr Value = Registry.f_GetThisValue();
			ch8 Character = Name[0];
			switch (Character)
			{
			case '%':
				{
					if (!bAllowChildren)
						fsp_ThrowError(Registry, "Child entities not allow at this scope");
					DMibRequire(pEntity);
					fp_ParseEntity(*pEntity, Registry);
				}
				break;
			case '&':
			case '|':
			case '!':
				{
					CCondition Condition;
					CCondition::fs_ParseCondition(Registry, Condition);
					for (auto iEntity = Entities.f_GetIterator(); iEntity; ++iEntity)
					{
						(*iEntity)->m_Condition = Condition;
					}
				}
				break;
			default:
				{
					if (fg_CharIsAlphabetical(Character) || fg_CharIsNumber(Character) || Character == 0 || Character == '_')
						fp_ParseProperty(Entities, Registry);
					else
						fsp_ThrowError(Registry, CStr::CFormat("Unrecognized operator '{}'") << Name.f_Left(1));
				}
				break;
			}
		}

		if (bMergeEntity)
		{
			pRetEntity->f_CopyProperties(TempEntity);
			pRetEntity->f_MergeEntities(TempEntity);
			pRetEntity->m_Position = TempEntity.m_Position;
		}

		return pRetEntity;
	}
}
