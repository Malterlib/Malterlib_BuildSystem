// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem::NNinja
{
	template <typename tf_CStream>
	void CRule::f_Stream(tf_CStream &o_Stream)
	{
		o_Stream % m_Command;
		o_Stream % m_Environment;
		o_Stream % m_OtherProperties;
	}

	template <typename tf_CStr>
	void CRule::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Command: {} Environment: {vs} Other: {vs}\n") << m_Command << m_Environment << m_OtherProperties;
	}

	template <typename tf_CStream>
	void CBuild::f_Stream(tf_CStream &o_Stream)
	{
		o_Stream % m_Description;
		o_Stream % m_Inputs;
		o_Stream % m_Outputs;
		o_Stream % m_ImplicitOutputs;
		o_Stream % m_ImplicitDependencies;
		o_Stream % m_OrderDependencies;
		o_Stream % m_Validations;
		o_Stream % m_Variables;
	}

	template <typename tf_CStr>
	void CBuild::f_Format(tf_CStr &o_Str) const
	{
		o_Str
			+= typename tf_CStr::CFormat("Description: {} Inputs: {} Outputs: {} ImplicitDependencies: {} OrderDependencies: {} Validations: {} Variables: {vs}\n")
			<< m_Description
			<< m_Inputs
			<< m_Outputs
			<< m_ImplicitDependencies
			<< m_OrderDependencies
			<< m_Validations
			<< m_Variables
		;
	}

	template <typename tf_CStr>
	void CBuildAndRuleEntry::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Rule: {}\nBuild: {}\nFlags: {}") << m_Rule << m_Build << m_Flags;
	}

	template <typename tf_CStr>
	void CRuleAndBuild::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Type: {}\nEntries: {}\n") << m_Type << m_Entries;
	}

	template <typename tf_CStr>
	void CConfigResultTarget::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("PreBuild: {} Build: {} PostBuild: {}\n") << m_PreBuild << m_Build << m_PostBuild;
	}

	template <typename tf_CStr>
	void CConfigResultCompile::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("Build: {}\n") << m_Build;
	}

	template <typename tf_CStr>
	void CGeneratorInstance::CCompileType::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{vs}") << m_EnabledConfigs;
	}
}
