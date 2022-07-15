#include "JobThread.h"
#include <assert.h>

void vp::JobThread::Start(JobThreadConfig config)
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

vp::JobThread::JobThread() : _threadHandle() // to explicitly show the intent
{
}

vp::JobThread::~JobThread()
{ 
	if (_threadHandle.joinable())
		_threadHandle.join();
}

void vp::JobThread::runLoop()
{
}
