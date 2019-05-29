
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_PS4

#include "SDL_hints.h"
#include "SDL_assert.h"
#include "../SDL_sysrender.h"


SDL_Renderer * PS4_CreateRenderer(SDL_Window * window, Uint32 flags)
{
	printf("# ERROR %s(), please use software renderer on ps4!\n", __FUNCTION__);
	return NULL;
}




SDL_RenderDriver PS4_RenderDriver = {
	.CreateRenderer = PS4_CreateRenderer,
	.info = {
		.name = "PS4",
		.flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
		.num_texture_formats = 4,
		.texture_formats = {
			[0] = SDL_PIXELFORMAT_BGR565,
			[1] = SDL_PIXELFORMAT_ABGR1555,
			[2] = SDL_PIXELFORMAT_ABGR4444,
			[3] = SDL_PIXELFORMAT_ABGR8888,
		},
		.max_texture_width  = 4096,
		.max_texture_height = 4096,
	 }
};



#endif // SDL_VIDEO_RENDER_PS4