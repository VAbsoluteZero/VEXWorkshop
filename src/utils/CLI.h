#pragma once
#include <VCore/Containers/Dict.h>
#include <VCore/Utils/CoreTemplates.h>
#include <VCore/Utils/VMath.h>
#include <VCore/Utils/VUtilsBase.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace flags
{
	struct args;
}

namespace vp::console
{
	// struct with params
	struct CmdCtx
	{
		const char* name = nullptr;
		std::string full_arg_string;
		std::vector<std::string_view> tokens;
		std::unique_ptr<flags::args> parsed_args;
		~CmdCtx();
	};

	void tokenize(const std::string& args_as_text, std::vector<std::string_view>& out_args); 

	struct ClCommand
	{
		const char* name = nullptr;
		const char* desc = nullptr;
		bool registered = false;
		// const char* last_error_msg = nullptr;
		std::function<bool(const CmdCtx&)> command;
		bool trigger(std::string args_as_text);
	};

	struct CmdRunner
	{
		static CmdRunner& global()
		{
			static CmdRunner runner;
			return runner;
		}

		bool registerCmd(ClCommand cmd, bool replace_if_needed = true)
		{
			auto key = cmd.name;
			bool b_does_not_contain = !commands.contains(key);
			if (replace_if_needed || b_does_not_contain)
			{
				cmd.registered = true;
				commands.emplace(cmd.name, std::move(cmd));
				return true;
			}
			return false;
		}
		void remove(const char* key) { commands.remove(key); }
		 
		CmdRunner();
		vex::Dict<const char*, ClCommand> commands; 
	};
} // namespace vp::console

namespace vp::console
{
	template <typename Callable>
	inline bool makeAndRegisterCmd(
		const char* name, const char* desc, bool replace_if_needed, Callable&& callable)
	{
		ClCommand cmd{.name = name, .desc = desc, . registered = false, .command = callable};

		return CmdRunner::global().registerCmd(cmd, replace_if_needed);
	}
	inline bool registerCmd(ClCommand cmd, bool replace_if_needed = true)
	{
		return CmdRunner::global().registerCmd(std::move(cmd), replace_if_needed);
	}
	inline void removeCmd(const char* key) { CmdRunner::global().remove(key); }
	inline ClCommand* findCmd(const char* key) { return CmdRunner::global().commands.tryGet(key); }
}; // namespace vp::console
