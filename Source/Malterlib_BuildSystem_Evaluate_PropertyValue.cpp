// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include <Mib/Cryptography/UUID>
#include <Mib/Process/ProcessLaunch>
#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsRegistry>
#endif

namespace NMib::NBuildSystem
{
	CUniversallyUniqueIdentifier g_GeneratorFunctionUUIDHashUUIDNamespace("{010669A0-1AEC-48C9-878A-CFC5FFD996C6}");

	CStr CBuildSystem::fp_EvaluatePropertyValue
		(
			CEntity const &_Context
			, CEntity const &_OriginalContext
			, CStr const &_Value
			, CFilePosition const &_Position
			, CEvaluationContext &_EvalContext
		) const
	{
		CStr Value = _Value;

		ch8 const *pParse = Value;

		CStr Return;
		auto pCopyStart = pParse;

		while (*pParse)
		{
			auto Char = *pParse;
			if ((Char == '@') && pParse[1] == '(')
			{
				Return.f_AddStr(pCopyStart, pParse - pCopyStart);
				pParse += 2;
				auto pStartEval = pParse;
				mint nParen = 1;
				CStr ToEval;
				
				bool bHasRecursive = false;

				while (*pParse)
				{
					if (*pParse == '\'')
					{
						++pParse;
						while (*pParse)
						{
							if (*pParse == '\\')
							{
								++pParse;
								if (*pParse)
									++pParse;
							}
							else if (*pParse == '\'')
							{
								++pParse;
								break;
							}
							else
								++pParse;
						}
						continue;
					}
					if (pParse[0] == '@' && pParse[1] == '(')
					{
						bHasRecursive = true;
					}
					if (*pParse == '(')
						++nParen;
					else if (*pParse == ')')
					{
						if (--nParen == 0)
						{
							ToEval.f_AddStr(pStartEval, pParse - pStartEval);
							++pParse;
							break;
						}
					}
					++pParse;
				}

				if (nParen != 0)
					fsp_ThrowError(_Position, "Parenthesis mismatch");
				
				if (bHasRecursive)
					ToEval = fp_EvaluatePropertyValue(_Context, _OriginalContext, ToEval, _Position, _EvalContext);

				CStr Value = fp_GetPropertyValue(_Context, _OriginalContext, ToEval, _Position, _EvalContext);
				Return += Value;

				pCopyStart = pParse;
				continue;
			}
			else if ((Char == '@') && pParse[1] == '@')
			{
				++pParse;
				Return.f_AddStr(pCopyStart, pParse - pCopyStart);
				++pParse;
				pCopyStart = pParse;
				continue;
			}
			else
				++pParse;
		}

		Return.f_AddStr(pCopyStart, pParse - pCopyStart);
		
		return Return;
	}
	
	namespace
	{
		struct CExecuteCommandState
		{
			struct CFileState
			{
				CTime m_WriteTime;
				template <typename tf_CStream>
				void f_Stream(tf_CStream &_Stream)
				{
					_Stream % m_WriteTime;
				}
			};
			
			TCMap<CStr, CFileState> m_States;
			TCVector<CStr> m_Parameters;
			
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream)
			{
				uint32 Version = 0x101;
				_Stream % Version;
				if (Version < 0x101)
					DMibError("Invalid CExecuteCommandState version");
				_Stream % m_States;
				_Stream % m_Parameters;
			}
			
