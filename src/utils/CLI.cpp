#include "CLI.h"

#include <flags.h>
#include <spdlog/spdlog.h>

void vp::console::tokenize(const std::string& args_as_text, std::vector<std::string_view>& out_args)
{
	auto eptr = args_as_text.c_str() + args_as_text.size();

	const char* wb = args_as_text.c_str();
	while (wb < eptr && *wb == ' ')
	{
		wb++;
	}
	const char* we = wb;
	while (we < eptr)
	{
		if (*we == ' ')
		{
			auto end_of_slice = we;
			while (*we == ' ')
			{
				we++;
			}

			out_args.push_back({wb, end_of_slice});
			wb = we;
			continue;
		}
		we++;
	}
	if (wb != we)
		out_args.push_back({wb, we});
}
 

vp::console::CmdCtx::~CmdCtx() {   }

void vp::console::ClCommand::trigger(std::string args_as_text)
{
	if (command)
	{
		CmdCtx ctx;
		ctx.full_arg_string = std::move(args_as_text);
		tokenize(ctx.full_arg_string, ctx.tokens);
		ctx.parsed_args.reset(new flags::args(ctx.tokens));
		ctx.name = name;

		if (ctx.parsed_args->get<bool>("help", false))
		{
			SPDLOG_INFO("Help for [{}] command : \n{}\n", name, desc);
		}

		command(ctx);
	}
}
