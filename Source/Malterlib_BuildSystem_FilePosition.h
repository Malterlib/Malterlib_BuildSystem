// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include "Malterlib_BuildSystem_Registry.h"

namespace NMib::NBuildSystem
{
	struct CFilePosition : public NStr::CParseLocation
	{
		inline CFilePosition(CBuildSystemRegistry const &_Position);
		inline CFilePosition(NStr::CParseLocation const &_Position);
		inline constexpr CFilePosition();
		inline CFilePosition &operator = (CBuildSystemRegistry const &_Position);
		inline CFilePosition &operator = (NStr::CParseLocation const &_Position);
		inline NStr::CParseLocation const &f_Location() const;

		static CFilePosition fs_FromJson(NEncoding::CEJsonSorted const &_Value);
		static CFilePosition fs_FromJson(NEncoding::CEJsonSorted &&_Value);

		COrdering_Strong operator <=> (CFilePosition const &_Other) const;

		static CFilePosition const &fs_Default();

		uint32 m_FileHash = 0;
	};

#define DMibBuildSystemFilePosition CFilePosition(NStr::CParseLocation{.m_File = CFile::fs_GetMalterlibPath((NStr::CStr const &)gc_Str<DMibPFile>), .m_Line = DMibPLine})

	struct CBuildSystemUniquePositions : public NStorage::CIntrusiveRefCount
	{
		struct CKey
		{
			auto operator <=> (CKey const &) const = default;

			CFilePosition m_Position;
			NStr::CStr m_Identifier;
		};

		struct CPosition
		{
			struct CCompare
			{
				inline_small CKey const &operator () (CPosition const &_Node) const
				{
					return _Node.m_Key;
				}
			};

			CPosition(CPosition const &_Right) = delete;
			CPosition();

			NStr::CStr f_GetMessage() const;
			void f_AddValue(NEncoding::CEJsonSorted const &_Value, bool _bEnabled);

			CKey m_Key;
			NStr::CStr m_Message;
			NIntrusive::TCAVLLink<> m_TreeLink;
		};

		CBuildSystemUniquePositions();
		explicit CBuildSystemUniquePositions(CFilePosition const &_Position, NStr::CStr const &_Message);
		CBuildSystemUniquePositions(CBuildSystemUniquePositions const &_Positions);
		~CBuildSystemUniquePositions();
		CBuildSystemUniquePositions &operator = (CBuildSystemUniquePositions const &_Positions);

		bool f_IsEmpty() const;

		CPosition *f_AddPosition(CFilePosition const &_Position, NStr::CStr const &_Message);
		CPosition *f_AddPosition(CFilePosition &&_Position, NStr::CStr &&_Message);

		CPosition *f_AddPosition(CFilePosition const &_Position, NStr::CStr const &_Identifier, NStr::CStr const &_Message);
		CPosition *f_AddPosition(CFilePosition &&_Position, NStr::CStr &&_Identifier, NStr::CStr &&_Message);
		CPosition *f_AddPositionFirst(CFilePosition const &_Position, NStr::CStr const &_Identifier, NStr::CStr const &_Message);
		void f_AddFirstFromPositions(CBuildSystemUniquePositions const &_Other);
		void f_AddPositions(CBuildSystemUniquePositions const &_Other);
		void f_AddPositions(CBuildSystemUniquePositions &&_Other);
		void f_AddPositions(NStorage::TCSharedPointer<CBuildSystemUniquePositions> const &_Other);

		static CBuildSystemUniquePositions fs_FromJson(NEncoding::CEJsonSorted const &_Value);
		static CBuildSystemUniquePositions fs_FromJson(NEncoding::CEJsonSorted &&_Value);

		NContainer::TCLinkedList<CPosition> m_Positions;
		NIntrusive::TCAVLTree<&CPosition::m_TreeLink, CPosition::CCompare> m_PositionTree;
		CBuildSystemUniquePositions *m_pParentPositions = nullptr;
	};

	struct CBuildSystemPropertyInfo;

	template <typename t_CType>
	struct TCValueWithPositions
	{
		template <typename tf_CValue>
		void f_SetFrom(tf_CValue &&_Value, NStr::CStr const &_Name, CBuildSystemPropertyInfo const &_PropertyInfo, t_CType &&_DefaultValue);
		template <typename tf_CValue>
		void f_SetFrom(tf_CValue &&_Value, NStr::CStr const &_Name, CBuildSystemPropertyInfo const &_PropertyInfo);

		t_CType m_Value = {};
		CBuildSystemUniquePositions m_Positions;
	};
}
