﻿#include "stdafx.h"

#include "time_ops.h"

#include <uXray/fxjitruntime.h>
#include <uXray/fxlog_console.h>
#include "API.h"

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr",
																		EVERYTHING & ~(MASK (Debug::E_OBJCREATION) | MASK (Debug::E_OBJDESTRUCTION)),
																		EVERYTHING),
												   &FXConLog::Instance());

	Debug::API::SetTypewideVerbosity ("MallocAllocator", Debug::E_USER);
	Debug::API::SetTypewideVerbosity ("StaticAllocator", Debug::E_USER);

	Debug::API::SetDefaultVerbosity (Debug::E_VERBOSE);
	Debug::API::SetTypewideVerbosity (0, Debug::E_DEBUG);
	Debug::API::SetTypewideVerbosity ("NativeExecutionManager", Debug::E_VERBOSE);


	Processor::ProcessorAPI api;

	NativeExecutionManager nexec;
	__sassert (nexec.EHSelftest(), "Exception handler self-test failed");
	nexec.EnableExceptionHandling();

	api.Attach (new ProcessorImplementation::MMU);
	api.Attach (new ProcessorImplementation::UATLinker);
	api.Attach (new ProcessorImplementation::Logic);
	api.Attach (new ProcessorImplementation::CommandSet_mkI);
	api.Attach (new ProcessorImplementation::FloatExecutor);
	api.Attach (new ProcessorImplementation::IntegerExecutor);
	api.Attach (new ProcessorImplementation::ServiceExecutor);
// 	api.Flush();

	api.Attach (new ProcessorImplementation::AsmHandler);
	FILE* read = fopen ("input.dasm", "rt");
	api.Load (read);

	delete api.Reader();

	calibrate_and_setup_clock();
	clock_measure_start ("Execution");

	Processor::calc_t result = api.Exec();

	Processor::ProcDebug::PrintValue (result);
	smsg (E_INFO, E_USER, "Testing has apparently completed OK: result %s", Processor::ProcDebug::debug_buffer);

	clock_measure_end ();

	delete api.MMU();
	delete api.Linker();
	delete api.LogicProvider();
	delete api.CommandSet();
	delete api.Executor (Processor::Value::V_FLOAT);
	delete api.Executor (Processor::Value::V_INTEGER);
	delete api.Executor (Processor::Value::V_MAX);

	Debug::System::Instance().CloseTargets();
}