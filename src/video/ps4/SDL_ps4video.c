/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#if __INTELLISENSE__
#define SDL_VIDEO_DRIVER_PS4 1
#endif

#if SDL_VIDEO_DRIVER_PS4

//#include <gnm.h>


/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

#include "SDL_ps4video.h"

static int PS4_Available(void);
static SDL_VideoDevice *PS4_Create();


VideoBootStrap PS4_bootstrap = {
	"PS4",
	"PS4 Video Driver",
	PS4_Available,
	PS4_Create
};

#define INLINE inline

INLINE static SDL_VideoData * getVideoData(_THIS) {
	return ((SDL_VideoData *)_this->driverdata);
}
#define VData getVideoData(_this)
#define PS4_VideoData SDL_VideoData *videoData = VData


INLINE static void setHandle(_THIS, int handle) {
	VData->h_vout = handle;
}

INLINE static int32_t Handle(_THIS) { return VData->h_vout; }

INLINE static size_t Width(_THIS) { return VData->attr.width; }
INLINE static size_t Height(_THIS) { return VData->attr.height; }

INLINE static size_t MemSize(_THIS) { return VData->memSize; }
INLINE static size_t BufferSize(_THIS) { return VData->bufSize; }
INLINE static size_t BufferCount(_THIS) { return VOUT_NUM_BUFFERS; } // (tripleBuffer ? 3 : 2); }

INLINE static uint8_t* GetBuffer(_THIS, size_t n) { return VData->addrList[n & 3]; }
INLINE static uint8_t* CurrentBuffer(_THIS) { return GetBuffer(_this, VData->currBuffer); }
INLINE static uint8_t* NextBuffer(_THIS) { return GetBuffer(_this, ((VData->currBuffer + 1) % VOUT_NUM_BUFFERS)); }


#if 1
INLINE static int32_t IsFlipPending(_THIS) {
	return sceVideoOutIsFlipPending(Handle(_this)) > 0;
}

INLINE static void WaitOnFlip(_THIS) {
	int out = 0;
	SceKernelEvent ev;
	while (IsFlipPending(_this)) {
		//assert(SCE_OK == 
			sceKernelWaitEqueue(VData->flipQueue, &ev, 1, &out, 0); 
		//);	// assert
	}
}
#else
//INLINE bool GetFlipStatus(_THIS,SceVideoOutFlipStatus *status) {
//	return SCE_OK == sceVideoOutGetFlipStatus(Handle(), status);
//}

// sceVideoOutGetResolutionStatus

INLINE static void WaitOnFlip(_THIS, size_t arg) {
	int out = 0;
	SceKernelEvent ev;
	SceVideoOutFlipStatus status;

	while (1) {
		GetFlipStatus(&status);
		if (status.flipArg >= arg)
			return;
		if (SCE_OK != sceKernelWaitEqueue(VData->flipQueue, &ev, 1, &out, 0)) {
			printf("ERROR sceKernelWaitEqueue() failed!\n"); return;
		};	// assert
	}
}
#endif

INLINE static void SubmitFlip(_THIS) //s64 buffer = -1, u64 arg = 0)
{
//	sce::Gnm::flushGarlic();

	//currBuffer = (buffer == -1) ? currBuffer : buffer;
	sceVideoOutSubmitFlip(Handle(_this), VData->currBuffer, SCE_VIDEO_OUT_FLIP_MODE_VSYNC, 0);	// !wantVsync _HSYNC
	//printf("SubmitFlip() Buffer[%d] %p \n", currBuffer, (u8*)CurrentBuffer());

	VData->currBuffer = ((VData->currBuffer + 1) % BufferCount(_this));

	WaitOnFlip(_this);
}






















static void FreeFramebuffers(_THIS)
{
	for (uint32_t i = 0; i < VOUT_NUM_BUFFERS; i++)
		sceVideoOutUnregisterBuffers(VData->h_vout, i);

	sceKernelMunmap((void*)VData->mapAddr, VData->memSize);
	sceKernelReleaseDirectMemory(VData->phyAddr, VData->memSize);
}

inline static size_t AlignUp(size_t v, size_t a)
{
	return ((v) + (a - 1)) & ~(a - 1);
}

