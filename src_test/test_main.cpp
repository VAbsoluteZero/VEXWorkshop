#include <cstdio>
#include <optional>

#include "config.h"

int main(int argc, char** argv)
{
	// Parse the command line arguments.
	std::optional<snitch::cli::input> args = snitch::cli::parse_arguments(argc, argv);
	if (!args)
	{
		// Parsing failed, an error has been reported, just return.
		return 1;
	}

	// Configure snitch using command line options.
	// You can then override the configuration below, or just remove this call to disable
	// command line options entirely.
	args->arguments.push_back({"--verbosity", "high", "high"});
	snitch::tests.configure(*args);

	// Your own initialization code goes here.
	// ...

	// Actually run the tests.
	// This will apply any filtering specified on the command line.
	bool r = snitch::tests.run_tests(*args) ? 0 : 1; 

	return r;
}