			void f_AddFile(CStr const &_FileName)
			{
				auto &State = m_States[_FileName];
				State.m_WriteTime = CFile::fs_GetWriteTime(_FileName);
			}
		};
	}
	
	CStr CBuildSystem::fp_GetPropertyValue
		(
			CEntity const &_Context
			, CEntity const &_OriginalContext
			, CStr const &_Value
			, CFilePosition const &_Position
			, CEvaluationContext &_EvalContext
		) const
	{
		CStr Type;
		CStr Property;
		CStr Function;
		CStr EntityContext;
		TCVector<CStr> FunctionParams;
					
		{
			ch8 const *pParse = _Value;
			ch8 const *pParseStart = pParse;

			bool bFunction = false;
			while (*pParse)
			{
				ch8 Char = *pParse;
				if (fg_CharIsAlphabetical(Char) || fg_CharIsNumber(Char) || Char == '\'')
				{
					auto pStart = pParse;
					if (*pParse == '\'')
					{
						++pParse;
						while (*pParse)
						{
							if (*pParse == '\\')
							{
								++pParse;
								if (*pParse)
									++pParse;
							}
							else if (*pParse == '\'')
							{
								++pParse;
								break;
							}
							else if (*pParse == '\r' || *pParse == '\n')
							{
								fsp_ThrowError(_Position, "New line in string constant");
							}
							else
								++pParse;
						}
					}
					else
					{
						fg_ParseAlphaNumericAndChars(pParse, "_");

						if (*pParse == '(' && Type.f_IsEmpty() && Property.f_IsEmpty())
						{
							Type = "Builtin";
							Property = "Function";
							bFunction = true;
						}
					}
					
					if (*pParse == ':')
					{
						EntityContext = _Value.f_Extract(pStart - pParseStart, pParse - pStart);
						++pParse;
					}
					else if (*pParse == '.')
					{
						Type = _Value.f_Extract(pStart - pParseStart, pParse - pStart);
						++pParse;
					}
					else if (*pParse == '-' && pParse[1] == '>')
					{
						Property = _Value.f_Extract(pStart - pParseStart, pParse - pStart);
						pParse += 2;
						bFunction = true;
					}
					else if (bFunction)
					{
						Function = _Value.f_Extract(pStart - pParseStart, pParse - pStart);
						if (*pParse != '(')
							fsp_ThrowError(_Position, "Syntax error in property evaluation (Should be function paren start)");

						++pParse;
						auto pStartParams = pParse;
						mint nParen = 1;
						while (*pParse)
						{
							if (*pParse == '\'')
							{
								++pParse;
								while (*pParse)
								{
									if (*pParse == '\\')
									{
										++pParse;
										if (*pParse)
											++pParse;
									}
									else if (*pParse == '\'')
									{
										++pParse;
										break;
									}
									else if (*pParse == '\r' || *pParse == '\n')
									{
										fsp_ThrowError(_Position, "New line in string constant");
									}
									else
										++pParse;
								}
								continue;
							}
							if (*pParse == '(')
								++nParen;
							else if (*pParse == ')')
							{
								if (--nParen == 0)
								{
									CStr Params(pStartParams, pParse - pStartParams);
									while (!Params.f_IsEmpty())
									{
										FunctionParams.f_Insert() 
											= fp_EvaluatePropertyValue
											(
												_Context
												, _OriginalContext
												, fg_GetStrSepEscaped<'\''>(Params, ",")
												, _Position
												, _EvalContext
											)	
										;
									}
									++pParse;
									break;
								}
							}
							++pParse;
						}

						DMibCheck(nParen == 0);
					}
					else if (*pParse)
					{
						fsp_ThrowError(_Position, "Syntax error in property evaluation (No more characters expected)");
					}
					else if (Property.f_IsEmpty())
						Property = _Value.f_Extract(pStart - pParseStart, pParse - pStart);
				}
				else
					fsp_ThrowError(_Position, fg_Format("Syntax error in property evaluation (Expected alpha character): {}", _Value));
			}
		}

		CStr Ret;

		auto pOriginalContext = &_OriginalContext;
		if (!EntityContext.f_IsEmpty())
		{
			EEntityType EntityType = fg_EntityTypeFromStr(EntityContext);
			if (EntityType == EEntityType_Invalid)
				fsp_ThrowError(_Position, CStr::CFormat("Invalid entity type '{}'") << EntityContext);
			
			while (pOriginalContext && pOriginalContext->m_Key.m_Type != EntityType)
				pOriginalContext = pOriginalContext->m_pParent;

			if (!pOriginalContext)
				fsp_ThrowError(_OriginalContext, _Position, CStr::CFormat("No entity with type '{}' found in parent entities for path: {}") << EntityContext << _OriginalContext.f_GetPath());
		}

		if (Type == "Builtin")
		{
			if (Property == "Function")
			{
				if (Function == "Error")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "Error takes one parameter");
					fsp_ThrowError(_Position, FunctionParams[0]);
				}
				else if (Function == "RelativeBase")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "RelativeBase takes one parameter");
					CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(FunctionParams[0], CFile::fs_GetPath(_Position.m_FileName));
					Ret = CFile::fs_MakePathRelative(WholePath, mp_BaseDir);
				}
				else if (Function == "SetProperty")
				{
					if (FunctionParams.f_GetLen() != 4)
						fsp_ThrowError(_Position, "SetProperty takes four parameter");

					CStr EntityTypeStr = FunctionParams[0];
					CStr PropertyTypeStr = FunctionParams[1];
					CPropertyKey PropertyKey;
					PropertyKey.m_Name = FunctionParams[2];
					CStr Value = FunctionParams[3];

					EEntityType EntityType = fg_EntityTypeFromStr(EntityTypeStr);
					if (EntityType == EEntityType_Invalid)
						fsp_ThrowError(_Position, CStr::CFormat("Invalid entity type '{}'") << EntityTypeStr);
					PropertyKey.m_Type = fg_PropertyTypeFromStr(PropertyTypeStr);
					if (PropertyKey.m_Type == EPropertyType_Invalid)
						fsp_ThrowError(_Position, CStr::CFormat("Invalid property type '{}'") << PropertyTypeStr);

					CEntity const *pEntity = pOriginalContext;
					while (pEntity && pEntity->m_Key.m_Type != EntityType)
						pEntity = pEntity->m_pParent;

					if (!pEntity)
						fsp_ThrowError(*pOriginalContext, _Position, CStr::CFormat("No entity with type '{}' found in parent entities") << EntityTypeStr);

					DMibLock(pEntity->m_Lock);
					auto &EvalProp = pEntity->m_EvaluatedProperties[PropertyKey];
					//EvalProp.m_Type =  EEvaluatedPropertyType_External;
					EvalProp.m_Value = Value;
				}
				else if (Function == "GetProperty" || Function == "HasProperty" || Function == "HasEntity")
				{
					CPropertyKey PropertyKey;
					CStr EntityName;
					if (Function == "HasEntity")
					{
						if (FunctionParams.f_GetLen() != 1)
							fsp_ThrowError(_Position, "HasEntity takes one parameter");
						EntityName = FunctionParams[0];
					}
					else 
					{
						if (FunctionParams.f_GetLen() != 3)
							fsp_ThrowError(_Position, fg_Format("{} takes three parameters", Function));
						EntityName = FunctionParams[2];
						PropertyKey.m_Type = fg_PropertyTypeFromStr(FunctionParams[0]);
						if (PropertyKey.m_Type == EPropertyType_Invalid)
							fsp_ThrowError(_Position, CStr::CFormat("Invalid property type '{}'") << FunctionParams[0]);
						PropertyKey.m_Name = FunctionParams[1];
					}
					
					auto pEntity = &_OriginalContext;
					for (int i = 0; i < 2; ++i)
					{
						if (i == 1 && !_OriginalContext.m_pCopiedFromEvaluated)
							break;
						CStr Entity = EntityName;
						pEntity = nullptr;
						while (!Entity.f_IsEmpty())
						{
							CStr SubEntity = fg_GetStrSepEscaped(Entity, ".");
							CStr Type = fg_GetStrSep(SubEntity, ":");

							if (Type == "Parent")
							{
								if (!pEntity)
								{
									if (i == 0)
										pEntity = &_OriginalContext;
									else
										pEntity = _OriginalContext.m_pCopiedFromEvaluated.f_Get();
								}
								pEntity = pEntity->m_pParent;
								if (!pEntity)
									fsp_ThrowError(_Position, "No parent found");
								continue;
							}

							CEntityKey EntityKey;
							EntityKey.m_Type = fg_EntityTypeFromStr(Type);
							if (EntityKey.m_Type == EEntityType_Invalid)
								fsp_ThrowError(_Position, CStr::CFormat("Invalid entity type '{}'") << Type);
							
							if (SubEntity == "*")
							{
								if (!pEntity)
								{
									if (i == 0)
										pEntity = &_OriginalContext;
									else
										pEntity = _OriginalContext.m_pCopiedFromEvaluated.f_Get();
								}
								while (pEntity && pEntity->m_Key.m_Type != EntityKey.m_Type)
									pEntity = pEntity->m_pParent;
								
								if (!pEntity)
									fsp_ThrowError(_Position, fg_Format("No parent with type '{}' found", Type));
								continue;
							}
							
							EntityKey.m_Name = SubEntity;
						
							if (!pEntity)
							{
								if (i == 0)
									pEntity = &_OriginalContext;
								else
									pEntity = _OriginalContext.m_pCopiedFromEvaluated.f_Get();
									
								
								auto pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
								while (!pChild && pEntity->m_pParent)
								{
									pEntity = pEntity->m_pParent;
									pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
								}
								if (!pChild)
								{
									pEntity = nullptr;
									break;
								}
								pEntity = pChild;
								continue;
							}

							auto pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
							if (!pChild)
							{
								pEntity = nullptr;
								break;
							}
							pEntity = pChild;
						}
						if (pEntity)
							break;
					}

					if (Function == "HasEntity")
					{ 
						if (pEntity)
							Ret = "true";
						else
							Ret = "false";
					}
					else
					{
						if (!pEntity)
						{
							if (Function == "GetProperty")
								fsp_ThrowError(_OriginalContext, _Position, CStr::CFormat("Entity '{}' not found") << EntityName);
							else
								Ret = "false";
						}
						else
						{
							TCMap<CPropertyKey, CEvaluatedProperty> TempProperties;
							{
								CChangePropertiesScope ChangeProperties(_EvalContext, &TempProperties);
								CProperty const *pFromProperty = nullptr;
								Ret = fp_EvaluateEntityProperty(*pEntity, *pEntity, PropertyKey, _EvalContext, pFromProperty);
								
								if (Function == "HasProperty")
								{
									if (pFromProperty)
										Ret = "true";
									else
										Ret = "false";
								}
							}
						}
					}
				}
				else if (Function == "GeneratedFiles")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "GeneratedFiles takes one parameter");

					CStr WildCard = FunctionParams[0];

					{
						DMibLock(mp_GeneratedFilesLock);

						for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
						{
							if (iFile->m_bGeneral)
							{
								if (NStr::fg_StrMatchWildcard(iFile.f_GetKey().f_GetStr(), WildCard.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
								{
									fg_AddStrSep(Ret, iFile.f_GetKey(), ";");
								}
							}
						}
					}
				}
				else if (Function == "ReadFile")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "ReadFile takes one parameter");

					Ret = CFile::fs_ReadStringFromFile(FunctionParams[0], true);

					f_AddSourceFile(FunctionParams[0]);
				}
				else if (Function == "SourceFiles")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "SourceFiles takes one parameter");

					CStr WildCard = FunctionParams[0];

					DMibLockRead(mp_SourceFilesLock);

					for (auto iFile = mp_SourceFiles.f_GetIterator(); iFile; ++iFile)
					{
						if (NStr::fg_StrMatchWildcard(iFile->f_GetStr(), WildCard.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						{
							fg_AddStrSep(Ret, *iFile, ";");
						}
					}
				}
				else if (Function == "HasParentEntity")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "HasParentEntity takes one parameter");
					EEntityType EntityType = fg_EntityTypeFromStr(FunctionParams[0]);
					if (EntityType == EEntityType_Invalid)
						fsp_ThrowError(_Position, CStr::CFormat("Invalid entity type '{}'") << FunctionParams[0]);
					auto pOriginalContext = &_OriginalContext;
					while (pOriginalContext && pOriginalContext->m_Key.m_Type != EntityType)
						pOriginalContext = pOriginalContext->m_pParent;

					if (pOriginalContext)
						Ret = "true";
					else
						Ret = "false";
				}
				else if (Function == "MalterlibTime")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "MalterlibTime takes one parameter");
					NTime::CTime Time = mp_NowUTC;
					Ret = CStr::CFormat(FunctionParams[0]) << Time.f_GetSeconds() << Time.f_GetFractionInt();
				}
				else if (Function == "DateTime")
				{
					if (FunctionParams.f_GetLen() != 1)
						fsp_ThrowError(_Position, "DateTime takes one parameter");

					CStr Format = FunctionParams[0];

					Format = Format.f_Replace("{YYYY}", "{0}");
					Format = Format.f_Replace("{YY}", "{0,sl2,sf0}");
					Format = Format.f_Replace("{M}", "{1}");
					Format = Format.f_Replace("{MM}", "{1,sl2,sf0}");
					Format = Format.f_Replace("{D}", "{2}");
					Format = Format.f_Replace("{DD}", "{2,sl2,sf0}");
					Format = Format.f_Replace("{H}", "{3}");
					Format = Format.f_Replace("{HH}", "{3,sl2,sf0}");
					Format = Format.f_Replace("{MN}", "{4}");
					Format = Format.f_Replace("{MNMN}", "{4,sl2,sf0}");
					Format = Format.f_Replace("{S}", "{5}");
					Format = Format.f_Replace("{SS}", "{5,sl2,sf0}");
					Format = Format.f_Replace("{F}", "{6}");
					Format = Format.f_Replace("{WD}", "{7}");

					static const char* cs_lWeekDays[] = {
							""
						,	"Monday"
						,	"Tuesday"
						,	"Wednesday"
						,	"Thursday"
						,	"Friday"
						,	"Saturday"
						,	"Sunday"
					};

					NTime::CTime Time = mp_Now;
					NTime::CTimeConvert::CDateTime DateTime;
					NTime::CTimeConvert(Time).f_ExtractDateTime(DateTime);

					Ret = CStr::CFormat(Format) 
							<< DateTime.m_Year 
							<< DateTime.m_Month
							<< DateTime.m_DayOfMonth
							<< DateTime.m_Hour
							<< DateTime.m_Minute
							<< DateTime.m_Second
							<< DateTime.m_Fraction
							<< cs_lWeekDays[DateTime.m_DayOfWeek];
				}
				else if (Function == "ExpandList")
				{
					if (FunctionParams.f_GetLen() != 3)
						fsp_ThrowError(_Position, "ExpandList takes three parameters: ExpandList(<Source>, <Separator>, <Template>)");

					CStr Source = FunctionParams[0];
					CStr Separator = FunctionParams[1];
					CStr Template = FunctionParams[2];

					TCMap<CPropertyKey, CEvaluatedProperty> TempProperties;

					int iExpand = 0;

					while (!Source.f_IsEmpty())
					{
						CPropertyKey Key;
						Key.m_Name = CStr::CFormat("Expand{}") << iExpand;
						Key.m_Type = EPropertyType_Property;

						auto& Expand = TempProperties[Key];

						Expand.m_Value = fg_GetStrSep(Source, Separator);
						Expand.m_Type = EEvaluatedPropertyType_External;
						Expand.m_pProperty = &mp_ExternalProperty[Key.m_Type];

						++iExpand;
					}

					CChangePropertiesScope ChangeProperties(_EvalContext, &TempProperties);

					CPropertyKey TemplateKey;
					TemplateKey.m_Name = Template;
					TemplateKey.m_Type = EPropertyType_Property;

					CProperty const* pProperty;

					Ret = fp_EvaluateEntityProperty
							(
								_Context
								, _OriginalContext
								, TemplateKey
								, _EvalContext
								, pProperty
							);
				}
				else if (Function == "ExplodeList")
				{
					if (FunctionParams.f_GetLen() != 3)
						fsp_ThrowError(_Position, "ExplodeList takes three parameters: ExplodeList(<Source>, <Separator>, <Template>)");

					CStr Source = FunctionParams[0];
					CStr Separator = FunctionParams[1];
					CStr Template = FunctionParams[2];
					//Template = fp_GetPropertyValue(_Context, _OriginalContext, Template, _Position, _EvalContext);
					
					auto &ExplodeStackEntry = _EvalContext.m_ExplodeListStack.f_InsertFirst();
					
					auto CleanUp 
						= g_OnScopeExit > [&]()
						{
							_EvalContext.m_ExplodeListStack.f_Remove(ExplodeStackEntry);
						}
					;

					while (!Source.f_IsEmpty())
					{
						TCMap<CPropertyKey, CEvaluatedProperty> TempProperties;
						
						CPropertyKey Key;
						Key.m_Type = EPropertyType_Property;

						CStr Value =  fg_GetStrSep(Source, Separator);
						ExplodeStackEntry.m_Value = Value;
						ExplodeStackEntry.m_ExplodedValue = Ret;

						{
							Key.m_Name = "Explodee";
							auto &Property = TempProperties[Key];

							Property.m_Value = Value;
							Property.m_Type = EEvaluatedPropertyType_External;
							Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
						}

						{
							mint i = 0;
							for (auto iEntry = _EvalContext.m_ExplodeListStack.f_GetIterator(); iEntry; ++iEntry, ++i)
							{
								{
									Key.m_Name = fg_Format("Explodee{}", i);
									auto &Property = TempProperties[Key];

									Property.m_Value = iEntry->m_Value;
									Property.m_Type = EEvaluatedPropertyType_External;
									Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
								}
							}
						}
						
						CChangePropertiesScope ChangeProperties(_EvalContext, &TempProperties);

						CPropertyKey TemplateKey;
						TemplateKey.m_Name = Template;
						TemplateKey.m_Type = EPropertyType_Property;

						CProperty const* pProperty;

						Ret += fp_EvaluateEntityProperty
							(
								_Context
								, _OriginalContext
								, TemplateKey
								, _EvalContext
								, pProperty
							)
						;
					}
				}
				else if (Function == "Explode")
				{
					if (FunctionParams.f_GetLen() != 3)
						fsp_ThrowError(_Position, "Explode takes three parameters: Explode(<Source>, <Separator>, <Template>)");

					CStr Source = FunctionParams[0];
					CStr Separator = FunctionParams[1];
					CStr Template = FunctionParams[2];
					//Template = fp_GetPropertyValue(_Context, _OriginalContext, Template, _Position, _EvalContext);

					auto &ExplodeStackEntry = _EvalContext.m_ExplodeListStack.f_InsertFirst();

					auto CleanUp
						= g_OnScopeExit > [&]()
						{
							_EvalContext.m_ExplodeListStack.f_Remove(ExplodeStackEntry);
						}
					;

					while (!Source.f_IsEmpty())
					{
						TCMap<CPropertyKey, CEvaluatedProperty> TempProperties;

						CPropertyKey Key;
						Key.m_Type = EPropertyType_Property;

						CStr Value =  fg_GetStrSep(Source, Separator);
						ExplodeStackEntry.m_Value = Value;
						ExplodeStackEntry.m_ExplodedValue = Ret;

						{
							Key.m_Name = "Explodee";
							auto &Property = TempProperties[Key];

							Property.m_Value = Value;
							Property.m_Type = EEvaluatedPropertyType_External;
							Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
						}
						{
							Key.m_Name = "ExplodedValue";
							auto &Property = TempProperties[Key];

							Property.m_Value = Ret;
							Property.m_Type = EEvaluatedPropertyType_External;
							Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
						}

						{
							mint i = 0;
							for (auto iEntry = _EvalContext.m_ExplodeListStack.f_GetIterator(); iEntry; ++iEntry, ++i)
							{
								{
									Key.m_Name = fg_Format("Explodee{}", i);
									auto &Property = TempProperties[Key];

									Property.m_Value = iEntry->m_Value;
									Property.m_Type = EEvaluatedPropertyType_External;
									Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
								}
								{
									Key.m_Name = fg_Format("ExplodedValue{}", i);
									auto &Property = TempProperties[Key];

									Property.m_Value = iEntry->m_ExplodedValue;
									Property.m_Type = EEvaluatedPropertyType_External;
									Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
								}
							}
						}

						CChangePropertiesScope ChangeProperties(_EvalContext, &TempProperties);

						CPropertyKey TemplateKey;
						TemplateKey.m_Name = Template;
						TemplateKey.m_Type = EPropertyType_Property;

						CProperty const* pProperty;

						Ret = fp_EvaluateEntityProperty
							(
								_Context
								, _OriginalContext
								, TemplateKey
								, _EvalContext
								, pProperty
							)
						;
					}
				}
				else if (Function == "ReadWindowsRegistry")
				{
#ifdef DPlatformFamily_Windows
					if (FunctionParams.f_GetLen() != 3)
						fsp_ThrowError(_Position, "ReadWindowsRegistry takes three parameters: <Root> <Key> <ValueName>");

					using ERegRoot = NMib::NPlatform::CWin32_Registry::ERegRoot;
					ERegRoot RegRoot;
					CStr Root = FunctionParams[0];
					if (Root == "LocalMachine")
						RegRoot = ERegRoot::ERegRoot_LocalMachine;
					else if (Root == "CurrentUser")
						RegRoot = ERegRoot::ERegRoot_CurrentUser;
					else if (Root == "Classes")
						RegRoot = ERegRoot::ERegRoot_Classes;
					else if (Root == "Win64_LocalMachine")
						RegRoot = ERegRoot::ERegRoot_Win64_LocalMachine;
					else if (Root == "Win64_CurrentUser")
						RegRoot = ERegRoot::ERegRoot_Win64_CurrentUser;
					else if (Root == "Win64_Classes")
						RegRoot = ERegRoot::ERegRoot_Win64_Classes;

					NMib::NPlatform::CWin32_Registry Registry{RegRoot};
					if (Registry.f_ValueExists(FunctionParams[1], FunctionParams[2]))
						Ret = Registry.f_Read_Str(FunctionParams[1], FunctionParams[2]);
#else
					Ret = "";
#endif
				}
				else if (Function == "ExecuteCommand")
				{
					if (FunctionParams.f_GetLen() < 3)
						fsp_ThrowError(_Position, "ExecuteCommand takes at least three parameters: StateFile Inputs Executable [Params...]");
					auto Params = FunctionParams;
					Params.f_Remove(0, 3);
					CStr GenerateStateFile = FunctionParams[0];
					CStr InputsString = FunctionParams[1];
					CStr Executable = FunctionParams[2];
					
					TCVector<CStr> Inputs;
					
					while (!InputsString.f_IsEmpty())
						Inputs.f_Insert(fg_GetStrSep(InputsString, ";"));
					
					if (GenerateStateFile.f_IsEmpty())
						fsp_ThrowError(_Position, "You need to specify states file");
				
					if (CFile::fs_FileExists(GenerateStateFile))
					{
						try
						{
							CExecuteCommandState State;
							TCBinaryStreamFile<> Stream;
							Stream.f_Open(GenerateStateFile, EFileOpen_Read | EFileOpen_ShareAll);
							Stream >> State;
							
							bool bAllValid = true;
							for (auto &Input : Inputs)
							{
								auto *pState = State.m_States.f_FindEqual(Input);
								if (!pState)
								{
									bAllValid = false;
									break;
								}
								if (pState->m_WriteTime != CFile::fs_GetWriteTime(Input))
								{
									bAllValid = false;
									break;
								}
							}
							
							if (bAllValid && FunctionParams == State.m_Parameters)
							{
								for (auto &Input : Inputs)
									f_AddSourceFile(Input);

								Stream >> Ret;
								return Ret;
							}
						}
						catch (CException const &_Exception)
						{
							DConErrOut2("Failed to check ExecuteCommmand state: {}\n", _Exception);
						}
					}

					try
					{
						CProcessLaunchParams LaunchParams;
						LaunchParams.m_bShowLaunched = false;
						LaunchParams.m_bCreateNewProcessGroup = true;
						Ret = CProcessLaunch::fs_LaunchTool(Executable, Params, LaunchParams);
					}
					catch (CException const &_Exception)
					{
						fsp_ThrowError(_Position, fg_Format("ExecuteCommand({vs,vb}) failed: {}", FunctionParams, _Exception));
					}

					CExecuteCommandState State;
					for (auto &Input : Inputs)
					{
						f_AddSourceFile(Input);
						State.f_AddFile(Input);
					}
					
					State.m_Parameters = FunctionParams;

					CBinaryStreamMemory<> Stream;
					Stream << State;
					Stream << Ret;
					
					CFile::fs_CreateDirectory(CFile::fs_GetPath(GenerateStateFile));
					f_WriteFile(Stream.f_MoveVector(), GenerateStateFile);
				}
				else
					fsp_ThrowError(_Position, CStr::CFormat("Builtin function not found '{}'") << Function);

				Function.f_Clear();
			}
			else if (Property == "GeneratedFiles")
			{
				DMibLock(mp_GeneratedFilesLock);
				for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
				{
					if (iFile->m_bGeneral)
						fg_AddStrSep(Ret, iFile.f_GetKey(), ";");
				}
			}
			else if (Property == "SourceFiles")
			{
				DMibLockRead(mp_SourceFilesLock);
				for (auto iFile = mp_SourceFiles.f_GetIterator(); iFile; ++iFile)
					fg_AddStrSep(Ret, *iFile, ";");
			}
			else if (Property == "BuildSystemSourceAbsolute")
			{
				Ret = this->mp_FileLocation;
			}
			else if (Property == "BuildSystemSource")
			{
				Ret = this->mp_FileLocationFile;
			}
			else if (Property == "GeneratorStateFile")
			{
				Ret = mp_GeneratorStateFileName;
			}
			else if (Property == "BasePathAbsolute")
			{
				Ret = f_GetBaseDir();
			}
			else if (Property == "MToolExe")
			{
				Ret = CFile::fs_GetProgramDirectory() / "MTool";
				#ifdef DPlatformFamily_Windows
					Ret += ".exe";
				#endif
			}
			else if (Property == "MalterlibExe")
			{
				Ret = CFile::fs_GetProgramDirectory() / "mib";
				#ifdef DPlatformFamily_Windows
					Ret += ".exe";
				#endif
			}
			else if (!mp_GeneratorInterface->f_GetBuiltin(Property, Ret))
				fsp_ThrowError(_Position, CStr::CFormat("Unrecognized builtin '{}'") << Property);
		}
		else if (Type == "this")
		{
			if (Property == "Identity")
				Ret = pOriginalContext->m_Key.m_Name;
			else if (Property == "IdentityAsAbsolutePath")
				Ret = CFile::fs_GetExpandedPath(pOriginalContext->m_Key.m_Name, CFile::fs_GetPath(pOriginalContext->m_Position.m_FileName));
			else if (Property == "IdentityPath")
				Ret = pOriginalContext->m_Position.m_FileName;
			else if (Property == "Type")
				Ret = fg_EntityTypeToStr(pOriginalContext->m_Key.m_Type);
			else
				fsp_ThrowError(_Position, CStr::CFormat("Unrecognized entity accessor '{}'") << Property);
		}
		else if (Property.f_StartsWith("\'"))
		{
			Ret = fg_RemoveEscape<'\''>(Property);
		}
		else
		{			
			
			CPropertyKey Key;
			Key.m_Name = Property;
			Key.m_Type = fg_PropertyTypeFromStr(Type);
			if (Key.m_Type == EPropertyType_Invalid)
				fsp_ThrowError(_Position, CStr::CFormat("Unrecognized property '{}'") << Type);
			
			if (pOriginalContext == &_OriginalContext)
			{
				auto pValue = _EvalContext.m_pEvaluatedProperties->f_FindEqual(Key);
				if (!pValue)
				{
					CProperty const *pFromProperty = nullptr;
					Ret = fp_EvaluateEntityProperty(_OriginalContext, _OriginalContext, Key, _EvalContext, pFromProperty);
				}
				else
					Ret = pValue->m_Value;
			}
			else
			{
				CEvaluatedProperty * pValue;
				{
					DLockReadLocked(pOriginalContext->m_Lock);
					pValue = pOriginalContext->m_EvaluatedProperties.f_FindEqual(Key);
					if (pValue)
						Ret = pValue->m_Value;
				}
				if (!pValue)
				{
					DMibLock(pOriginalContext->m_Lock);
					pValue = pOriginalContext->m_EvaluatedProperties.f_FindEqual(Key);
					if (!pValue)
					{
						CProperty const *pFromProperty = nullptr;
						{
							CChangePropertiesScope ChangeProperties(_EvalContext, &pOriginalContext->m_EvaluatedProperties);
							Ret = fp_EvaluateEntityProperty(*pOriginalContext, *pOriginalContext, Key, _EvalContext, pFromProperty);
						}
					}
					else
						Ret = pValue->m_Value;
				}
			}
		}

		if (!Function.f_IsEmpty())
		{
			if (Function == "GetLastPaths")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "GetLastPaths takes one parameter");
				int32 nPaths = FunctionParams[0].f_ToInt();
				CStr Value;
				while (nPaths--)
				{
					if (Value.f_IsEmpty())
						Value = CFile::fs_GetFile(Ret);
					else
						Value = CFile::fs_AppendPath(CFile::fs_GetFile(Ret), Value);
					Ret = CFile::fs_GetPath(Ret);
				}
				Ret = Value;
			}
			else if (Function == "RemoveStartPaths")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "RemoveStartPaths takes one parameter");
				int32 nPaths = FunctionParams[0].f_ToInt();
				CStr Value = CFile::fs_GetMalterlibPath(Ret);
				while (nPaths--)
				{
					fg_GetStrSep(Value, "/");
				}
				Ret = Value;
			}
			else if (Function == "GetPath")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "GetPath takes zero parameters");

				Ret = CFile::fs_GetPath(Ret);
			}
			else if (Function == "GetFile")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "GetFile takes zero parameters");

				Ret = CFile::fs_GetFile(Ret);
			}
			else if (Function == "GetFileNoExt")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "GetFileNoExt takes zero parameters");

				Ret = CFile::fs_GetFileNoExt(Ret);
			}
			else if (Function == "GetDrive")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "GetDrive takes zero parameters");

				Ret = CFile::fs_GetDrive(Ret);
			}
			else if (Function == "MakeRelative")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "MakeRelative takes one parameter");
				CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(Ret, CFile::fs_GetPath(_Position.m_FileName));
				CStr WholePathBase = mp_GeneratorInterface->f_GetExpandedPath(FunctionParams[0], CFile::fs_GetPath(_Position.m_FileName));
				Ret = CFile::fs_MakePathRelative(WholePath, WholePathBase);
			}
			else if (Function == "MakeAbsolute")
			{
				if (FunctionParams.f_GetLen() > 1)
					fsp_ThrowError(_Position, "MakeAbsolute takes one or zero parameters");
				
				if (FunctionParams.f_GetLen() == 1)
					Ret = mp_GeneratorInterface->f_GetExpandedPath(Ret, FunctionParams[0]);
				else
					Ret = mp_GeneratorInterface->f_GetExpandedPath(Ret, CFile::fs_GetPath(_Position.m_FileName));
			}
			else if (Function == "ContainsListElement")
			{
				if (FunctionParams.f_GetLen() != 2)
					fsp_ThrowError(_Position, "IsInList takes two parameters: Separator, Value");
				
				CStr const &List = Ret;
				CStr const &Separator = FunctionParams[0];
				CStr const &Value = FunctionParams[1];
				
				ch8 const *pParse = List;
				ch8 const *pParseEnd = List.f_GetStr() + List.f_GetLen();
				
				bool bFound = false;
				
				while (*pParse)
				{
					aint iSeparator = fg_StrFind(pParse, Separator);
					
					if (iSeparator < 0)
					{
						if (NStr::CStrPtr(pParse, pParseEnd - pParse) == Value)
							bFound = true;
						
						break;
					}
					else
					{
						if (NStr::CStrPtr(pParse, iSeparator) == Value)
						{
							bFound = true;
							break;
						}
					}
					
					pParse += iSeparator + 1;
				}
				
				if (bFound)
					Ret = "true";
				else
					Ret = "false";
			}
			else if (Function == "FindFilesIn")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FindIn takes a parameter for the pattern to look for");
				CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(Ret, CFile::fs_GetPath(_Position.m_FileName));
				WholePath = CFile::fs_AppendPath(WholePath, FunctionParams[0]);
				CFindOptions FindOptions(WholePath, EFileAttrib_File);
				
				auto Files = mp_FindCache.f_FindFiles(FindOptions, true);
				Ret = CStr();
				
				for (auto &File : Files)
				{
					fg_AddStrSep(Ret, File.m_Path, ";");
				}
			}
			else if (Function == "FindDirectoriesIn")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FindIn takes a parameter for the pattern to look for");
				CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(Ret, CFile::fs_GetPath(_Position.m_FileName));
				WholePath = CFile::fs_AppendPath(WholePath, FunctionParams[0]);
				CFindOptions FindOptions(WholePath, EFileAttrib_Directory);
				auto Files = mp_FindCache.f_FindFiles(FindOptions, true);
				Ret = CStr();
				
				for (auto &File : Files)
				{
					fg_AddStrSep(Ret, File.m_Path, ";");
				}
			}
			else if (Function == "FindFilesRecursiveIn")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FindIn takes a parameter for the pattern to look for");
				CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(Ret, CFile::fs_GetPath(_Position.m_FileName));
				WholePath = CFile::fs_AppendPath(WholePath, FunctionParams[0]);
				CFindOptions FindOptions(WholePath, EFileAttrib_File, true);
				auto Files = mp_FindCache.f_FindFiles(FindOptions, true);
				Ret = CStr();
				
				for (auto &File : Files)
				{
					fg_AddStrSep(Ret, File.m_Path, ";");
				}
			}
			else if (Function == "FindDirectoriesRecursiveIn")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FindIn takes a parameter for the pattern to look for");
				CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(Ret, CFile::fs_GetPath(_Position.m_FileName));
				WholePath = CFile::fs_AppendPath(WholePath, FunctionParams[0]);
				CFindOptions FindOptions(WholePath, EFileAttrib_Directory, true);
				auto Files = mp_FindCache.f_FindFiles(FindOptions, true);
				Ret = CStr();
				
				for (auto &File : Files)
				{
					fg_AddStrSep(Ret, File.m_Path, ";");
				}
			}
			else if (Function == "GetExtension")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "GetExtension takes zero parameters");

				Ret = CFile::fs_GetExtension(Ret);
			}
			else if (Function == "Replace")
			{
				if (FunctionParams.f_GetLen() != 2)
					fsp_ThrowError(_Position, "Replace takes two parameters");

				Ret = Ret.f_Replace(FunctionParams[0], FunctionParams[1]);
			}
			else if (Function == "ReplaceChars")
			{
				if (FunctionParams.f_GetLen() != 2)
					fsp_ThrowError(_Position, "ReplaceChars takes two parameters");

				CStr ToReplaceWith = FunctionParams[1];
				TCVector<ch8> Characters;
				Characters.f_Insert(FunctionParams[0].f_GetStr(), FunctionParams[0].f_GetLen());
				for (auto &Character : Characters)
				{
					ch8 ToFind[] = {Character, 0};
					Ret = Ret.f_Replace(ToFind, ToReplaceWith);
				}
			}
			else if (Function == "FindGetLine")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FindGetLine takes one parameters");
				
				ch8 const *pPattern = FunctionParams[0].f_GetStr();

				ch8 const *pParse = Ret.f_GetStr();
				while (*pParse)
				{
					auto pParseStart = pParse;
					fg_ParseToEndOfLine(pParse);
					
					CStr Line(pParseStart, pParse - pParseStart);
					
					if (fg_StrMatchWildcard(Line.f_GetStr(), pPattern) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
					{
						Ret = Line;
						break;
					}
					
					fg_ParseEndOfLine(pParse);
				}
			}
			else if (Function == "Find")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "Find takes one parameters");

				if (Ret.f_Find(FunctionParams[0]) >= 0)
					Ret = "true";
			}
			else if (Function == "FindNoCase")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "Find takes one parameters");

				if (Ret.f_FindNoCase(FunctionParams[0]) >= 0)
					Ret = "true";
			}
			else if (Function == "WindowsPath")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "WindowsPath takes zero parameters");

				Ret = Ret.f_ReplaceChar('/', '\\');
			}
			else if (Function == "UnixPath")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "UnixPath takes zero parameters");

				Ret = Ret.f_ReplaceChar('\\', '/');
			}
			else if (Function == "NativePath")
			{
				if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "NativePath takes zero parameters");

