#pragma once
#include <VCore/Systems/ISystem.h>
#include <VCore/World/World.h>

namespace vp
{
	class ARenderSystem : vex::ISystem
	{
	public:
		virtual void OnUpdate(vex::World& world) override;
	};
}