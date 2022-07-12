#pragma once

#include <memory>

namespace parallel
{
    class ITask {};
	namespace Tasks
	{
		void AddTask(std::unique_ptr<ITask> task);
	}
}