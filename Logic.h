﻿#ifndef _LOGIC_H
#define _LOGIC_H

#include "build.h"
#include "Interfaces.h"

namespace ProcessorImplementation
{
	using namespace Processor;

	class INTERPRETER_API Logic : public ILogic
	{
		static const char* RegisterIDs [R_MAX];

	public:
		Logic();
		virtual ~Logic();

		virtual void Analyze (calc_t value);
		virtual void Syscall (size_t index);

		virtual Register DecodeRegister (const char* reg);
		virtual const char* EncodeRegister (Register reg);

		virtual void Jump (Reference& ref);
		virtual calc_t Read (Reference& ref);
		virtual void Write (Reference& ref, calc_t value);

		virtual calc_t StackTop();
		virtual calc_t StackPop();
		virtual void StackPush (calc_t value);

		virtual size_t ChecksumState();

		virtual void ExecuteSingleCommand (Command& command);
	};
}

#endif // _LOGIC_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
