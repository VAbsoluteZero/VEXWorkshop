#include "JobAPI.h"

#include <cassert>

using namespace parallel;

void Tasks::AddTask(std::unique_ptr<ITask> task)
{
	assert(task);
}
