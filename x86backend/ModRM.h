#ifndef INTERPRETER_X86BACKEND_MODRM_H
#define INTERPRETER_X86BACKEND_MODRM_H

#include "build.h"

#include "Defs.h"
#include "Registers.h"
#include "SIB.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		ModRM.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: ModR/M opcode part definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

enum class ModField : unsigned char
{
	NoShift = 0, // 00b
	Disp8   = 1, // 01b
	Disp32  = 2, // 10b
	Direct  = 3  // 11b
};

// [xxx]
enum class IndirectNoShift : unsigned char
{
	RAX             = 0,
	RCX             = 1,
	RDX             = 2,
	RBX             = 3,
	UseSIB          = 4,
	Displacement32  = 5, // WARNING: this is RIP-based addressing in x86_64
	RSI             = 6,
	RDI             = 7
};

// [xxx] + disp8
enum class IndirectShiftDisplacement8 : unsigned char
{
	RAX    = 0,
	RCX    = 1,
	RDX    = 2,
	RBX    = 3,
	UseSIB = 4,
	RBP    = 5,
	RSI    = 6,
	RDI    = 7
};

// [xxx] + disp32
enum class IndirectShiftDisplacement32 : unsigned char
{
	RAX    = 0,
	RCX    = 1,
	RDX    = 2,
	RBX    = 3,
	UseSIB = 4,
	RBP    = 5,
	RSI    = 6,
	RDI    = 7
};

struct ModRM
{
	static const unsigned char _UseSIB = 4;
	static const unsigned char _UseDisplacement32 = 5;

	unsigned char rm : 3;
	unsigned char reg : 3;
	ModField mod : 2;

	// SIB is not used when mod+r/m directly addresses 4-th register
	bool UsingSIB() {
		return ( mod != ModField::Direct ) && ( rm == _UseSIB );
	}

	// Displacement32 is implicitly used when mod == no shift and r/m == 5
	ModField UsingDisplacement() {
		if( ( mod == ModField::NoShift ) && ( rm == _UseDisplacement32 ) )
			return ModField::Disp32;
		return mod;
	}

} PACKED;
static_assert( sizeof( ModRM ) == 1, "ModR/M structure is not packed" );

struct ModRMWrapper
{
	static const AddressSize _DefaultMemoryLocationSize = AddressSize::NONE;

	union {
		IndirectNoShift no_shift;
		IndirectShiftDisplacement8 disp8;
		IndirectShiftDisplacement32 disp32;
		unsigned char raw;
	};

	bool need_extension;
	ModField mod;
	AddressSize operand_size;

	SIBWrapper sib;

	// Intended for mod == 00b, memory addressing like [RAX] or displacement32
	ModRMWrapper( IndirectNoShift reg ) :
		no_shift( reg ),
		need_extension( false ),
		mod( ModField::NoShift ),
		operand_size( _DefaultMemoryLocationSize )
	{
	}

	// Intended for mod == 01b, memory addressing like [RAX]+displacement8
	ModRMWrapper( IndirectShiftDisplacement8 reg ) :
		disp8( reg ),
		need_extension( false ),
		mod( ModField::Disp8 ),
		operand_size( _DefaultMemoryLocationSize )
	{
	}

	// Intended for mod == 10b, memory addressing like [RAX]+displacement32
	ModRMWrapper( IndirectShiftDisplacement32 reg ) :
		disp32( reg ),
		need_extension( false ),
		mod( ModField::Disp32 ),
		operand_size( _DefaultMemoryLocationSize )
	{
	}

	// Intended for mod == 11b, register-register operations
	ModRMWrapper( RegisterWrapper reg ) :
		raw( reg.raw ),
		need_extension( reg.need_extension ),
		mod( ModField::Direct ),
		operand_size( reg.operand_size )
	{
		s_cassert( reg.operand_size == AddressSize::QWORD, "Cannot generate direct r/m field from non-QWORD register" );
	}

	template <typename BaseRegT, typename IndexRegT>
	ModRMWrapper( BaseRegT base_reg, IndexRegT index_reg, unsigned char scale_factor, ModField shift ) :
		no_shift( IndirectNoShift::UseSIB ),
		need_extension( false ),
		mod( shift ),
		operand_size( _DefaultMemoryLocationSize ),
		sib( base_reg, index_reg, scale_factor )
	{
	}

	// Intended for any mod-field, addressing of additional registers (R8, [R8], [R8]+displacement8, [R8]+displacement32)
	ModRMWrapper( Reg64E reg, ModField shift ) :
		raw( reinterpret_cast<unsigned char&>( reg ) ),
		need_extension( true ),
		mod( shift ),
		operand_size( _DefaultMemoryLocationSize )
	{
		if( reg == Reg64E::R13 && shift == ModField::NoShift ) {
			no_shift = IndirectNoShift::UseSIB;
			need_extension = false;
			mod = ModField::Disp8;
			sib = SIBWrapper( reg, IndexRegs::None );
		}
	}
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_MODRM_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;