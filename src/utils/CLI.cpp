#include "CLI.h"

#include <flags.h>
#include <spdlog/spdlog.h>

vp::console::CmdCtx::~CmdCtx() {}

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


vp::console::CmdRunner::CmdRunner()
{
	auto list_all = [&](const CmdCtx& ctx)
	{
		std::optional<std::string_view> opt_filter =
			ctx.parsed_args->get<std::string_view>("filter");

		std::string_view filter_str = opt_filter.value_or("-");
		bool filter = opt_filter.has_value();
		bool verbose = ctx.parsed_args->get<bool>("verbose", false);

		for (auto& [name, cmd] : commands)
		{
			std::string_view name_v = name;
			bool do_print = !filter || (name_v.starts_with(filter_str));
			if (!do_print)
				continue;

			SPDLOG_INFO("*{}", name);
			if (verbose && (cmd.desc != nullptr))
			{
				SPDLOG_INFO("*  -{}\n", cmd.desc);
			}
		}
		return true;
	};

	ClCommand cmd{.name = "help",
		.desc =
			"List all commands, -verbose=1 to print help, -filter=\"text\" to filter "
			"by substring.",
		.registered = true,
		.command = list_all};

	commands.emplace(cmd.name, std::move(cmd));
}


bool vp::console::ClCommand::trigger(std::string args_as_text)
{
	if (command)
	{
		CmdCtx ctx;
		ctx.full_arg_string = std::move(args_as_text);
		tokenize(ctx.full_arg_string, ctx.tokens);
		ctx.parsed_args.reset(new flags::args(ctx.tokens));
		ctx.name = name;

		if (ctx.parsed_args->get<bool>("help", false) && (nullptr != desc))
		{
			SPDLOG_INFO("Help for [{}] command : \n{}\n", name, desc); 
			return true;
		}

		return command(ctx);
	}
	return false;
}