static int AllocFramebuffers(_THIS)
{
	size_t alignTo = 1024 * 1024;
	size_t pixelCount = VData->attr.width * VData->attr.height, Bpp = 4; // 32bpp
	size_t alignedSize = AlignUp(pixelCount * Bpp, alignTo);

	VData->bufSize = pixelCount * Bpp;
	VData->memSize = alignedSize * VOUT_NUM_BUFFERS;

	int32_t res = SCE_OK;

	if (SCE_OK != (res = sceKernelAllocateDirectMemory(0, SCE_KERNEL_MAIN_DMEM_SIZE, VData->memSize, alignTo, SCE_KERNEL_WC_GARLIC, &VData->phyAddr))) {
		SDL_SetError("sceKernelAllocateDirectMemory() Failed ");	// printf("Error, sceKernelAllocateDirectMemory() Failed with 0x%08X\n", (uint32_t)res);
		return -1;
	}
	if (SCE_OK != (res = sceKernelMapDirectMemory((void**)&VData->mapAddr, VData->memSize, SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW, 0, VData->phyAddr, alignTo))) {
		SDL_SetError("sceKernelMapDirectMemory() Failed ");	// printf("Error, sceKernelMapDirectMemory() Failed with 0x%08X\n", (uint32_t)res);
		return -1;
	}

	for (uint32_t i = 0; i < VOUT_NUM_BUFFERS; i++) {
		VData->addrList[i] = VData->mapAddr + (i * alignedSize);	// fbAlloc(memSize, alignTo)
	//	printf("Buffer[%X] %p \n", i, (void*)VData->addrList[i]);
	}

	if (SCE_OK != (res = sceVideoOutRegisterBuffers(VData->h_vout, 0, (void* const*)VData->addrList, VOUT_NUM_BUFFERS, &VData->attr))) {
		SDL_SetError("sceVideoOutRegisterBuffers() Failed ");	// printf("Error, sceVideoOutRegisterBuffers() Failed with 0x%08X\n", (uint32_t)res);
		FreeFramebuffers(_this);
		return -1;
	}
	return res;
}


static int vout_init(_THIS)
{
	int handle = 0;
	SceVideoOutResolutionStatus res;
	
	handle = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	if(handle <= 0) {
		SDL_SetError("sceVideoOutOpen() failed");
		return -1;
	}
	setHandle(_this, handle);

	if (0 == sceVideoOutGetResolutionStatus(handle, &res)) {
		VData->width  = res.fullWidth;
		VData->height = res.fullHeight;
	}

	memset(&VData->attr, 0, sizeof(VData->attr));
	VData->attr.width	= VData->width;
	VData->attr.height	= VData->height;
	VData->attr.aspectRatio	= SCE_VIDEO_OUT_ASPECT_RATIO_16_9;
	VData->attr.pixelFormat	= SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB;
	VData->attr.tilingMode	= SCE_VIDEO_OUT_TILING_MODE_LINEAR; // (!tile) ? : 0;		// change to tiled later, GpuMode must be NEO for pro
	VData->attr.pitchInPixel= VData->width;


	if (SCE_OK != sceKernelCreateEqueue(&VData->flipQueue, "flipQueue") ||
		SCE_OK != sceVideoOutAddFlipEvent(VData->flipQueue, handle, NULL)) {
		SDL_SetError("Failed to create/add flip queue\n");
		return -1;
	}

	if (SCE_OK != AllocFramebuffers(_this)) {
		SDL_SetError("Failed to AllocFramebuffers!\n");
		return -1;
	}
	sceVideoOutSetFlipRate(handle, 0);	// 0=60fps , 1=30fps, 2=20fps?


	return 0;
}



void vout_term(_THIS) {
	FreeFramebuffers(_this);
	sceVideoOutClose(VData->h_vout);
	VData->h_vout = 0;
}

















/* unused
static SDL_bool PS4_initialized = SDL_FALSE;
*/
static int PS4_Available(void)
{
	D_FN();
    return 1;
}

