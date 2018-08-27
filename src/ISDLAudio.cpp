
#include "SDLAudio.h"

static SDL::SDLAudio* s_sdl = nullptr;

namespace SDL {
	ISDL* SimpleSDL::createInstance()
	{
		if (s_sdl == nullptr)
		{
			s_sdl = new SDLAudio();
		}
		return s_sdl;
	}

	ISDL* SimpleSDL::getInstance()
	{
		return s_sdl;
	}

	void SimpleSDL::release()
	{
		if (s_sdl != nullptr)
		{
			delete s_sdl;
		}
	}
}