#pragma once
#include <memory>
#include <thread>

namespace vp
{
	//name, debug data, ect
	struct JobThreadConfig
	{

	};

	class JobThread
	{
	public: 
		void Start(JobThreadConfig config);

		const JobThreadConfig& Config() const { return _config; }

		JobThread();
		~JobThread();
	private:
		static void runLoop();

		std::thread _threadHandle;
		JobThreadConfig _config;
	};
}