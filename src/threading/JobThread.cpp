#include "JobThread.h"
#include <assert.h>

void vex::JobThread::Start(JobThreadConfig config)
{
	static bool gGuard = false;

	if (gGuard)
	{
		assert(false);
		return;
	}
	gGuard = true;

	_config = config;
	_threadHandle = std::thread([]() { runLoop(); });
	//_threadHandle.detach();
}

vex::JobThread::JobThread() : _threadHandle() // to explicitly show the intent
{
}

vex::JobThread::~JobThread()
{ 
	if (_threadHandle.joinable())
		_threadHandle.join();
}

void vex::JobThread::runLoop()
{
}
