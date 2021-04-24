// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

namespace NMib::NBuildSystem
{
	CStr CBuildSystem::fs_GetNameIdentifierString(CBuildSystemRegistry const &_Registry)
	{
		auto &Name = _Registry.f_GetName();
		if (!Name.f_IsValue())
			fsp_ThrowError(_Registry, "Only identifiers supported");

		auto &Value = Name.f_Value();

		if (!Value.f_IsIdentifier())
			fsp_ThrowError(_Registry, "Only identifiers supported");

		auto &Identifier = Value.f_Identifier();

		if (!Identifier.f_IsNameConstantString())
			fsp_ThrowError(_Registry, "Only constant identifiers supported");

		return Identifier.f_NameConstantString();
	}

	void CBuildSystem::fp_HandleKey
		(
			CBuildSystemRegistry &_Registry
			, TCFunction<void (CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOprator)> const &_fOnPrefix
			, TCFunction<void (CBuildSystemSyntax::CIdentifier const &_Identifier)> const &_fOnIdentifier
			, ch8 const *_pTypeError
			, EHandleKeyFlag _Flags
		) const
	{
		auto fThrowError = [&]
			{
				fsp_ThrowError(_Registry, _pTypeError);
			}
		;

		auto &Name = _Registry.f_GetName();

		if (Name.f_IsKeyPrefixOperator())
		{
			CFilePosition Position = _Registry;
			auto &PrefixOperator = Name.f_KeyPrefixOperator();

			if (!PrefixOperator.m_Right.f_IsIdentifier())
				fsp_ThrowError(_Registry, "Expected identifier for prefix operator");

			auto &Identifier = PrefixOperator.m_Right.f_Identifier();

			if (!Identifier.m_bEmptyPropertyType && !(_Flags & EHandleKeyFlag_AllowPropertyType))
				fsp_ThrowError(_Registry, "Didn't expect a property type for identifier");

			if (Identifier.m_EntityType != EEntityType_Invalid)
				fsp_ThrowError(_Registry, "Didn't expect an entity type for identifier");

			if (!Identifier.f_IsNameConstantString())
				fsp_ThrowError(_Registry, "Expected a constant string for identifier");

			_fOnPrefix(PrefixOperator);

			return;
		}
		else if (Name.f_IsValue())
		{
			auto &Value = Name.f_Value();
			if (Value.f_IsIdentifier())
			{
				_fOnIdentifier(Value.f_Identifier());
				return;
			}
		}

		fThrowError();
	}

	void CBuildSystem::fp_ParseData(CEntity &_RootEntity, CBuildSystemRegistry &_Registry, TCMap<CStr, CConfigurationType> *_pConfigurations) const
	{
		bool bFilterWorkspace = !mp_GenerateWorkspace.f_IsEmpty();
		bFilterWorkspace = false; // This does not work because the name of the workspace can be different from the name of the entity
		if (bFilterWorkspace)
		{
			TCLinkedList<CBuildSystemRegistry *> ToRemove;

			for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
			{
				auto &Registry = *iReg;

				fp_HandleKey
					(
						Registry
						, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
						{
							if (_PrefixOperator.m_Operator != CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Entity)
								return;

							auto &Identifier = _PrefixOperator.m_Right.f_Identifier();
							auto &EntityType = Identifier.f_NameConstantString();

							if (EntityType != "Workspace")
								return;

							auto &Value = Registry.f_GetThisValue();

							if (Value.m_Value.f_IsConstantString() && Value.m_Value.f_ConstantString() != mp_GenerateWorkspace)
								ToRemove.f_Insert(&Registry);
							else
								fp_ParseEntity(_RootEntity, Identifier, Registry);
						}
						, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
						{
						}
						, "Only entities, and configurations and properties are supported here"
						, EHandleKeyFlag_None
					)
				;
			}

			for (auto iRemove = ToRemove.f_GetIterator(); iRemove; ++iRemove)
				delete *iRemove;
		}

		TCSharedPointer<CCondition> pConditions = fg_Construct();

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			auto &Registry = *iReg;

			fp_HandleKey
				(
					Registry
					, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
					{
						auto &Value = Registry.f_GetThisValue();

						auto &Identifier = _PrefixOperator.m_Right.f_Identifier();

						if (_PrefixOperator.m_Operator == CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Entity)
						{
							auto &EntityType = Identifier.f_NameConstantString();

							if (bFilterWorkspace)
							{
								if (EntityType == "Workspace")
									return;
							}
							fp_ParseEntity(_RootEntity, Identifier, Registry);
						}
						else if (_PrefixOperator.m_Operator == CBuildSystemSyntax::CKeyPrefixOperator::EOperator_ConfigurationTuple)
						{
							auto &ConfigurationName = Identifier.f_NameConstantString();

							if (Value.m_Value.f_IsValid())
								fsp_ThrowError(Registry, "Configuration operator cannot specify a value");

							if (_pConfigurations)
								fp_ParseConfigurationType(ConfigurationName, Registry, *_pConfigurations);
							else
								fsp_ThrowError(Registry, "Configuration tuples cannot be specified in imports");
						}
						else
							fsp_ThrowError(Registry, "Unexpected prefix operator: {}"_f << _PrefixOperator.m_Operator);
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						auto &Value = Registry.f_GetThisValue();

						if
							(
								_Identifier.m_PropertyType == EPropertyType_Property
								&& _Identifier.m_EntityType == EEntityType_Invalid
								&& _Identifier.f_IsNameConstantString()
								&& (_Identifier.f_NameConstantString() == "Import" || _Identifier.f_NameConstantString() == "Include")
								&& Value.m_Value.f_IsValid()
							)
						{
							return; // Error recovery to allow repositories to be handled
						}

						fp_ParseProperty(&_RootEntity, _Identifier, Registry, pConditions, true);
					}
					, "Expected a prefix operator or an property identifier"
					, EHandleKeyFlag_None
				)
			;
		}

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			auto &Registry = *iReg;

