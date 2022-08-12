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
						gc_ConstString_ExecuteCommand
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Void
								, fg_FunctionParam(g_String, gc_ConstString__StateFile)
								, fg_FunctionParam(g_StringArray, gc_ConstString__Inputs)
								, fg_FunctionParam(g_String, gc_ConstString__Executable)
								, fg_FunctionParam(g_String, gc_ConstString_p_Params, g_Ellipsis)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								auto const &GenerateStateFile = _Params[0].f_String();

								if (GenerateStateFile.f_IsEmpty())
									fs_ThrowError(_Context, "You need to specify states file");

								auto const &Executable = _Params[2].f_String();

								TCVector<CStr> FunctionParams = _Params[3].f_StringArray();
								TCVector<CStr> Inputs = _Params[1].f_StringArray();

								CExecuteCommand *pExecuteCommand = nullptr;
								{
									DMibLock(_This.mp_ExecuteCommandsLock);
									pExecuteCommand = &_This.mp_ExecuteCommands[GenerateStateFile];
								}

								DMibLock(pExecuteCommand->m_Lock);

								if (pExecuteCommand->m_bInitialized)
								{
									if (Executable != pExecuteCommand->m_Executable)
										fs_ThrowError(_Context, "Executable differs from previous invocation: {} != {}"_f << Executable << pExecuteCommand->m_Executable);

									if (FunctionParams != pExecuteCommand->m_FunctionParams)
										fs_ThrowError(_Context, "Function params differs from previous invocation: {vs} != {vs}"_f << FunctionParams << pExecuteCommand->m_FunctionParams);

									if (Inputs != pExecuteCommand->m_Inputs)
										fs_ThrowError(_Context, "Inputs params differs from previous invocation: {vs} != {vs}"_f << Inputs << pExecuteCommand->m_Inputs);

									if (pExecuteCommand->m_Error)
										fs_ThrowError(_Context, pExecuteCommand->m_Error);

									return pExecuteCommand->m_Result;
								}

								pExecuteCommand->m_bInitialized = true;
								pExecuteCommand->m_Executable = Executable;
								pExecuteCommand->m_FunctionParams = FunctionParams;
								pExecuteCommand->m_Inputs = Inputs;

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

											pExecuteCommand->m_Result = Ret;

											return fg_Move(Ret);
										}
									}
									catch (CException const &_Exception)
									{
										_This.f_OutputConsole("Failed to check ExecuteCommmand state: {}\n"_f << _Exception, true);
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
									pExecuteCommand->m_Error = fg_Format("ExecuteCommand({vs,vb}) failed: {}", FunctionParams, _Exception);
									fs_ThrowError(_Context, pExecuteCommand->m_Error);
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

								pExecuteCommand->m_Result = Ret;

								return Ret;
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
