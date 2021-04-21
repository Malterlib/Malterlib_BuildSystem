// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Process/ProcessLaunch>

namespace NMib::NBuildSystem
{
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

	void CBuildSystem::fp_RegisterBuiltinFunctions_Execute()
	{
		f_RegisterFunctions
			(
				{
					{
						"ExecuteCommand"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Void
								, fg_FunctionParam(g_String, "_StateFile")
								, fg_FunctionParam(g_StringArray, "_Inputs")
								, fg_FunctionParam(g_String, "_Executable")
								, fg_FunctionParam(g_String, "p_Params", g_Ellipsis)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								auto const &GenerateStateFile = _Params[0].f_String();
								auto const &Executable = _Params[2].f_String();

								TCVector<CStr> FunctionParams = _Params[3].f_StringArray();
								TCVector<CStr> Inputs = _Params[1].f_StringArray();

								if (GenerateStateFile.f_IsEmpty())
									fsp_ThrowError(_Context, "You need to specify states file");

								CStr Ret;

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
												_This.f_AddSourceFile(Input);

											Stream >> Ret;
											return fg_Move(Ret);
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
									Ret = CProcessLaunch::fs_LaunchTool(Executable, FunctionParams, LaunchParams);
								}
								catch (CException const &_Exception)
								{
									fsp_ThrowError(_Context, fg_Format("ExecuteCommand({vs,vb}) failed: {}", FunctionParams, _Exception));
								}

								CExecuteCommandState State;
								for (auto &Input : Inputs)
								{
									_This.f_AddSourceFile(Input);
									State.f_AddFile(Input);
								}

								State.m_Parameters = FunctionParams;

								CBinaryStreamMemory<> Stream;
								Stream << State;
								Stream << Ret;

								CFile::fs_CreateDirectory(CFile::fs_GetPath(GenerateStateFile));
								_This.f_WriteFile(Stream.f_MoveVector(), GenerateStateFile);

								return Ret;
							}
						}
					}
				}
			)
		;
	}
}
