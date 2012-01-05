#include "stdafx.h"
#include "AssemblyIO.h"

ImplementDescriptor (AsmHandler, "assembly reader/writer", MOD_APPMODULE);

namespace ProcessorImplementation
{
	using namespace Processor;

	AsmHandler::AsmHandler() :
	writing_file_ (0)
	{
	}

	bool AsmHandler::_Verify() const
	{
		if (writing_file_)
			verify_statement (!ferror (writing_file_), "Error in writing stream");
		return 1;
	}

	void AsmHandler::WrReset()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Resetting writing file");
		fclose (writing_file_);
		writing_file_ = 0;
	}

	void AsmHandler::WrSetup (FILE* file)
	{
		writing_file_ = file;
		verify_method;

		msg (E_INFO, E_DEBUG, "Writer set up");
	}

	void AsmHandler::Write (size_t ctx_id)
	{
		verify_method;
		__assert (writing_file_, "Writer has not been set up");

		msg (E_INFO, E_VERBOSE, "Beginning ASM write of context %zu", ctx_id);

		proc_ ->MMU() ->SetTemporaryContext (ctx_id);
		InternalWriteFile();
		proc_ ->MMU() ->RestoreContext();
	}

	void AsmHandler::InternalWriteFile()
	{
		__asshole ("Not implemented");
	}

	size_t AsmHandler::NextSection (Processor::FileProperties* prop,
									Processor::FileSectionType* type,
									size_t*,
									size_t*)
	{
		verify_method;

		__assert (prop, "Invalid properties pointer");
		auto cur_section = prop ->file_description.front();

		if (cur_section.first != SEC_MAX)
			return 0; /* we always have 2 sections, with first being a stub (SEC_MAX) */

		prop ->file_description.pop_front();

		if (type)
			*type = prop ->file_description.front().first;

		return 1;
	}

	FileProperties AsmHandler::RdSetup (FILE* file)
	{
		__assert (file, "Invalid reading file");
		__assert (!ferror (file), "Error in stream");

		FileProperties fp;
		fp.file = file;
		fp.file_description.push_back (std::make_pair (SEC_MAX, 0));
		fp.file_description.push_back (std::make_pair (SEC_NON_UNIFORM, 0));
		return fp;
	}

	void AsmHandler::RdReset (FileProperties* prop)
	{
		__assert (prop, "Invalid properties");
		__assert (prop ->file, "Invalid file");

		fclose (prop ->file);
		prop ->file = 0;
		prop ->file_description.clear();
	}

	size_t AsmHandler::ReadNextElement (FileProperties*, void*, size_t)
	{
		__asshole ("Not implemented");
		return 0;
	}

	size_t AsmHandler::ReadSectionImage (FileProperties*, void*, size_t)
	{
		__asshole ("Not implemented");
		return 0;
	}

	Reference AsmHandler::ParseReference (Processor::symbol_map& symbols, const char* arg)
	{
		verify_method;

		char id;
		size_t address;
		Reference result;
		init (result);

		if (sscanf (arg, "%c:%zu", &id, &address) == 2)
		{
			result.is_symbol = 0;
			result.direct.address = address;

			switch (id)
			{
			case 'c':
				result.direct.type = S_CODE;
				break;

			case 'd':
				result.direct.type = S_DATA;
				break;

			case 'f':
				result.direct.type = S_FRAME;
				break;

			case 'p':
				result.direct.type = S_FRAME_BACK;
				break;

			case 'r':
				result.direct.type = S_REGISTER;
				__verify (address < R_MAX, "Invalid address in register reference: %zu", address);
				break;

			default:
				result.direct.type = S_NONE;
			}

			__verify (result.direct.type != S_NONE, "Invalid direct address specifier: '%c'", id);
		}

		else if (arg[0] == '$')
		{
			result.is_symbol = 0;
			result.direct.type = S_REGISTER;
			result.direct.address = proc_ ->LogicProvider() ->DecodeRegister (arg + 1);
		}

		else
		{
			Symbol decoded_symbol;
			init (decoded_symbol);

			decoded_symbol.is_resolved = 0; /* symbol is not defined here. */
			symbols.insert (PrepareSymbol (arg, decoded_symbol, &result.symbol_hash)); /* add symbols into mentioned and write hash to reference */
		}

		return result;
	}

	char* AsmHandler::PrepLine (char* read_buffer)
	{
		char *begin, *tmp = read_buffer;

		while (isspace (*(begin = tmp++)));

		if (*begin == '\0')
			return 0;

		tmp = begin;

		/* drop unused parts of decoded string */
		do
		{
			if (*tmp == ';')
				*tmp = '\0';

			else if (*tmp == '\n')
				*tmp = '\0';

			else
				*tmp = tolower (*tmp);

		} while (*tmp++);

		return begin;
	}

	void AsmHandler::ReadSingleDeclaration (const char* decl_data, DecodeResult& output)
	{
		char name[STATIC_LENGTH], initialiser[STATIC_LENGTH], type;

		// Declaration is a symbol itself.
		Symbol declaration;
		init (declaration);
		declaration.is_resolved = 1;

		int arguments = sscanf (decl_data, "%s %c %s", name, &type, initialiser);
		switch (arguments)
		{
			case 3:
				switch (type)
				{
					case '=':
						output.data.Parse (initialiser); // type of value is already set

						declaration.ref.is_symbol = 0;
						declaration.ref.direct.type = S_DATA;
						declaration.ref.direct.address = ILinker::symbol_auto_placement_addr;

						ProcDebug::PrintValue (output.data);
						msg (E_INFO, E_DEBUG, "Declaration: DATA entry = %s", ProcDebug::debug_buffer);

						output.type = DecodeResult::DEC_DATA;
						break;

					case ':':
						declaration.ref = ParseReference (output.mentioned_symbols, initialiser);

						ProcDebug::PrintReference (declaration.ref);
						msg (E_INFO, E_DEBUG, "Declaration: alias to %s", ProcDebug::debug_buffer);

						output.type = DecodeResult::DEC_NOTHING; // in this case declaration is not a declaration itself
						break;

					default:
						__asshole ("Parse error: \"%s\"", decl_data);
						break;
				}

				output.mentioned_symbols.insert (PrepareSymbol (name, declaration));
				break;

			case 1:
				output.data = Value(); // default initialiser

				declaration.ref.is_symbol = 0;
				declaration.ref.direct.type = S_DATA;
				declaration.ref.direct.address = ILinker::symbol_auto_placement_addr;

				msg (E_INFO, E_DEBUG, "Declaration: DATA entry uninitialised");

				output.type = DecodeResult::DEC_DATA;
				output.mentioned_symbols.insert (PrepareSymbol (name, declaration));
				break;

			default:
				__asshole ("Parse error: \"%s\"", decl_data);
				break;
		}
	}

	void AsmHandler::ReadSingleCommand (const char* command,
										const char* argument,
										Processor::DecodeResult& output)
	{
		const CommandTraits& desc = proc_ ->CommandSet() ->DecodeCommand (command);
		output.command.id = desc.id;

		if (!argument)
		{
			__verify (desc.arg_type == A_NONE,
					  "No argument required for command \"%s\" while given argument \"%s\"",
					  command, argument);

			msg (E_INFO, E_DEBUG, "Command: \"%s\" no argument", desc.mnemonic);
		}

		else /* have argument */
		{
			__verify (desc.arg_type != A_NONE,
					  "Argument needed for command \"%s\"", command);

			switch (desc.arg_type)
			{
			case A_REFERENCE:
			{
				output.command.arg.ref = ParseReference (output.mentioned_symbols, argument);

				ProcDebug::PrintReference (output.command.arg.ref);
				msg (E_INFO, E_DEBUG, "Command: \"%s\" reference to %s",
					 desc.mnemonic, ProcDebug::debug_buffer);

				break;
			}

			case A_VALUE:
			{
				output.command.arg.value.Parse (argument);

				ProcDebug::PrintValue (output.command.arg.value);
				msg (E_INFO, E_DEBUG, "Command: \"%s\" argument %s",
					 desc.mnemonic, ProcDebug::debug_buffer);

				break;
			}

			case A_NONE:
			default:
				__asshole ("Switch error");
				break;
			} // switch (argument type)

		} // have argument
	}

	char* AsmHandler::ParseLabel (char* string)
	{
		do
		{
			if (*string == ' ' || *string == '\0')
				return 0; // no label

			if (*string == ':')
			{
				*string = '\0';
				return string + 1; // found label
			}
		} while (*string++);

		return 0;
	}

	void AsmHandler::ReadSingleStatement (size_t line_num, char* input, DecodeResult& output)
	{
		verify_method;
		size_t labels = 0;
		Symbol label_symbol;
		init (label_symbol);

		Value::Type statement_type;
		char *command, *argument, typespec = default_type_specifier, *current_position = PrepLine (input);


		/* skip leading space and return if empty string */

		if (!current_position)
		{
			msg (E_INFO, E_DEBUG, "Not decoding empty line");
			return;
		}

		msg (E_INFO, E_DEBUG, "Decoding line %zu: \"%s\"", line_num, current_position);


		/* parse labels */

		while (char* next_chunk = ParseLabel (current_position))
		{
			label_symbol.is_resolved = 1; /* defined here */
			label_symbol.ref.is_symbol = 0; /* it does not reference another symbol */
			label_symbol.ref.direct.type = S_CODE; /* it points to text */
			label_symbol.ref.direct.address = ILinker::symbol_auto_placement_addr; /* its address must be determined later */

			output.mentioned_symbols.insert (PrepareSymbol (current_position, label_symbol));

			msg (E_INFO, E_DEBUG, "Label: \"%s\"", current_position);

			++labels;

			current_position = next_chunk;
			while (isspace (*current_position)) ++current_position;
		}
		msg (E_INFO, E_DEBUG, "Labels: %zu", labels);


		/* get remaining statement */

		command = strtok (current_position, " \t");
		argument = strtok (0, " \t");

		if (!command)
			return; // no command

		/* parse explicit type-specifier */

		if (char* dot = strchr (command, '.'))
		{
			*dot++ = '\0';
			typespec = *dot;
		}

		/* decode type-specifier */

		switch (tolower (typespec))
		{
			case 'f':
				statement_type = Value::V_FLOAT;

			case 'd':
			case 'i':
				statement_type = Value::V_INTEGER;

			default:
				__asshole ("Invalid command type specification: '%c'", typespec);
		}

		/* determine if we have a declaration or a command and parse accordingly */

		if (!strcmp (command, "decl"))
		{
			__verify (argument, "Empty declaration");

			output.type = DecodeResult::DEC_DATA;
			output.data.type = statement_type;

			ReadSingleDeclaration (argument, output);
		}

		else
		{
			output.type = DecodeResult::DEC_COMMAND;
			output.command.type = statement_type;

			ReadSingleCommand (command, argument, output);
		}
	}

	size_t AsmHandler::ReadStream (FileProperties* prop, DecodeResult* destination)
	{
		__assert (prop, "Invalid properties");
		__assert (destination, "Invalid destination");
		__assert (prop ->file_description.front().first == SEC_NON_UNIFORM,
				  "Invalid (non-stream) section type: \"%s\"",
				  ProcDebug::FileSectionType_ids[prop ->file_description.front().first]);

		*destination = DecodeResult();
		char read_buffer[STATIC_LENGTH];

		do
		{
			if (!fgets (read_buffer, STATIC_LENGTH, prop ->file))
				return 0;

			ReadSingleStatement (++prop ->file_description.front().second, read_buffer, *destination);

		} while (destination ->type == DecodeResult::DEC_NOTHING);

		return 1;
	}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