			fp_HandleKey
				(
					Registry
					, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
					{
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						auto &Value = Registry.f_GetThisValue();

						if
							(
								_Identifier.m_PropertyType == EPropertyType_Property
								&& _Identifier.m_EntityType == EEntityType_Invalid
								&& _Identifier.f_IsNameConstantString()
								&& (_Identifier.f_NameConstantString() == "Import" || _Identifier.f_NameConstantString() == "Include")
								&& Value.m_Value.f_IsValid()
							)
						{
							return; // Error recovery to allow repositories to be handled
						}

						fp_ParseProperty(&_RootEntity, _Identifier, Registry, pConditions, false);
					}
					, "Expected a prefix operator or an property identifier"
					, EHandleKeyFlag_None
				)
			;
		}
	}

	void CBuildSystem::fp_ParseConfigurationConditions(CBuildSystemRegistry &_Registry, CBuildSystemConfiguration &_Configuration) const
	{
		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CBuildSystemRegistry &Registry = *iReg;
			if (!CCondition::fs_TryParseCondition(Registry, _Configuration.m_Condition))
				fsp_ThrowError(Registry, "Configurations only support conditions !!, !, | and & at root level");
		}
	}

	void CBuildSystem::fp_ParseConfigurationType(CStr const &_Name, CBuildSystemRegistry &_Registry, TCMap<CStr, CConfigurationType> &o_Configurations) const
	{
		auto &Type = o_Configurations[_Name];

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CBuildSystemRegistry &Registry = *iReg;
			auto Name = fs_GetNameIdentifierString(Registry);
			auto const &Value = Registry.f_GetThisValue();
			if (!Value.m_Value.f_IsConstantString())
				fsp_ThrowError(Registry, "Configuration needs to specify description as a constant string");
			auto &Configuration = Type.m_Configurations[Name];
			Configuration.m_Description = Value.m_Value.f_ConstantString();
			Configuration.m_Position = Registry;

			fp_ParseConfigurationConditions(Registry, Configuration);
		}
	}

	void CBuildSystem::fs_AddEntityUserType
		(
			CEntity &_DestinationEntity
			, CStr const &_TypeName
			, CBuildSystemSyntax::CType const &_Type
			, CFilePosition const &_Position
		)
	{
		if (auto *pChildPositions = _DestinationEntity.f_ChildDependentData().m_ChildrenUserTypes.f_FindEqual(_TypeName))
		{
			TCVector<CBuildSystemError> ChildErrors;
			for (auto &Position : *pChildPositions)
				ChildErrors.f_Insert(CBuildSystemError{Position, "Defined here"});
			fsp_ThrowError(_Position, "User type name collision with child entities", ChildErrors);
		}

		if (auto pExistingType = _DestinationEntity.f_Data().m_UserTypes.f_FindEqual(_TypeName))
			fsp_ThrowError(_Position, "User type name collision with previous type", TCVector<CBuildSystemError>{{pExistingType->m_Position, "Defined here"}});

		for (auto *pParent = _DestinationEntity.m_pParent; pParent; pParent = pParent->m_pParent)
		{
			if (auto pExistingType = pParent->f_Data().m_UserTypes.f_FindEqual(_TypeName))
				fsp_ThrowError(_Position, "User type name collision with parent entity", TCVector<CBuildSystemError>{{pExistingType->m_Position, "Defined here"}});

			if (auto *pUserType = pParent->f_ChildDependentData().m_ChildrenUserTypes.f_FindEqual(_TypeName); pUserType && pUserType->f_FindEqual(_Position))
				continue;

			pParent->f_ChildDependentDataWritable().m_ChildrenUserTypes[_TypeName][_Position];
		}

		_DestinationEntity.f_DataWritable().m_UserTypes[_TypeName] = {_Type, _Position};
	}

	void CBuildSystem::fs_AddEntityVariableDefinition
		(
			CBuildSystem const *_pBuildSystem
			, CEntity &_DestinationEntity
			, CPropertyKey const &_VariableName
			, CBuildSystemSyntax::CType const &_Type
			, CFilePosition const &_Position
			, CStr const &_Whitespace
			, TCSharedPointer<CCondition> const &_pConditions
		)
	{
		if (_pBuildSystem)
		{
			if (auto *pDefinition = _pBuildSystem->mp_BuiltinVariablesDefinitions.f_FindEqual(_VariableName))
			{
				if (_DestinationEntity.m_pParent == &_pBuildSystem->mp_Data.m_RootEntity)
				{
					if (_Type != pDefinition->m_Type)
						fsp_ThrowError(_Position, "You cannot redefine bultin variable '{}' with a different type"_f << _VariableName);
				}
				else
				{
					if (pDefinition->m_Type != _Type)
						fsp_ThrowError(_Position, "This name '{}' is reserved for a builtin functionality, and cannot be redefined"_f << _VariableName);
				}
			}

			if (_VariableName.m_Type == EPropertyType_Property && _pBuildSystem->mp_BuiltinFunctions.f_FindEqual(_VariableName.m_Name))
				fsp_ThrowError(_Position, "This name '{}' is reserved for a builtin function"_f << _VariableName);
		}

		if (auto *pChildPositions = _DestinationEntity.f_ChildDependentData().m_ChildrenVariableDefinitions.f_FindEqual(_VariableName))
		{
			TCVector<CBuildSystemError> ChildErrors;
			for (auto &Position : *pChildPositions)
				ChildErrors.f_Insert(CBuildSystemError{Position, "Defined here"});
			fsp_ThrowError(_Position, "User type name collision with child entities", ChildErrors);
		}

		if (auto pExistingType = _DestinationEntity.f_Data().m_VariableDefinitions.f_FindEqual(_VariableName))
			fsp_ThrowError(_Position, "User type name collision with previous type", TCVector<CBuildSystemError>{{pExistingType->m_Type.m_Position, "Defined here"}});

 		for (auto *pParent = _DestinationEntity.m_pParent; pParent; pParent = pParent->m_pParent)
		{
			if (auto pExistingType = pParent->f_Data().m_VariableDefinitions.f_FindEqual(_VariableName))
				fsp_ThrowError(_Position, "User type name collision with parent entity", TCVector<CBuildSystemError>{{pExistingType->m_Type.m_Position, "Defined here"}});

			if (auto *pDefinition = pParent->f_ChildDependentData().m_ChildrenVariableDefinitions.f_FindEqual(_VariableName); pDefinition && pDefinition->f_FindEqual(_Position))
				continue;

			pParent->f_ChildDependentDataWritable().m_ChildrenVariableDefinitions[_VariableName][_Position];
		}

		_DestinationEntity.f_DataWritable().m_VariableDefinitions[_VariableName] = {{_Type, _Position, _Whitespace}, _pConditions};
	}

	void CBuildSystem::fp_ParsePropertyValueDefines
		(
			EPropertyType _Type
			, CStr const &_PropertyName
			, CEntity *o_pEntity
			, CBuildSystemRegistry &_Registry
			, TCSharedPointer<CCondition> const &_pConditions
		) const
	{
		CProperty Property;
		Property.m_Key.m_Type = _Type;
		Property.m_Key.m_Name = _PropertyName;
		Property.m_Value = _Registry.f_GetThisValue();
		Property.m_Position = _Registry;

		if (Property.m_Key.m_Type == EPropertyType_Type)
		{
			if (!_Registry.f_GetChildren().f_IsEmpty())
				fsp_ThrowError(*_Registry.f_GetChildren().f_GetIterator(), "Type does not support conditions or debug");

			if (!Property.m_Value.m_Accessors.f_IsEmpty())
				fsp_ThrowError(Property.m_Position, "Type does not support accessors");

			if (!Property.m_Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
				fsp_ThrowError(Property.m_Position, "Type only supports define values");

			auto &Type = Property.m_Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type;

			if (o_pEntity)
				fs_AddEntityUserType(*o_pEntity, Property.m_Key.m_Name, Type, Property.m_Position);
 		}
		else if (Property.m_Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
		{
			if (!_Registry.f_GetChildren().f_IsEmpty())
				fsp_ThrowError(*_Registry.f_GetChildren().f_GetIterator(), "Define does not support conditions or debug");

			if (!Property.m_Value.m_Accessors.f_IsEmpty())
				fsp_ThrowError(Property.m_Position, "Define does not support accessors");

			auto &Type = Property.m_Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type;
			if (o_pEntity)
				fs_AddEntityVariableDefinition(this, *o_pEntity, Property.m_Key, Type, Property.m_Position, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_After), _pConditions);
		}
	}

	void CBuildSystem::fp_ParsePropertyValue
		(
			EPropertyType _Type
			, CStr const &_PropertyName
			, CEntity *o_pEntity
			, CBuildSystemRegistry &_Registry
			, CCondition const *_pParentConditions
		) const
	{
		CProperty Property;
		Property.m_Key.m_Type = _Type;
		Property.m_Key.m_Name = _PropertyName;
		Property.m_Value = _Registry.f_GetThisValue();
		Property.m_Position = _Registry;
		Property.m_pRegistry = &_Registry;

		if (Property.m_Key.m_Type == EPropertyType_Type)
			;
		else if (Property.m_Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
			;
		else
		{
			CCondition *pDestinationCondition = &Property.m_Condition;
			if (_pParentConditions && !_pParentConditions->m_Children.f_IsEmpty())
			{
				auto &Condition = Property.m_Condition.m_Children.f_Insert();
				Condition.m_Type = EConditionType_And;
				Condition.m_Position = _pParentConditions->m_Position;

				Condition.m_Children.f_Insert(*_pParentConditions);
				pDestinationCondition = &Condition.m_Children.f_Insert();
			}

			for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
			{
				CBuildSystemRegistry &Registry = *iReg;
				if (CCondition::fs_TryParseCondition(Registry, *pDestinationCondition))
					continue;

				fp_HandleKey
					(
						Registry
						, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
						{
							if (_PrefixOperator.m_Operator != CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Pragma)
								fsp_ThrowError(Registry, "Only conditions and debug pragmas statements can be specified for a property");

							if (!_PrefixOperator.m_Right.f_IsIdentifier())
								fsp_ThrowError(Registry, "Only identifiers are supported for pragma statements");

							auto &Identifier = _PrefixOperator.m_Right.f_Identifier();
							if (!Identifier.f_IsNameConstantString())
								fsp_ThrowError(Registry, "Only constant string identifiers are supported for pragma statements");

							if (Identifier.f_NameConstantString() != "Debug")
								fsp_ThrowError(Registry, "Only conditions and debug pragmas statements can be specified for a property");

							auto &Value = Registry.f_GetThisValue();
							if (!Value.m_Value.f_IsConstantString())
								fsp_ThrowError(Registry, "Only strings are supported for debug pragma statements");

							Property.m_Debug = Value.m_Value.f_ConstantString();
						}
						, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
						{
							fsp_ThrowError(Registry, "Only conditions and debug statements can be specified for a property");
						}
						, "Only conditions and debug statements can be specified for a property"
						, EHandleKeyFlag_AllowPropertyType
					)
				;
			}

			Property.m_bNeedPerFile = Property.m_Condition.f_NeedPerFile();

			if (o_pEntity)
			{
				auto &EntityData = o_pEntity->f_DataWritable();
				[[maybe_unused]] auto &NewProperty = EntityData.m_Properties[Property.m_Key].f_Insert(fg_Move(Property));
				DMibCheck(NewProperty.m_Key.m_Type != EPropertyType_Type);

				if (Property.m_Key.m_Name == "FullEval")
					EntityData.m_HasFullEval |= 1 << Property.m_Key.m_Type;
			}
		}
	}

 	void CBuildSystem::fp_ParseProperty
		(
			CEntity *o_pEntitiy
			, CBuildSystemSyntax::CIdentifier const &_Identifier
			, CBuildSystemRegistry &_Registry
			, TCSharedPointer<CCondition> const &_pConditions
			, bool _bDefines
		) const
	{
		if (_Identifier.m_EntityType != EEntityType_Invalid)
			fsp_ThrowError(_Registry, "You cannot specify an entity type for a property expression");

		if (!_Identifier.f_IsNameConstantString())
			fsp_ThrowError(_Registry, "You cannot use dynamic naming for a property expression");

		auto &Identifier = _Identifier.f_NameConstantString();

		if (!_Identifier.m_bEmptyPropertyType)
		{
			if (_bDefines)
				fp_ParsePropertyValueDefines(_Identifier.m_PropertyType, Identifier, o_pEntitiy, _Registry, _pConditions);
			else
				fp_ParsePropertyValue(_Identifier.m_PropertyType, Identifier, o_pEntitiy, _Registry, nullptr);
			return;
		}

		if (_Registry.f_GetThisValue().m_Value.f_IsValid())
			fsp_ThrowError(_Registry, "Property groups cannot have a value");

		EPropertyType Type = fg_PropertyTypeFromStr(Identifier);
		if (Identifier.f_IsEmpty() || Type == EPropertyType_Invalid)
			fsp_ThrowError(_Registry, "Unrecognized property type '{}'"_f << _Identifier.m_Name);

		CCondition Conditions;

		TCSharedPointer<CCondition> pConditions;

		auto fGetConditions = [&]
			{
				if (!pConditions)
					pConditions = fg_Construct(Conditions);

				return pConditions;
			}
		;

		bool bWasCondition = true;

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			auto &Registry = *iReg;
			if (CCondition::fs_TryParseCondition(Registry, Conditions))
			{
				if (!bWasCondition)
					fsp_ThrowError(Registry, "Conditions needs to specified first");
				continue;
			}

			bWasCondition = false;

			fp_HandleKey
				(
					Registry
					, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
					{
						if (_PrefixOperator.m_Operator == CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Entity)
							fsp_ThrowError(Registry, "You cannot specify entities inside property groups");
						else if (_PrefixOperator.m_Operator == CBuildSystemSyntax::CKeyPrefixOperator::EOperator_ConfigurationTuple)
							fsp_ThrowError(Registry, "You cannot specify configurations inside property groups");
						else
							fsp_ThrowError(Registry, "Unexpected prefix operator: {}"_f << _PrefixOperator.m_Operator);
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						if (_Identifier.m_EntityType != EEntityType_Invalid)
							fsp_ThrowError(Registry, "You cannot specify an entity type for a property expression");

						if (!_Identifier.m_bEmptyPropertyType)
							fsp_ThrowError(Registry, "You cannot specify an property type for propertsie in a property group");

						if (!_Identifier.f_IsNameConstantString())
							fsp_ThrowError(Registry, "You cannot use dynamic naming for a property expression");

						if (_bDefines)
							fp_ParsePropertyValueDefines(Type, _Identifier.f_NameConstantString(), o_pEntitiy, Registry, fGetConditions());
						else
							fp_ParsePropertyValue(Type, _Identifier.f_NameConstantString(), o_pEntitiy, Registry, &Conditions);
					}
					, "Only conditions and properties are supported here"
					, EHandleKeyFlag_AllowPropertyType
				)
			;
		}
	}

	CEntity *CBuildSystem::fp_ParseEntity(CEntity &_Parent, CBuildSystemSyntax::CIdentifier const &_Identifier, CBuildSystemRegistry &_Registry) const
	{
		auto const &EntityType = _Identifier.f_NameConstantString();

		auto &ParentKey = _Parent.f_GetKey();

		EEntityType Type = fg_EntityTypeFromStr(EntityType);
		bool bAllowChildren = false;
		bool bMustHaveName = true;
		if (Type == EEntityType_Target)
		{
			bAllowChildren = true;
			if (ParentKey.m_Type == EEntityType_Group)
			{
				auto *pParent = &_Parent;
				while
					(
						pParent->m_pParent
						&& pParent->m_pParent->f_GetKey().m_Type != EEntityType_Root
					)
				{
					pParent = pParent->m_pParent;
					if (pParent->f_GetKey().m_Type == EEntityType_Target)
						break;
				}

				if (pParent->f_GetKey().m_Type == EEntityType_Target)
					fsp_ThrowError(_Registry, "A target can not be specified inside another target");
			}
			else if (ParentKey.m_Type != EEntityType_Root && ParentKey.m_Type != EEntityType_Workspace && ParentKey.m_Type != EEntityType_Import)
				fsp_ThrowError(_Registry, "A target can only be specified at root, group, workspace or import scope");
		}
		else if (Type == EEntityType_Group)
		{
			bAllowChildren = true;
			if
				(
					ParentKey.m_Type != EEntityType_Target
					&& ParentKey.m_Type != EEntityType_Root
					&& ParentKey.m_Type != EEntityType_Workspace
					&& ParentKey.m_Type != EEntityType_Group
					&& ParentKey.m_Type != EEntityType_Import
				)
			{
				fsp_ThrowError(_Registry, "A group can only be specified in target, workspace, root, group or import scope");
			}
		}
		else if (Type == EEntityType_Workspace)
		{
			bAllowChildren = true;
			if (ParentKey.m_Type != EEntityType_Root && ParentKey.m_Type != EEntityType_Import)
				fsp_ThrowError(_Registry, "A workspace can only be specified at root or import scope");
		}
		else if (Type == EEntityType_File)
		{
			bAllowChildren = false;
			if (ParentKey.m_Type == EEntityType_Group)
			{
			}
			else if (ParentKey.m_Type != EEntityType_Target)
				fsp_ThrowError(_Registry, "A file can only be specified in target or group scope");
		}
		else if (Type == EEntityType_Dependency)
		{
			bAllowChildren = false;
			if (ParentKey.m_Type != EEntityType_Target && ParentKey.m_Type != EEntityType_Group)
				fsp_ThrowError(_Registry, "A dependency can only be specified in target or group scope");
		}
		else if (Type == EEntityType_GeneratorSetting)
		{
			bAllowChildren = true;
			bMustHaveName = false;
			if (ParentKey.m_Type != EEntityType_Root && ParentKey.m_Type != EEntityType_GeneratorSetting && ParentKey.m_Type != EEntityType_Import)
				fsp_ThrowError(_Registry, "Generator settings can only be specified at root, import or recursively");
		}
		else if (Type == EEntityType_Import)
		{
			bAllowChildren = true;
			if (ParentKey.m_Type != EEntityType_Root)
				fsp_ThrowError(_Registry, "Dynamic import can only be specified at root");
		}
		else if (Type == EEntityType_Repository)
		{
			bAllowChildren = true;
			if (ParentKey.m_Type != EEntityType_Root)
				fsp_ThrowError(_Registry, "Repositories can only be specified at root");
		}
		else if (Type == EEntityType_CreateTemplate)
		{
			bAllowChildren = true;
			if (ParentKey.m_Type != EEntityType_CreateTemplate)
				fsp_ThrowError(_Registry, "Create templates can only be specified at root");
		}
		else if (Type == EEntityType_GenerateFile)
		{
			bAllowChildren = false;
			if (ParentKey.m_Type == EEntityType_Group)
			{
			}
			else if (ParentKey.m_Type != EEntityType_Target && ParentKey.m_Type != EEntityType_Root)
				fsp_ThrowError(_Registry, "A file generation can only be specified in target, root or group scope");
		}
		else
		{
			fsp_ThrowError(_Registry, CStr::CFormat("Unrecognized entity type {}") << EntityType);
		}

		auto const &EntityName = _Registry.f_GetThisValue();

		// Entity
		if (!EntityName.m_Value.f_IsValid() && bMustHaveName)
			fsp_ThrowError(_Registry, "An entity must have a name");

		if (!EntityName.m_Accessors.f_IsEmpty())
			fsp_ThrowError(_Registry, "An entity cannot have accessors");

		CEntityKey Key;
		Key.m_Name = EntityName.m_Value;
		Key.m_Type = Type;

		CEntity *pEntity = nullptr;
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
						bMergeEntity = true;
						break;
					}
					fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared") << EntityName);
				}

				auto &NewEntity = _Parent.m_ChildEntitiesMap(Key, &_Parent).f_GetResult();
				pEntity = &NewEntity;
				NewEntity.f_DataWritable().m_Position = _Registry;
				pRetEntity = &NewEntity;
			}
			break;
		case EEntityType_File:
			{
				auto pOldEntity = _Parent.m_ChildEntitiesMap.f_FindEqual(Key);

				if (pOldEntity)
					_Parent.m_ChildEntitiesMap.f_Remove(pOldEntity);

				auto &NewEntity = _Parent.m_ChildEntitiesMap(Key, &_Parent).f_GetResult();
				NewEntity.f_DataWritable().m_Position = _Registry;
				pEntity = &NewEntity;
			}
			break;
		case EEntityType_Invalid:
		case EEntityType_Root:
			DMibNeverGetHere;
			break;
		}

		bool bWasCondition = true;

		auto &Conditions = pEntity->f_DataWritable().m_Condition;

		TCSharedPointer<CCondition> pConditions;

		auto fGetConditions = [&]
			{
				if (!pConditions)
					pConditions = fg_Construct(Conditions);

				return pConditions;
			}
		;

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CBuildSystemRegistry &Registry = *iReg;
			if (CCondition::fs_TryParseCondition(Registry, Conditions))
			{
				if (!bWasCondition)
					fsp_ThrowError(Registry, "Conditions needs to specified first");

				continue;
			}

			bWasCondition = false;

			fp_HandleKey
				(
					Registry
					, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
					{
						auto &Identifier = _PrefixOperator.m_Right.f_Identifier();

						if (_PrefixOperator.m_Operator == CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Entity)
						{
							if (!bAllowChildren)
								fsp_ThrowError(Registry, "Child entities are not allowed at this scope");

							fp_ParseEntity(*pEntity, Identifier, Registry);
						}
						else if (_PrefixOperator.m_Operator == CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Pragma)
						{
							if (!_PrefixOperator.m_Right.f_IsIdentifier())
								fsp_ThrowError(Registry, "Only identifiers are supported for pragma statements");

							auto &Identifier = _PrefixOperator.m_Right.f_Identifier();
							if (!Identifier.f_IsNameConstantString())
								fsp_ThrowError(Registry, "Only constant string identifiers are supported for pragma statements");

							if (Identifier.f_NameConstantString() != "Debug")
								fsp_ThrowError(Registry, "Only conditions and debug pragmas statements can be specified for a property");

							auto &Value = Registry.f_GetThisValue();
							if (!Value.m_Value.f_IsConstantString())
								fsp_ThrowError(Registry, "Only strings are supported for debug pragma statements");

							pEntity->f_DataWritable().m_Debug = Value.m_Value.f_ConstantString();
						}
						else
							fsp_ThrowError(Registry, "Unexpected prefix operator: {}"_f << _PrefixOperator.m_Operator);
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						fp_ParseProperty(pEntity, _Identifier, Registry, fGetConditions(), true);
					}
					, "Expected a prefix operator or an property identifier"
					, EHandleKeyFlag_None
				)
			;
		}

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CBuildSystemRegistry &Registry = *iReg;
			CCondition Condition;
			if (CCondition::fs_TryParseCondition(Registry, Condition))
				continue;

			fp_HandleKey
				(
					Registry
					, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
					{
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						fp_ParseProperty(pEntity, _Identifier, Registry, fGetConditions(), false);
					}
					, "Expected a prefix operator or an property identifier"
					, EHandleKeyFlag_None
				)
			;
		}

		if (bMergeEntity)
		{
			pRetEntity->f_CopyProperties(TempEntity);
			pRetEntity->f_CopyEntities(TempEntity, EEntityCopyFlag_MergeEntities);
			pRetEntity->f_DataWritable().m_Position = TempEntity.f_Data().m_Position;
		}

		return pRetEntity;
	}
}