static void PS4_Destroy(SDL_VideoDevice * device)
{
	D_FN();

/*    SDL_VideoData *phdata = (SDL_VideoData *) device->driverdata; */

    if (device->driverdata != NULL) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *PS4_Create()
{
	D_FN();

    SDL_VideoDevice *device;
    SDL_VideoData *phdata;
#if PS4_VIDEO_GL
  SDL_GLDriverData *gldata;
#endif

    int status;

    /* Check if PS4 could be initialized */
    status = PS4_Available();
    if (status == 0) {
        /* PS4 could not be used */
        return NULL;
    }

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal PS4 specific data */
    phdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (phdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

#if PS4_VIDEO_GL
	gldata = (SDL_GLDriverData *) SDL_calloc(1, sizeof(SDL_GLDriverData));
    if (gldata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        SDL_free(phdata);
        return NULL;
    }
    phdata->egl_initialized = SDL_FALSE;
    device->gl_data = gldata;
#endif

	phdata->h_vout = 0;		// VideoOut handle
	phdata->width  = 1920;
	phdata->height = 1080;	// defaults

    device->driverdata = phdata;

    /* Setup amount of available displays */
    device->num_displays = 0;

    /* Set device free function */
    device->free = PS4_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit			= PS4_VideoInit;
    device->VideoQuit			= PS4_VideoQuit;
    device->GetDisplayModes		= PS4_GetDisplayModes;
    device->SetDisplayMode		= PS4_SetDisplayMode;
    device->CreateSDLWindow		= PS4_CreateWindow;
    device->CreateSDLWindowFrom	= PS4_CreateWindowFrom;
    device->SetWindowTitle		= PS4_SetWindowTitle;
    device->SetWindowIcon		= PS4_SetWindowIcon;
    device->SetWindowPosition	= PS4_SetWindowPosition;
    device->SetWindowSize		= PS4_SetWindowSize;
    device->ShowWindow			= PS4_ShowWindow;
    device->HideWindow			= PS4_HideWindow;
    device->RaiseWindow			= PS4_RaiseWindow;
    device->MaximizeWindow		= PS4_MaximizeWindow;
    device->MinimizeWindow		= PS4_MinimizeWindow;
    device->RestoreWindow		= PS4_RestoreWindow;
    device->SetWindowGrab		= PS4_SetWindowGrab;
    device->DestroyWindow		= PS4_DestroyWindow;
#if PS4_VIDEO_GL
    device->GetWindowWMInfo		= PS4_GetWindowWMInfo;
    device->GL_LoadLibrary		= PS4_GL_LoadLibrary;
    device->GL_GetProcAddress	= PS4_GL_GetProcAddress;
    device->GL_UnloadLibrary	= PS4_GL_UnloadLibrary;
    device->GL_CreateContext	= PS4_GL_CreateContext;
    device->GL_MakeCurrent		= PS4_GL_MakeCurrent;
    device->GL_SetSwapInterval	= PS4_GL_SetSwapInterval;
    device->GL_GetSwapInterval	= PS4_GL_GetSwapInterval;
    device->GL_SwapWindow		= PS4_GL_SwapWindow;
    device->GL_DeleteContext	= PS4_GL_DeleteContext;
#endif
    device->HasScreenKeyboardSupport	= PS4_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard			= PS4_ShowScreenKeyboard;
    device->HideScreenKeyboard			= PS4_HideScreenKeyboard;
    device->IsScreenKeyboardShown		= PS4_IsScreenKeyboardShown;
#if 1
	device->CreateWindowFramebuffer		= PS4_CreateWindowFramebuffer;
	device->UpdateWindowFramebuffer		= PS4_UpdateWindowFramebuffer;
	device->DestroyWindowFramebuffer	= PS4_DestroyWindowFramebuffer;
#endif
    device->PumpEvents			= PS4_PumpEvents;

    return device;
}

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int PS4_VideoInit(_THIS)
{
	D_FN();

	uint8_t *ppp = (uint8_t*)SDL_malloc(1024 * 1024 * 128);
	if (ppp) SDL_free(ppp);
	else {
		printf("@@@ ERROR, the memory allocator is refusing to work,\n make sure you're not trying to use the GLES wrapper and try raising the libc heap limit!\n");
		return -1;
	}

    SDL_VideoDisplay display;

    SDL_DisplayMode current_mode;
    SDL_zero(current_mode);

	if (SCE_OK != vout_init(_this))
		return -1;

    current_mode.w = getVideoData(_this)->width;
    current_mode.h = getVideoData(_this)->height;

    current_mode.refresh_rate = 60;
    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;
	
    SDL_AddVideoDisplay(&display);

    return 1;
}

void PS4_VideoQuit(_THIS)
{
	D_FN();

	vout_term(_this);
}

void PS4_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
	D_FN();

}

int PS4_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
	D_FN();
    return 0;
}


