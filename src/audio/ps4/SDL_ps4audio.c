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

#if SDL_AUDIO_DRIVER_PS4

#include <audioout.h>
#include <user_service.h>

#include "SDL_audio.h"
/*
#include "SDL_error.h"
#include "SDL_timer.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "../SDL_sysaudio.h"
*/
#include "SDL_ps4audio.h"

static int PS4AUDIO_Init(SDL_AudioDriverImpl * impl);

AudioBootStrap PS4AUDIO_bootstrap = {
	"ps4", "PS4 audio driver", PS4AUDIO_Init, 0
};

inline static 
Uint16 ps4_sample_size(Uint16 ssize)
{
	if (ssize >= 2048) return 2048;
	if (ssize >= 1792) return 1792;
	if (ssize >= 1536) return 1536;
	if (ssize >= 1280) return 1280;
	if (ssize >= 1024) return 1024;
	if (ssize >= 768)  return 768;
	if (ssize >= 512)  return 512;
	return 256;
}


/* The tag name used by PS4 audio */
#define PS4AUDIO_DRIVER_NAME         "ps4"

static int
PS4AUDIO_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
	int mixlen, i;
	this->hidden = (struct SDL_PrivateAudioData *)SDL_malloc(sizeof(*this->hidden));
	if (this->hidden == NULL) {
		return SDL_OutOfMemory();
	}
	SDL_zerop(this->hidden);

	int mono = (1 == this->spec.channels) ? 1 : 0;
	if (!mono) this->spec.channels = 2;

	uint8_t p_fmt = 0, p_attr = 0;
	switch (this->spec.format) {
	case AUDIO_S16LSB:	p_fmt = (mono) ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;		break;
	case AUDIO_F32LSB:	p_fmt = (mono) ? SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO : SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;	break;
	default: return SDL_SetError("Unsupported audio format");
	}

	this->spec.samples = ps4_sample_size(this->spec.samples);	// make sure it's specific granularity
	this->spec.freq = 48000;	// PS4 only does 48k, it's possible to get SDL to resample w.o libsamplerate??

	/* Update the fragment size as size in bytes. */
	SDL_CalculateAudioSpec(&this->spec);





	/* Allocate the mixing buffer.  Its size and starting address must
	   be a multiple of 64 bytes.  Our sample count is already a multiple of
	   64, so spec->size should be a multiple of 64 as well. */
	mixlen = this->spec.size * NUM_BUFFERS;
	this->hidden->rawbuf = (Uint8 *)memalign(64, mixlen);
	if (this->hidden->rawbuf == NULL) {
		return SDL_SetError("Couldn't allocate mixing buffer");
	}



	this->hidden->h_aout = sceAudioOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_AUDIO_OUT_PORT_TYPE_MAIN,
			0, this->spec.samples, 48000, ((p_fmt&255) | ((p_attr&15) << SCE_AUDIO_OUT_PARAM_ATTR_SHIFT)));
	if (this->hidden->h_aout <= 0) {
		free(this->hidden->rawbuf);
		this->hidden->rawbuf = NULL;
		return SDL_SetError("sceAudioOutOpen() failed!");
	}

	memset(this->hidden->rawbuf, 0, mixlen);
	for (i = 0; i < NUM_BUFFERS; i++) {
		this->hidden->mixbufs[i] = &this->hidden->rawbuf[i * this->spec.size];
	}

	this->hidden->next_buffer = 0;
	return 0;
}

static void PS4AUDIO_PlayDevice(_THIS)
{
	sceAudioOutOutput(this->hidden->h_aout, this->hidden->mixbufs[this->hidden->next_buffer]);

	this->hidden->next_buffer = (this->hidden->next_buffer + 1) % NUM_BUFFERS;
}

/* This function waits until it is possible to write a full sound buffer */
static void PS4AUDIO_WaitDevice(_THIS)
{
	sceAudioOutOutput(this->hidden->h_aout, NULL);	// Call with NULL to wait until play of current buffer is complete
}

static Uint8 *PS4AUDIO_GetDeviceBuf(_THIS)
{
	return this->hidden->mixbufs[this->hidden->next_buffer];
}

static void PS4AUDIO_CloseDevice(_THIS)
{
	sceAudioOutClose(this->hidden->h_aout);

	free(this->hidden->rawbuf);  /* this uses memalign(), not SDL_malloc(). */
	SDL_free(this->hidden);
}

static void PS4AUDIO_ThreadInit(_THIS)
{
#if 0
	/* Increase the priority of this audio thread by 1 to put it
	   ahead of other SDL threads. */
	SceUID thid;
	SceKernelThreadInfo status;
	thid = sceKernelGetThreadId();
	status.size = sizeof(SceKernelThreadInfo);
	if (sceKernelReferThreadStatus(thid, &status) == 0) {
		sceKernelChangeThreadPriority(thid, status.currentPriority - 1);
	}
#endif
}



static int PS4AUDIO_Init(SDL_AudioDriverImpl * impl)
{


	sceUserServiceInitialize(NULL);

	if (0 != sceAudioOutInit()) {
		SDL_SetError("sceAudioOutInit() failed!");
		return -1;
	}
/*
	void(*DetectDevices) (void);
	int(*OpenDevice) (_THIS, void *handle, const char *devname, int iscapture);
	void(*ThreadInit) (_THIS); // Called by audio thread at start 
	void(*ThreadDeinit) (_THIS); // Called by audio thread at end 
	void(*BeginLoopIteration)(_THIS);  // Called by audio thread at top of loop 
	void(*WaitDevice) (_THIS);
	void(*PlayDevice) (_THIS);
	int(*GetPendingBytes) (_THIS);
	Uint8 *(*GetDeviceBuf) (_THIS);
	int(*CaptureFromDevice) (_THIS, void *buffer, int buflen);
	void(*FlushCapture) (_THIS);
	void(*PrepareToClose) (_THIS);  //  Called between run and draining wait for playback devices 
	void(*CloseDevice) (_THIS);
	void(*LockDevice) (_THIS);
	void(*UnlockDevice) (_THIS);
	void(*FreeDeviceHandle) (void *handle);  // SDL is done with handle from SDL_AddAudioDevice() 
	void(*Deinitialize) (void);
*/
	/* Set the function pointers */
	impl->OpenDevice	= PS4AUDIO_OpenDevice;
	impl->PlayDevice	= PS4AUDIO_PlayDevice;
	impl->WaitDevice	= PS4AUDIO_WaitDevice;
	impl->GetDeviceBuf	= PS4AUDIO_GetDeviceBuf;
	impl->CloseDevice	= PS4AUDIO_CloseDevice;
	impl->ThreadInit	= PS4AUDIO_ThreadInit;



	/* PSP audio device */
	impl->OnlyHasDefaultOutputDevice = 1;
	/*
		impl->HasCaptureSupport = 1;
		impl->OnlyHasDefaultCaptureDevice = 1;
	*/
	/*
	impl->DetectDevices = DSOUND_DetectDevices;
	impl->Deinitialize  = DSOUND_Deinitialize;
	*/
	return 1;   /* this audio target is available. */
}




#endif // SDL_AUDIO_DRIVER_PS4