#include "ARenderSystem.h"

#include <SDL.h>

#include "scene/Grid.h" 
#include "VFramework/Misc/Color.h"

using namespace vp;

void DrawLine(v3f origin, v3f destination, vex::Color c);

void DrawGrid(SDL_Renderer* renderer, Grid gd)
{
	// SDL_RenderDrawLine()
}

void vp::ARenderSystem::OnUpdate(vex::World& world)
{
	// clean
	// setup camera
	// draw grid

	// draw agents

	// present
}