#ifdef DPlatformFamily_Windows
				Ret = Ret.f_ReplaceChar('/', '\\');
#else
				Ret = Ret.f_ReplaceChar('\\', '/');
#endif
			}
			else if (Function == "HashUUID")
			{
				EUniversallyUniqueIdentifierFormat UUIDFormat = EUniversallyUniqueIdentifierFormat_Registry;

				if (FunctionParams.f_GetLen() == 1)
				{
					if (FunctionParams[0].f_CmpNoCase("Registry") == 0)
						UUIDFormat = EUniversallyUniqueIdentifierFormat_Registry;
					else if (FunctionParams[0].f_CmpNoCase("Bare") == 0)
						UUIDFormat = EUniversallyUniqueIdentifierFormat_Bare;
					else if (FunctionParams[0].f_CmpNoCase("AlphaNum") == 0)
						UUIDFormat = EUniversallyUniqueIdentifierFormat_AlphaNum;
					else
						fsp_ThrowError(_Position, "HashUUID format arg can be \"Registry\", \"Bare\" or \"AlphaNum\"");
				}
				else if (FunctionParams.f_GetLen() != 0)
					fsp_ThrowError(_Position, "HashUUID takes one or zero parameters");

				Ret = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorFunctionUUIDHashUUIDNamespace, Ret).f_GetAsString(UUIDFormat);
			}
			else if (Function == "HashSHA256")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "HashSHA256 takes one parameter, the length of the string");

				mint HashLen = FunctionParams[0].f_ToInt(mint(64));

				NCryptography::CHash_SHA256 Hash;
				Hash.f_AddData(Ret.f_GetStr(), Ret.f_GetLen());

				Ret = Hash.f_GetDigest().f_GetString().f_Left(HashLen);
			}
			else if (Function == "FormatInt")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FormatInt takes one parameters: <Format>");

				int64 Value = Ret.f_ToInt((int64)0);
				Ret = CStr::CFormat(FunctionParams[0]) << Value;
			}
			else if (Function == "FormatFloat")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FormatFloat takes one parameters: <Format>");

				fp64 Value = Ret.f_ToFloat(fp64(0.0));
				Ret = CStr::CFormat(FunctionParams[0]) << Value;
			}
			else if (Function == "FormatString")
			{
				if (FunctionParams.f_GetLen() != 1)
					fsp_ThrowError(_Position, "FormatString takes one parameters: <Format>");

				CStr Value = Ret;

				Ret = CStr::CFormat(FunctionParams[0]) << Value;
			}
			else if (Function == "CompareInt")
			{
				if (FunctionParams.f_GetLen() != 2)
					fsp_ThrowError(_Position, "CompareInt takes two parameters: <Operator> <RightValue>");

				CStr Value = Ret;
				int64 Left = Ret.f_ToIntExact(TCLimitsInt<int64>::mc_Max);
				int64 Right = FunctionParams[1].f_ToIntExact(TCLimitsInt<int64>::mc_Max);
				CStr Operator = FunctionParams[0];
				if (Operator == "==")
					Ret = Left == Right ? "true" : "false";
				if (Operator == "!=")
					Ret = Left != Right ? "true" : "false";
				else if (Operator == "<")
					Ret = Left < Right ? "true" : "false";
				else if (Operator == ">")
					Ret = Left > Right ? "true" : "false";
				else if (Operator == "<=")
					Ret = Left <= Right ? "true" : "false";
				else if (Operator == ">=")
					Ret = Left >= Right ? "true" : "false";
				else
					fsp_ThrowError(_Position, "Unknown operator '{}'"_f << Operator);
			}
			else if (Function == "CompareFloat")
			{
				if (FunctionParams.f_GetLen() != 2)
					fsp_ThrowError(_Position, "CompareFloat takes two parameters: <Operator> <RightValue>");

				CStr Value = Ret;
				fp64 Left = Ret.f_ToFloatExact(fp64::fs_Inf());
				fp64 Right = FunctionParams[1].f_ToFloatExact(fp64::fs_Inf());
				CStr Operator = FunctionParams[0];
				if (Operator == "==")
					Ret = Left == Right ? "true" : "false";
				if (Operator == "!=")
					Ret = Left != Right ? "true" : "false";
				else if (Operator == "<")
					Ret = Left < Right ? "true" : "false";
				else if (Operator == ">")
					Ret = Left > Right ? "true" : "false";
				else if (Operator == "<=")
					Ret = Left <= Right ? "true" : "false";
				else if (Operator == ">=")
					Ret = Left >= Right ? "true" : "false";
				else
					fsp_ThrowError(_Position, "Unknown operator '{}'"_f << Operator);
			}
			else if (Function == "ParseFormatString")
			{
				if (FunctionParams.f_GetLen() != 2)
					fsp_ThrowError(_Position, "ParseFormatString takes two parameters: <ParseFormat> <OutputFormat>");

				CStr Params[32];
				auto Parse = CStr::CParse(FunctionParams[0]);

				for (auto i = 0; i < 32; ++i)
					Parse >> Params[i];

				Parse.f_Parse(Ret);

				auto Format = CStr::CFormat(FunctionParams[1]);

				for (auto i = 0; i < 32; ++i)
					Format << Params[i];

				Ret = Format;
			}
			else if (Function == "Sanitize")
			{

				if (FunctionParams.f_GetLen() > 1)
				{
					fsp_ThrowError(_Position, "Sanitize takes one optional parameter: <Format>");
				}

				CStr Format = "rfc1034";
				if (FunctionParams.f_GetLen() == 1)
					Format = FunctionParams[0];

				if (Format.f_CmpNoCase("rfc1034") == 0)
				{							
					CStr::CChar* pChar = Ret.f_GetStrUniqueWritable();
					CStr::CChar CurChar;

					while (*pChar)
					{
						CurChar = *pChar;

						if 
							(
								(CurChar >= 'a' && CurChar <= 'z')
								|| (CurChar >= 'A' && CurChar <= 'Z')
								|| (CurChar >= '0' && CurChar <= '9')
								|| (CurChar == '.' || CurChar == '-')
							)
						{

						}
						else
						{
							*pChar = '-';
						}

						++pChar;
					}
				}
				else if (Format.f_CmpNoCase("bash") == 0)
				{							
					CStr::CChar const* pChar = Ret.f_GetStr();
					CStr::CChar CurChar;

					CStr::CChar const* pStartRun = pChar;

					CStr Out;

					while (*pChar)
					{
						CurChar = *pChar;

						if 
							(
								CurChar == ' '
								|| CurChar == '"' 
								|| CurChar == '\''
							)
						{
							if (pStartRun < pChar)
								Out += CStr(pStartRun, pChar - pStartRun); // Ideally we would get rid of this alloc.
							Out += "\\";
							Out += CurChar;
							pStartRun = pChar + 1;
						}

						++pChar;
					}

					if (pStartRun < pChar)
						Out += CStr(pStartRun, pChar - pStartRun); // Ideally we would get rid of this alloc.

					Ret = Out;
				}
			}								
			else
				fsp_ThrowError(_Position, CStr::CFormat("Function not found '{}'") << Function);
		}

		//DDTrace("{} = {}" DNewLine, _Value << Ret);

		return Ret;
	}
}
