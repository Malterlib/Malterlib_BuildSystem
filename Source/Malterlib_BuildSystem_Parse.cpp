// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

							if (EntityType != gc_ConstString_Workspace.m_String)
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
				fg_DeleteObject(NMemory::CDefaultAllocator(), *iRemove);
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
								if (EntityType == gc_ConstString_Workspace.m_String)
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
								&& (_Identifier.f_NameConstantString() == gc_ConstString_Import.m_String || _Identifier.f_NameConstantString() == gc_ConstString_Include.m_String)
								&& Value.m_Value.f_IsValid()
							)
						{
							return; // Error recovery to allow repositories to be handled
						}

						fp_ParseProperty(_RootEntity, _Identifier, Registry, pConditions, true);
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
								&& (_Identifier.f_NameConstantString() == gc_ConstString_Import.m_String || _Identifier.f_NameConstantString() == gc_ConstString_Include.m_String)
								&& Value.m_Value.f_IsValid()
							)
						{
							return; // Error recovery to allow repositories to be handled
						}

						fp_ParseProperty(_RootEntity, _Identifier, Registry, pConditions, false);
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
			, TCSharedPointer<CCondition> const &_pConditions
			, EPropertyFlag _PropertyFlags
		)
	{
		if (auto *pChildPositions = _DestinationEntity.f_ChildDependentData().m_ChildrenUserTypes.f_FindEqual(_TypeName))
		{
			TCVector<CBuildSystemError> ChildErrors;
			for (auto &Position : *pChildPositions)
				ChildErrors.f_Insert(CBuildSystemError{CBuildSystemUniquePositions(Position, "Child type"), "Defined here"});
			fsp_ThrowError(_Position, "User type name collision with child entities", ChildErrors);
		}

		if (auto pExistingTypes = _DestinationEntity.f_Data().m_UserTypes.f_FindEqual(_TypeName))
		{
			for (auto &ExistingType : *pExistingTypes)
			{
				if ((!ExistingType.m_pConditions && !_pConditions) || (( !!ExistingType.m_pConditions && !!_pConditions) && *ExistingType.m_pConditions == *_pConditions))
				{
					fs_ThrowError
						(
							CBuildSystemUniquePositions(_Position, "User Type")
							, "User type name collision with previous type"
							, TCVector<CBuildSystemError>{{CBuildSystemUniquePositions(ExistingType.m_Type.m_Position, "User Type"), "Defined here"}}
						)
					;
				}
			}
		}

		for (auto *pParent = _DestinationEntity.m_pParent; pParent; pParent = pParent->m_pParent)
		{
			if (auto pExistingType = pParent->f_Data().m_UserTypes.f_FindEqual(_TypeName))
			{
				fs_ThrowError
					(
						CBuildSystemUniquePositions(_Position, "User Type")
						, "User type name collision with parent entity"
						, TCVector<CBuildSystemError>{{CBuildSystemUniquePositions(pExistingType->f_GetFirst().m_Type.m_Position, "User Type"), "Defined here"}}
					)
				;
			}

			if (auto *pUserType = pParent->f_ChildDependentData().m_ChildrenUserTypes.f_FindEqual(_TypeName); pUserType && pUserType->f_FindEqual(_Position))
				continue;

			pParent->f_ChildDependentDataWritable().m_ChildrenUserTypes[_TypeName][_Position];
		}

		_DestinationEntity.f_DataWritable().m_UserTypes[_TypeName].f_Insert({{_Type, _Position}, _pConditions, _PropertyFlags});
	}

	void CBuildSystem::fs_AddEntityVariableDefinition
		(
			CBuildSystem const *_pBuildSystem
			, CEntity &_DestinationEntity
			, CPropertyKeyReference const &_VariableName
			, CBuildSystemSyntax::CType const &_Type
			, CFilePosition const &_Position
			, CStr const &_Whitespace
			, TCSharedPointer<CCondition> const &_pConditions
			, EPropertyFlag _PropertyFlags
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

			if (_VariableName.f_GetType() == EPropertyType_Property && _pBuildSystem->mp_BuiltinFunctions.f_FindEqual(_VariableName.m_Name))
				fsp_ThrowError(_Position, "This name '{}' is reserved for a builtin function"_f << _VariableName);
		}

		if (auto *pChildPositions = _DestinationEntity.f_ChildDependentData().m_ChildrenVariableDefinitions.f_FindEqual(_VariableName))
		{
			TCVector<CBuildSystemError> ChildErrors;
			for (auto &Position : *pChildPositions)
				ChildErrors.f_Insert(CBuildSystemError{CBuildSystemUniquePositions(Position, "Variable definition"), "Defined here"});
			fsp_ThrowError(_Position, "Variable definition collision with child entities", ChildErrors);
		}

		if (auto pExistingTypes = _DestinationEntity.f_Data().m_VariableDefinitions.f_FindEqual(_VariableName))
		{
			for (auto &ExistingType : *pExistingTypes)
			{
				if ((!ExistingType.m_pConditions && !_pConditions) || (( !!ExistingType.m_pConditions && !!_pConditions) && *ExistingType.m_pConditions == *_pConditions))
				{
					fs_ThrowError
						(
							CBuildSystemUniquePositions(_Position, "Variable definition")
							, "Variable definition collision with previous variable"
							, TCVector<CBuildSystemError>{{CBuildSystemUniquePositions(ExistingType.m_Type.m_Position, "Variable definition"), "Defined here"}}
						)
					;
				}
			}
		}

		for (auto *pParent = _DestinationEntity.m_pParent; pParent; pParent = pParent->m_pParent)
		{
			if (auto pExistingType = pParent->f_Data().m_VariableDefinitions.f_FindEqual(_VariableName))
			{
				fs_ThrowError
					(
						CBuildSystemUniquePositions(_Position, "Variable definition")
						, "Variable definition with parent entity"
						, TCVector<CBuildSystemError>{{CBuildSystemUniquePositions(pExistingType->f_GetFirst().m_Type.m_Position, "Variable definition"), "Defined here"}}
					)
				;
			}

			if (auto *pDefinition = pParent->f_ChildDependentData().m_ChildrenVariableDefinitions.f_FindEqual(_VariableName); pDefinition && pDefinition->f_FindEqual(_Position))
				continue;

			pParent->f_ChildDependentDataWritable().m_ChildrenVariableDefinitions[_VariableName][_Position];
		}

		_DestinationEntity.f_DataWritable().m_VariableDefinitions[_VariableName].f_Insert({{_Type, _Position, _Whitespace}, _pConditions, _PropertyFlags});
	}

	void CBuildSystem::fp_ParsePropertyValueDefines
		(
			CPropertyKeyReference const &_VariableName
			, CEntity &o_Entity
			, CBuildSystemRegistry &_Registry
			, TCSharedPointer<CCondition> const &_pConditions
			, CStr const &_Namespaces
		) const
	{
		auto &Value = _Registry.f_GetThisValue();
		if (_VariableName.f_GetType() == EPropertyType_Type)
		{
			TCSharedPointer<CCondition> pConditions;
			EPropertyFlag DebugFlags = EPropertyFlag_None;

			fp_ParseConditionsAndDebug(_Registry, _pConditions, DebugFlags, pConditions);

			if (!Value.m_Accessors.f_IsEmpty())
				fsp_ThrowError(_Registry, "Type does not support accessors");

			if (!Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
				fsp_ThrowError(_Registry, "Type only supports define values");

			auto &Type = Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type;

			CStr TypeName;

			if (_Namespaces)
				TypeName = "{}::{}"_f << _Namespaces << _VariableName.m_Name;
			else
				TypeName = _VariableName.m_Name;

			fs_AddEntityUserType(o_Entity, TypeName, Type, _Registry, pConditions, DebugFlags);
		}
		else if (Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
		{
			TCSharedPointer<CCondition> pConditions;
			EPropertyFlag DebugFlags = EPropertyFlag_None;

			fp_ParseConditionsAndDebug(_Registry, _pConditions, DebugFlags, pConditions);

			if (!Value.m_Accessors.f_IsEmpty())
				fsp_ThrowError(_Registry, "Define does not support accessors");

			auto &Type = Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type;
			fs_AddEntityVariableDefinition
				(
					this
					, o_Entity
					, _VariableName
					, Type
					, _Registry.f_GetLocation()
					, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_After)
					, pConditions
					, DebugFlags
				)
			;
		}
	}

	EPropertyFlag CBuildSystem::fsp_ParseDebugFlags(CFilePosition const &_Position, CStr const &_String)
	{
		EPropertyFlag Flags = EPropertyFlag_None;
		for (auto Value : _String.f_Split(","))
		{
			Value = Value.f_Trim();
			if (Value == gc_ConstString_TraceEval.m_String)
				Flags |= EPropertyFlag_TraceEval;
			else if (Value == gc_ConstString_TraceEvalSuccess.m_String)
				Flags |= EPropertyFlag_TraceEvalSuccess;
			else if (Value == gc_ConstString_TraceCondition.m_String)
				Flags |= EPropertyFlag_TraceCondition;
			else
				fsp_ThrowError(_Position, "Unknown debug flag: {}"_f << Value);
		}

		return Flags;
	}

	void CBuildSystem::fp_ParseConditionsAndDebug
		(
			CBuildSystemRegistry &_Registry
			, NStorage::TCSharedPointer<CCondition> const &_pConditions
			, EPropertyFlag &o_DebugFlags
			, NStorage::TCSharedPointer<CCondition> &o_pConditions
		) const
	{
		CCondition TempCondition;
		CCondition *pDestinationCondition = &TempCondition;

		for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
		{
			CBuildSystemRegistry &Registry = *iReg;
			if (CCondition::fs_TryParseCondition(Registry, *pDestinationCondition))
			{
				if (!o_pConditions)
				{
					o_pConditions = fg_Construct();
					if (_pConditions && !_pConditions->m_Children.f_IsEmpty())
					{
						auto &Condition = o_pConditions->m_Children.f_Insert();
						Condition.m_Type = EConditionType_And;
						Condition.m_Position = _pConditions->m_Position;

						Condition.m_Children.f_Insert(*_pConditions);
						pDestinationCondition = &Condition.m_Children.f_Insert();
					}
					else
						pDestinationCondition = o_pConditions.f_Get();

					pDestinationCondition->m_Children.f_Insert(fg_Move(TempCondition.m_Children));
				}
				continue;
			}

			fp_HandleKey
				(
					Registry
					, [&](CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOperator)
					{
						if (_PrefixOperator.m_Operator != CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Pragma)
							fsp_ThrowError(Registry, "Only conditions and debug pragmas statements can be specified here");

						if (!_PrefixOperator.m_Right.f_IsIdentifier())
							fsp_ThrowError(Registry, "Only identifiers are supported for pragma statements");

						auto &Identifier = _PrefixOperator.m_Right.f_Identifier();
						if (!Identifier.f_IsNameConstantString())
							fsp_ThrowError(Registry, "Only constant string identifiers are supported for pragma statements");

						if (Identifier.f_NameConstantString() != "Debug")
							fsp_ThrowError(Registry, "Only conditions and debug pragmas statements can be specified here");

						auto &Value = Registry.f_GetThisValue();
						if (!Value.m_Value.f_IsConstantString())
							fsp_ThrowError(Registry, "Only strings are supported for debug pragma statements");

						o_DebugFlags = fsp_ParseDebugFlags(Registry.f_GetLocation(), Value.m_Value.f_ConstantString());
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						fsp_ThrowError(Registry, "Only conditions and debug statements can be specified here");
					}
					, "Only conditions and debug statements can be specified here"
					, EHandleKeyFlag_AllowPropertyType
				)
			;
		}

		if (!o_pConditions)
			o_pConditions = _pConditions;
	}

	void CBuildSystem::fp_ParsePropertyValue
		(
			CPropertyKeyReference const &_PropertyKey
			, CEntity &o_Entity
			, CBuildSystemRegistry &_Registry
			, TCSharedPointer<CCondition> const &_pConditions
		) const
	{
		if (_PropertyKey.f_GetType() == EPropertyType_Type)
			return;
		else if (_Registry.f_GetThisValue().m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
			return;

		CProperty Property;
		//Property.m_Key = CPropertyKey(_PropertyKey);
		Property.m_Value = _Registry.f_GetThisValue();
		Property.m_Position = _Registry;
		Property.m_pRegistry = &_Registry;

		fp_ParseConditionsAndDebug(_Registry, _pConditions, Property.m_Flags, Property.m_pCondition);

		if (Property.m_pCondition && Property.m_pCondition->f_NeedPerFile())
			Property.m_Flags |= EPropertyFlag_NeedPerFile;

		auto &EntityData = o_Entity.f_DataWritable();
		[[maybe_unused]] auto &NewProperty = EntityData.m_Properties[_PropertyKey].f_Insert(fg_Move(Property));
		DMibCheck(_PropertyKey.f_GetType() != EPropertyType_Type);

		if (_PropertyKey.m_Name == gc_ConstString_FullEval.m_String)
			EntityData.m_HasFullEval |= 1 << _PropertyKey.f_GetType();
	}

	void CBuildSystem::fp_ParseProperty
		(
			CEntity &o_Entity
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

		if (!_Identifier.f_IsPropertyTypeConstant())
			fsp_ThrowError(_Registry, "You cannot use dynamic property type for a property expression");

		if (!_Identifier.m_bEmptyPropertyType)
		{
			if (_bDefines)
				fp_ParsePropertyValueDefines(_Identifier.f_PropertyKeyReferenceConstant(), o_Entity, _Registry, _pConditions, {});
			else
				fp_ParsePropertyValue(_Identifier.f_PropertyKeyReferenceConstant(), o_Entity, _Registry, nullptr);
			return;
		}

		if (_Registry.f_GetThisValue().m_Value.f_IsValid())
			fsp_ThrowError(_Registry, "Property groups cannot have a value");

		auto &Identifier = _Identifier.f_NameConstantString();

		EPropertyType Type = fg_PropertyTypeFromStr(Identifier);
		if (Identifier.f_IsEmpty() || Type == EPropertyType_Invalid)
			fsp_ThrowError(_Registry, "Unrecognized property type '{}'"_f << _Identifier.m_Name);

		TCVector<CStr> Namespaces;
		CStr NamespacesString;

		auto fParseRecursiveNamespaces = [&]
			(
#ifndef DCompiler_Workaround_Apple_clang
				this
#endif
				auto &&_fThis
				, CBuildSystemRegistry &_Registry
				, CCondition const *_pConditions
			) -> void
			{
#ifdef DCompiler_Workaround_Apple_clang
#define _fThis(...) _fThis(_fThis, __VA_ARGS__)
#endif
				CCondition Conditions;
				TCSharedPointer<CCondition> pConditions;

				bool bWasCondition = true;
				auto fGetConditions = [&]
					{
						if (!pConditions && !Conditions.f_IsDefault())
						{
							if (!_pConditions || _pConditions->f_IsDefault())
								pConditions = fg_Construct(fg_Move(Conditions));
							else
							{
								pConditions = fg_Construct();
								pConditions->m_Children.f_Insert(_pConditions->m_Children);
								pConditions->m_Children.f_Insert(fg_Move(Conditions));
							}
						}

						return pConditions;
					}
				;

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

					if (Registry.f_GetName().f_IsNamespace())
					{
						if (!_bDefines)
							continue;

						auto &Value = Registry.f_GetThisValue();
						if (!Value.m_Value.f_IsConstantString())
							fsp_ThrowError(Registry, "Namespaces need to be constant strings: {}"_f << Value.m_Value);

						CStr Namespace = Value.m_Value.f_ConstantString();
						Namespaces.f_Insert(Namespace);
						NamespacesString = CStr::fs_Join(Namespaces, "::");

						auto pConditions = fGetConditions();
						_fThis(Registry, pConditions.f_Get());

						Namespaces.f_Remove(Namespaces.f_GetLen() - 1);
						NamespacesString = CStr::fs_Join(Namespaces, "::");

						continue;
					}

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
									fsp_ThrowError(Registry, "You cannot specify an property type for properties in a property group");

								if (!_Identifier.f_IsNameConstantString())
									fsp_ThrowError(Registry, "You cannot use dynamic naming for a property expression");

								if (_bDefines)
									fp_ParsePropertyValueDefines(_Identifier.f_PropertyKeyReferenceConstant(Type), o_Entity, Registry, fGetConditions(), NamespacesString);
								else
									fp_ParsePropertyValue(_Identifier.f_PropertyKeyReferenceConstant(Type), o_Entity, Registry, fGetConditions());
							}
							, "Only conditions and properties are supported here"
							, EHandleKeyFlag_AllowPropertyType
						)
					;
				}
			}
		;
#ifdef DCompiler_Workaround_Apple_clang
#define fParseRecursiveNamespaces(...) fParseRecursiveNamespaces(fParseRecursiveNamespaces, __VA_ARGS__)
#endif

		CCondition Conditions;
		fParseRecursiveNamespaces(_Registry, &Conditions);
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

		DMibCheck(pEntity);

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

							pEntity->f_DataWritable().m_DebugFlags = fsp_ParseDebugFlags(Registry.f_GetLocation(), Value.m_Value.f_ConstantString());
						}
						else
							fsp_ThrowError(Registry, "Unexpected prefix operator: {}"_f << _PrefixOperator.m_Operator);
					}
					, [&](CBuildSystemSyntax::CIdentifier const &_Identifier)
					{
						fp_ParseProperty(*pEntity, _Identifier, Registry, fGetConditions(), true);
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
						fp_ParseProperty(*pEntity, _Identifier, Registry, fGetConditions(), false);
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
