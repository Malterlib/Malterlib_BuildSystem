// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	CFilePosition const &CFilePosition::fs_Default()
	{
		static constinit CFilePosition Default;

		return Default;
	}

	CFilePosition CFilePosition::fs_FromJson(CEJsonSorted const &_Value)
	{
		CFilePosition FilePosition;
		FilePosition.m_File = _Value[gc_ConstString_File].f_String();
		FilePosition.m_Line = _Value[gc_ConstString_Line].f_Integer();
		FilePosition.m_Column = _Value[gc_ConstString_Column].f_Integer();
		FilePosition.m_FileHash = NStr::fg_StrHash(FilePosition.m_File);

		return FilePosition;
	}

	CFilePosition CFilePosition::fs_FromJson(CEJsonSorted &&_Value)
	{
		CFilePosition FilePosition;
		FilePosition.m_File = fg_Move(_Value[gc_ConstString_File].f_String());
		FilePosition.m_Line = _Value[gc_ConstString_Line].f_Integer();
		FilePosition.m_Column = _Value[gc_ConstString_Column].f_Integer();
		FilePosition.m_FileHash = NStr::fg_StrHash(FilePosition.m_File);

		return FilePosition;
	}

	COrdering_Strong CFilePosition::operator <=> (CFilePosition const &_Other) const noexcept
	{
		if (auto Result = m_FileHash <=> _Other.m_FileHash; Result != 0)
			return Result;

		if (auto Result = m_Line <=> _Other.m_Line; Result != 0)
			return Result;

		if (auto Result = m_Column <=> _Other.m_Column; Result != 0)
			return Result;

		if (auto Result = m_Character <=> _Other.m_Character; Result != 0)
			return Result;

		return m_File <=> _Other.m_File;
	}

	CBuildSystemUniquePositions::CBuildSystemUniquePositions() = default;

	CBuildSystemUniquePositions &CBuildSystemUniquePositions::operator = (CBuildSystemUniquePositions const &_Positions)
	{
		m_PositionTree.f_Clear();
		m_Positions.f_Clear();

		f_AddPositions(_Positions);

		return *this;
	}

	CBuildSystemUniquePositions::CBuildSystemUniquePositions(CBuildSystemUniquePositions const &_Positions)
	{
		f_AddPositions(_Positions);
	}

	CBuildSystemUniquePositions::CBuildSystemUniquePositions(CFilePosition const &_Position, CStr const &_Message)
	{
		f_AddPosition(_Position, _Message);
	}

	CBuildSystemUniquePositions::~CBuildSystemUniquePositions()
	{
		m_PositionTree.f_Clear();
	}

	CBuildSystemUniquePositions::CPosition::CPosition() = default;

	NStr::CStr CBuildSystemUniquePositions::CPosition::f_GetMessage() const
	{
		return m_Message ? m_Message : gc_ConstString_Contributing_to_value;
	}

	void CBuildSystemUniquePositions::CPosition::f_AddValue(CEJsonSorted const &_Value, bool _bEnabled)
	{
		if (!_bEnabled)
			return;

		m_Message += "\n{}"_f << _Value.f_ToString("    ", EJsonDialectFlag_All).f_Trim().f_Indent("    ");
	}

	CBuildSystemUniquePositions CBuildSystemUniquePositions::fs_FromJson(CEJsonSorted const &_Value)
	{
		CBuildSystemUniquePositions Return;

		for (auto &Position : _Value.f_Array())
			Return.f_AddPosition(CFilePosition::fs_FromJson(Position), Position[gc_ConstString_Identifier].f_String(), Position[gc_ConstString_Message].f_String());

		return Return;
	}

	CBuildSystemUniquePositions CBuildSystemUniquePositions::fs_FromJson(CEJsonSorted &&_Value)
	{
		CBuildSystemUniquePositions Return;

		for (auto &Position : _Value.f_Array())
		{
			Return.f_AddPosition
				(
					CFilePosition::fs_FromJson(fg_Move(Position))
					, fg_Move(Position[gc_ConstString_Identifier].f_String())
					, fg_Move(Position[gc_ConstString_Message].f_String())
				)
			;
		}

		return Return;
	}

	bool CBuildSystemUniquePositions::f_IsEmpty() const
	{
		return m_Positions.f_IsEmpty();
	}

	auto CBuildSystemUniquePositions::f_AddPosition(CFilePosition const &_Position, NStr::CStr const &_Message) -> CPosition *
	{
		return f_AddPosition(_Position, _Message, _Message);
	}

	auto CBuildSystemUniquePositions::f_AddPosition(CFilePosition &&_Position, NStr::CStr &&_Message) -> CPosition *
	{
		auto Identifier = _Message;
		return f_AddPosition(fg_Move(_Position), fg_Move(Identifier), fg_Move(_Message));
	}

	CBuildSystemUniquePositions::CPosition *CBuildSystemUniquePositions::f_AddPosition(CFilePosition const &_Position, NStr::CStr const &_Identifier, CStr const &_Message)
	{
		DMibFastCheck(_Position.f_IsValid());

		CKey Key;
		Key.m_Position = _Position;
		Key.m_Identifier = _Identifier;

		if (auto *pPosition = m_PositionTree.f_FindEqual(Key))
			return pPosition;

		auto &NewNode = m_Positions.f_Insert();
		NewNode.m_Key = fg_Move(Key);
		NewNode.m_Message = _Message;

		m_PositionTree.f_Insert(NewNode);

		return &NewNode;
	}

	CBuildSystemUniquePositions::CPosition *CBuildSystemUniquePositions::f_AddPosition(CFilePosition &&_Position, NStr::CStr &&_Identifier, CStr &&_Message)
	{
		DMibFastCheck(_Position.f_IsValid());

		CKey Key;
		Key.m_Position = fg_Move(_Position);
		Key.m_Identifier = fg_Move(_Identifier);

		if (auto *pPosition = m_PositionTree.f_FindEqual(Key))
			return pPosition;

		auto &NewNode = m_Positions.f_Insert();
		NewNode.m_Key = fg_Move(Key);
		NewNode.m_Message = fg_Move(_Message);

		m_PositionTree.f_Insert(NewNode);

		return &NewNode;
	}

	CBuildSystemUniquePositions::CPosition *CBuildSystemUniquePositions::f_AddPositionFirst(CFilePosition const &_Position, NStr::CStr const &_Identifier, CStr const &_Message)
	{
		DMibFastCheck(_Position.f_IsValid());

		CKey Key;
		Key.m_Position = _Position;
		Key.m_Identifier = _Identifier;

		if (auto *pPosition = m_PositionTree.f_FindEqual(Key))
			return pPosition;

		auto &NewNode = m_Positions.f_InsertFirst();
		NewNode.m_Key = fg_Move(Key);
		NewNode.m_Message = _Message;

		m_PositionTree.f_Insert(NewNode);

		return &NewNode;
	}

	void CBuildSystemUniquePositions::f_AddFirstFromPositions(CBuildSystemUniquePositions const &_Other)
	{
		if (_Other.f_IsEmpty())
			return;
		auto &First = _Other.m_Positions.f_GetFirst();
		f_AddPosition(First.m_Key.m_Position, First.m_Key.m_Identifier, First.m_Message);
	}

	void CBuildSystemUniquePositions::f_AddPositions(CBuildSystemUniquePositions const &_Other)
	{
		for (auto &Position : _Other.m_Positions)
			f_AddPosition(Position.m_Key.m_Position, Position.m_Key.m_Identifier, Position.m_Message);
	}

	void CBuildSystemUniquePositions::f_AddPositions(CBuildSystemUniquePositions &&_Other)
	{
		if (m_Positions.f_IsEmpty())
		{
			m_Positions = fg_Move(_Other.m_Positions);
			m_PositionTree = fg_Move(_Other.m_PositionTree);
			return;
		}

		for (auto &Position : _Other.m_Positions)
		{
			_Other.m_PositionTree.f_Remove(Position);
			f_AddPosition(fg_Move(Position.m_Key.m_Position), fg_Move(Position.m_Key.m_Identifier), fg_Move(Position.m_Message));
		}
	}

	void CBuildSystemUniquePositions::f_AddPositions(NStorage::TCSharedPointer<CBuildSystemUniquePositions> const &_pOther)
	{
		if (!_pOther)
			return;
		return f_AddPositions(*_pOther);
	}
}
