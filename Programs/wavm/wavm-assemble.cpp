#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "wavm.h"

using namespace WAVM;

static bool loadTextModuleFromFile(const char* filename, IR::Module& outModule)
{
	std::vector<U8> wastBytes;
	if(!loadFile(filename, wastBytes)) { return false; }

	// Make sure the WAST is null terminated.
	wastBytes.push_back(0);

	std::vector<WAST::Error> parseErrors;
	if(WAST::parseModule((const char*)wastBytes.data(), wastBytes.size(), outModule, parseErrors))
	{ return true; }
	else
	{
		Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
		reportParseErrors(filename, (const char*)wastBytes.data(), parseErrors);
		return false;
	}
}

void showAssembleHelp(Log::Category outputCategory)
{
	Log::printf(outputCategory,
				"Usage: wavm assemble [options] in.wast out.wasm\n"
				"  -n|--omit-names       Omits WAST names from the output\n"
				"  --enable <feature>    Enable the specified feature. See the list of supported\n"
				"                        features below.\n"
				"\n"
				"Features:\n"
				"%s"
				"\n",
				getFeatureListHelpText());
}

int execAssembleCommand(int argc, char** argv)
{
	if(argc < 2)
	{
		showAssembleHelp(Log::error);
		return EXIT_FAILURE;
	}
	const char* inputFilename = nullptr;
	const char* outputFilename = nullptr;
	bool omitNames = false;
	IR::FeatureSpec featureSpec;
	if(argc > 2)
	{
		for(Iptr argIndex = 0; argIndex < argc; ++argIndex)
		{
			if(!strcmp(argv[argIndex], "-n") || !strcmp(argv[argIndex], "--omit-names"))
			{ omitNames = true; }
			else if(!strcmp(argv[argIndex], "--enable"))
			{
				++argIndex;
				if(!argv[argIndex])
				{
					Log::printf(Log::error, "Expected feature name following '--enable'.\n");
					return false;
				}

				if(!parseAndSetFeature(argv[argIndex], featureSpec, true))
				{
					Log::printf(Log::error,
								"Unknown feature '%s'. Supported features:\n"
								"%s"
								"\n",
								argv[argIndex],
								getFeatureListHelpText());
					return false;
				}
			}
			else if(!inputFilename)
			{
				inputFilename = argv[argIndex];
			}
			else if(!outputFilename)
			{
				outputFilename = argv[argIndex];
			}
			else
			{
				Log::printf(Log::error, "Unrecognized argument: %s\n", argv[argIndex]);
				return EXIT_FAILURE;
			}
		}
	}

	// Load the WAST module.
	IR::Module module(featureSpec);
	if(!loadTextModuleFromFile(inputFilename, module)) { return EXIT_FAILURE; }

	// If the command-line switch to omit names was specified, strip the name section.
	if(omitNames)
	{
		for(auto sectionIt = module.customSections.begin();
			sectionIt != module.customSections.end();
			++sectionIt)
		{
			if(sectionIt->name == "name")
			{
				module.customSections.erase(sectionIt);
				break;
			}
		}
	}

	// Serialize the WASM module.

	Timing::Timer saveTimer;

	Serialization::ArrayOutputStream stream;
	WASM::saveBinaryModule(stream, module);
	std::vector<U8> wasmBytes = stream.getBytes();

	Timing::logRatePerSecond(
		"Serialized WASM", saveTimer, wasmBytes.size() / 1024.0 / 1024.0, "MiB");

	// Write the serialized data to the output file.
	return saveFile(outputFilename, wasmBytes.data(), wasmBytes.size()) ? EXIT_SUCCESS
																		: EXIT_FAILURE;
}