int  PS4_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
	D_FN();

	//PS4_VideoData;	// videoData *

    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_BGR888;
    int w, h;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

    /* Free the old framebuffer surface */
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    surface = data->surface;
    SDL_FreeSurface(surface);

    /* Create a new one */
    SDL_PixelFormatEnumToMasks(surface_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    SDL_GetWindowSize(window, &w, &h);

    surface = SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);
    if (!surface) {
		SDL_SetError("SDL_CreateRGBSurface() FAILED");
        return -1;
    }

    /* Save the info and return! */
    data->surface = surface;
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

int  PS4_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
//	D_FN();

    SDL_Surface *surface;

    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    surface = data->surface;
    if (!surface) {
        return SDL_SetError("Couldn't find framebuffer surface for window");
    }
    /* Send the data to the display */



	uint32_t * pDst = (uint32_t*)CurrentBuffer(_this);
	if (!pDst) {
		SDL_SetError("GOT INVALID FB PTR");
		return -1;
	}
	memset(pDst, 0, BufferSize(_this));

	/// annoyances ...  calculate actual window coords and clip

	PS4_VideoData;	// videoData *
	uint32_t xOffs = window->x, yOffs = window->y;
	uint32_t drawW = window->w, drawH = window->h;

	xOffs = (xOffs < videoData->width) ? xOffs : videoData->width - 1;
	yOffs = (yOffs < videoData->height)? yOffs : videoData->height- 1;

	drawW = ((xOffs + drawW) <= videoData->width) ? drawW : (videoData->width - xOffs);
	drawH = ((yOffs + drawH) <= videoData->height)? drawH : (videoData->height- yOffs);

	for (uint32_t y = 0; y < drawH; y++)
		memcpy(&pDst[videoData->width * (yOffs+y) + xOffs], &((uint8_t*)surface->pixels)[y*surface->pitch], sizeof(uint32_t)*drawW);	// surface->pitch);

	SubmitFlip(_this);

    return 0;
}

void PS4_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
	D_FN();

    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    SDL_FreeSurface(data->surface);
    data->surface = NULL;
}












void PS4_PumpEvents(_THIS)
{
//	D_FN();

	return;
}



int PS4_CreateWindow(_THIS, SDL_Window * window)
{
	D_FN();
    SDL_WindowData *wdata;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return SDL_OutOfMemory();
    }
	//PS4_VideoData;	// videoData *


    /* Setup driver data for this window */
	if (window) {
		window->driverdata = wdata;

		if (_this->num_displays > 0) {
			_this->displays[0].fullscreen_window = window;
			window->fullscreen_mode = _this->displays[0].current_mode;
		}
	}
    /* Window has been successfully created */
    return 0;
}

int PS4_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
	D_FN();
    return SDL_Unsupported();
}

void PS4_SetWindowTitle(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
	D_FN();
}
void PS4_SetWindowPosition(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_SetWindowSize(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_ShowWindow(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_HideWindow(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_RaiseWindow(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_MaximizeWindow(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_MinimizeWindow(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_RestoreWindow(_THIS, SDL_Window * window)
{
	D_FN();
}
void PS4_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
	D_FN();
}
void PS4_DestroyWindow(_THIS, SDL_Window * window)
{
	D_FN();
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
#if 0
SDL_bool PS4_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}
#endif


/* TO Write Me */
SDL_bool PS4_HasScreenKeyboardSupport(_THIS)
{
	D_FN();
    return SDL_FALSE;
}
void PS4_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
	D_FN();
}
void PS4_HideScreenKeyboard(_THIS, SDL_Window *window)
{
	D_FN();
}
SDL_bool PS4_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
	D_FN();
    return SDL_FALSE;
}


#endif /* SDL_VIDEO_DRIVER_PS4 */

/* vi: set ts=4 sw=4 expandtab: */
