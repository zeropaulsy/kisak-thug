#include "Mixer.h"

#include <SDL.h>

#include "p_sfx.h"
#include <Gel/Music/SDL/p_music.h>

namespace Audio
{
	// Audio device
	static SDL_AudioDeviceID s_audio_device = 0;

	// Audio callback
#ifdef __linux__
	// The mixers write SDL's 5 channel layout (FL FR LFE BL BR), which PulseAudio refuses to open. The device is opened
	// as 5.1 (FL FR C LFE BL BR) and each block widened, with the centre silent.
	static void AudioCallback5(void *userdata, Uint8 *stream, int len);
	void AudioCallback(void *userdata, Uint8 *stream, int len)
	{
		static float *s_mix = NULL;
		static int s_mix_frames = 0;
		const int frames = len / (int)(6 * sizeof(float));
		if (frames > s_mix_frames)
		{
			s_mix = (float*)realloc(s_mix, frames * 5 * sizeof(float));
			s_mix_frames = frames;
		}
		AudioCallback5(userdata, (Uint8*)s_mix, frames * 5 * (int)sizeof(float));
		float *out = (float*)stream;
		const float *in = s_mix;
		for (int i = 0; i < frames; ++i, out += 6, in += 5)
		{
			out[0] = in[0];
			out[1] = in[1];
			out[2] = 0.0f;
			out[3] = in[2];
			out[4] = in[3];
			out[5] = in[4];
		}
	}
	static void AudioCallback5(void *userdata, Uint8 *stream, int len)
#else
	void AudioCallback(void *userdata, Uint8 *stream, int len)
#endif
	{
		(void)userdata;

		// Clear the stream
		std::fill((float*)stream, (float*)(stream + len), 0.0f);

		// Mix music
		Pcm::PCMAudio_Mix((char*)stream, len);

		// Mix sound effects
		Sfx::MixSoundFX((char*)stream, len);
	}

	// Mixer API
	void Init()
	{
		// Check if audio is already initialised
		if (s_audio_device != 0)
			return;

		// Audio device spec
		SDL_AudioSpec s_wanted_spec = {};
		s_wanted_spec.format = AUDIO_F32SYS;
		s_wanted_spec.freq = 48000;
#ifdef __linux__
		s_wanted_spec.channels = 6; // see AudioCallback
#else
		s_wanted_spec.channels = 5;
#endif
		s_wanted_spec.samples = 1024;
		s_wanted_spec.callback = AudioCallback;

		// Open audio device
		s_audio_device = SDL_OpenAudioDevice(nullptr, 0, &s_wanted_spec, nullptr, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

		// printf("%s", SDL_GetError());

		// Start audio
#ifdef __linux__
		if (s_audio_device == 0) // no sound device is not fatal here: everything else keeps working
		{
			fprintf(stderr, "No audio: %s\n", SDL_GetError());
			return;
		}
#else
		Dbg_Assert(s_audio_device > 0);
#endif

		SDL_PauseAudioDevice(s_audio_device, 0);
	}

	void Lock()
	{
		// Lock audio device
		if (s_audio_device > 0)
			SDL_LockAudioDevice(s_audio_device);
	}

	void Unlock()
	{
		// Unlock audio device
		if (s_audio_device > 0)
			SDL_UnlockAudioDevice(s_audio_device);
	}
}
