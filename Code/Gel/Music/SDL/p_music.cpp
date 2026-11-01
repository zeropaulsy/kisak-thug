#include <Core/macros.h>
#include <Core/Defines.h>
#include <Core/allmath.h>
#include <Core/crc.h>

#include "p_music.h"

#include "usersoundtrack.h"
#include "BinkAudio.h"

#include <Sys/Config/config.h>
#include <Gel/SoundFX/soundfx.h>
#include <Gel/Scripting/checksum.h>

#include <Gel/SoundFX/SDL/Mixer.h>

#include <Sys/File/filesys.h>
#include <filesystem>

#include <mutex>
#include <thread>
#include <functional>
#include <fstream>

#include <Shlwapi.h>

namespace File
{
static std::filesystem::path ModulePath()
{
	char		nameConversionBuffer[256];
	GetModuleFileNameA(NULL, nameConversionBuffer, 255);
	PathRemoveFileSpecA(nameConversionBuffer);

	return nameConversionBuffer;
}
// The child of a folder called name, matched ignoring case like every other game file. Missing: name as given.
static std::filesystem::path Child(const std::filesystem::path &parent, const char *name)
{
	std::error_code error;
	for (auto &i : std::filesystem::directory_iterator(parent, error))
	{
		const std::string found = i.path().filename().string();
		if (found.size() == strlen(name) && std::equal(found.begin(), found.end(), name,
			[](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); }))
			return i.path();
	}
	return parent / name;
}
static std::filesystem::path DataPath()
{
	return Child(ModulePath(), "Data");
}
}

namespace Pcm
{
	// Volume constants
	static constexpr float MUSIC_VOLUME = 1.0f;
	static constexpr float STREAM_VOLUME = 0.5f; // Loose stream files are too loud, ripped from PS2
	static constexpr double vOUTPUT_RATE = 48000.0;

	// Music decoder using miniaudio
	class Streamer
	{
		private:
			char *buffer = nullptr;
			size_t buffer_size = 0;

			bool paused = true;
			bool playing = true;
			bool m_held = false; // preloaded: unpausing must not start it, only starting it does

			float volume[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

		public:
			virtual ~Streamer() { free(buffer); }

			// Mix runs on the audio thread, and the game heap is not thread safe: streamers and their buffers use malloc
			static void *operator new(size_t size) { return malloc(size); }
			static void operator delete(void *p) { free(p); }

			virtual void Rewind() = 0;
			virtual size_t Stream(char *p, size_t bytes) = 0;
			virtual void SetPitch(float percent) { (void)percent; }
			virtual float GetStreamGain() const { return STREAM_VOLUME; }

			size_t Mix(char *out, size_t bytes)
			{
				// If stopped or paused, return 0
				if (!playing || paused || m_held)
					return 0;

				// Read in buffer
				if (buffer_size < bytes)
				{
					char *grown = (char*)realloc(buffer, bytes);
					if (grown == nullptr)
						return 0;
					buffer = grown;
					buffer_size = bytes;
				}

				size_t read = Stream(buffer, bytes);
				if (read < bytes)
					playing = false;

				// Accumulate
				float *a = (float*)out;
				float *b = (float*)buffer;
				for (size_t i = 0; i < read / sizeof(float) / 5; i++)
				{
					for (int c = 0; c < 5; c++)
						*a++ += *b++ * volume[c];
				}

				return read;
			}

			void Play()
			{
				paused = false;
			}

			void Pause()
			{
				paused = true;
			}

			void Hold()
			{
				m_held = true;
			}

			void Start()
			{
				m_held = false;
				paused = false;
			}

			void Stop()
			{
				Rewind();
				paused = true;
				playing = true;
			}

			bool IsPaused() const { return paused; }
			bool IsPlaying() const { return playing; }

			void SetVolume(float *coefs)
			{
				for (int i = 0; i < 5; i++)
					volume[i] = coefs[i];
			}
	};

	class MusicDecoder : public Streamer
	{
		private:
			ma_decoder decoder = {};
			std::ifstream file;

		public:
			MusicDecoder(std::filesystem::path path) : file(path, std::ios::binary)
			{
				if (!file.is_open())
					return;

				ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 5, 48000);
				ma_decoder_init(
					[](ma_decoder *pDecoder, void *pBufferOut, size_t bytesToRead, size_t *pBytesRead) -> ma_result
					{
						MusicDecoder *self = (MusicDecoder*)pDecoder->pUserData;
						self->file.read((char *)pBufferOut, bytesToRead);
						if (pBytesRead != nullptr)
							*pBytesRead = (size_t)self->file.gcount();
						return MA_SUCCESS;
					},
					[](ma_decoder *pDecoder, ma_int64 byteOffset, ma_seek_origin origin) -> ma_result
					{
						MusicDecoder *self = (MusicDecoder*)pDecoder->pUserData;
						switch (origin)
						{
							case ma_seek_origin_start:
								self->file.seekg(byteOffset, std::ios::beg);
								break;
							case ma_seek_origin_current:
								self->file.seekg(byteOffset, std::ios::cur);
								break;
							case ma_seek_origin_end:
								self->file.seekg(byteOffset, std::ios::end);
								break;
						}
						return MA_SUCCESS;
					},
					this,
					&config,
					&decoder
				);
			}

			~MusicDecoder()
			{
				if (!file.is_open())
					return;
				
				// Close decoder
				ma_decoder_uninit(&decoder);
			}

		private:
			void Rewind() override
			{
				if (!file.is_open())
					return;

				ma_decoder_seek_to_pcm_frame(&decoder, 0);
			}

			size_t Stream(char *p, size_t bytes) override
			{
				if (!file.is_open())
					return 0;

				// Read from decoder
				ma_uint64 frames_read = 0;
				ma_decoder_read_pcm_frames(&decoder, p, bytes / (sizeof(float) * 5), &frames_read);
				return (size_t)(frames_read * (sizeof(float) * 5));
			}
	};

	// Retail music (stereo, kept to the front speakers) and speech (mono, panned by the stream volume),
	// resampled to the mixer rate. A rate equal to the mixer's passes through untouched.
	class CBinkDecoder : public Streamer
	{
		public:
			CBinkDecoder(const std::filesystem::path &path, unsigned int fileOffset = 0)
				: m_opened(m_bink.Open(path.string().c_str(), fileOffset))
			{
				if (m_opened)
				{
					m_rate_step = m_bink.GetSampleRate() / vOUTPUT_RATE;
					m_step = m_rate_step;
				}
			}

			bool				IsOpen() const { return m_opened; }

			void SetPitch(float percent) override
			{
				if (percent > 0.0f)
				{
					m_step = m_rate_step * percent / 100.0f;
				}
			}

			// Retail streams are mastered quieter than music, unlike the PS2 rips STREAM_VOLUME is for
			float GetStreamGain() const override { return 1.0f; }

		private:
			void Rewind() override
			{
				if (m_opened)
				{
					m_bink.Rewind();
				}
				m_primed = false;
			}

			size_t Stream(char *p, size_t bytes) override
			{
				if (!m_opened)
				{
					return 0;
				}

				if (!m_primed)
				{
					if (m_bink.Read(m_current, 1) != 1 || m_bink.Read(m_next, 1) != 1)
					{
						return 0;
					}
					m_position = 0.0;
					m_primed = true;
				}

				const size_t num_frames = bytes / (sizeof(float) * 5);
				const bool stereo = (m_bink.GetNumChannels() == 2);
				float *p_out = (float*)p;
				for (size_t i = 0; i < num_frames; i++)
				{
					while (m_position >= 1.0)
					{
						m_current[0] = m_next[0];
						m_current[1] = m_next[1];
						if (m_bink.Read(m_next, 1) != 1)
						{
							return i * sizeof(float) * 5;
						}
						m_position -= 1.0;
					}

					const float frac = (float)m_position;
					const float first = m_current[0] + (m_next[0] - m_current[0]) * frac;
					if (stereo)
					{
						p_out[0] = first;
						p_out[1] = m_current[1] + (m_next[1] - m_current[1]) * frac;
						p_out[2] = 0.0f;
						p_out[3] = 0.0f;
						p_out[4] = 0.0f;
					}
					else
					{
						for (int c = 0; c < 5; c++)
						{
							p_out[c] = first;
						}
					}
					p_out += 5;
					m_position += m_step;
				}
				return num_frames * sizeof(float) * 5;
			}

			CBinkAudio			m_bink;
			bool				m_opened;
			double				m_rate_step = 1.0;			// source frames per output frame, at normal pitch
			double				m_step = 1.0;
			double				m_position = 0.0;			// between m_current and m_next
			float				m_current[2] = {};
			float				m_next[2] = {};
			bool				m_primed = false;
	};

	static Streamer *OpenMusic(const std::filesystem::path &path)
	{
		CBinkDecoder *bink = new CBinkDecoder(path);
		if (bink->IsOpen())
			return bink;
		delete bink;
		return new MusicDecoder(path);
	}

	// Mixing streams
	Streamer *music_stream = nullptr;
	Streamer *stream_stream[NUM_STREAMS] = {};

	// Mixer
	void PCMAudio_Mix(char *buffer, size_t size)
	{
		// Mix music
		if (music_stream != nullptr)
			music_stream->Mix(buffer, size);

		// Mix streams
		for (auto &i : stream_stream)
			if (i != nullptr)
				i->Mix(buffer, size);
	}

	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	static std::unordered_map<uint32, std::filesystem::path> music_paths;
	static std::unordered_map<uint32, std::filesystem::path> stream_paths;

	// Retail streams, packed in pcm.wad. pcm.dat: count, then checksum, offset, length, sorted by checksum.
	static uint32 *sp_wad_index = nullptr;
	static uint32 s_wad_entries = 0;
	static std::filesystem::path s_wad_path;

	////////////////////////////////////////////////////////////////////////
	static void s_load_wad_index(const std::filesystem::path &folder)
	{
		std::FILE *p_file = std::fopen(File::Child(folder, "pcm.dat").string().c_str(), "rb");
		if (p_file == nullptr)
		{
			return;
		}

		uint32 entries = 0;
		if (std::fread(&entries, sizeof(entries), 1, p_file) == 1 && entries < 0x100000)
		{
			uint32 *p_index = (uint32*)malloc(entries * 3 * sizeof(uint32));
			if (p_index != nullptr && std::fread(p_index, sizeof(uint32) * 3, entries, p_file) == entries)
			{
				sp_wad_index = p_index;
				s_wad_entries = entries;
				s_wad_path = File::Child(folder, "pcm.wad");
			}
			else
			{
				free(p_index);
			}
		}
		std::fclose(p_file);
	}

	////////////////////////////////////////////////////////////////////////
	static bool s_find_in_wad(uint32 checksum, uint32 *p_offset)
	{
		uint32 low = 0;
		uint32 high = s_wad_entries;
		while (low < high)
		{
			const uint32 middle = low + (high - low) / 2;
			const uint32 *p_entry = sp_wad_index + middle * 3;
			if (p_entry[0] == checksum)
			{
				*p_offset = p_entry[1];
				return true;
			}
			if (p_entry[0] < checksum)
			{
				low = middle + 1;
			}
			else
			{
				high = middle;
			}
		}
		return false;
	}

	static std::unordered_map<uint32, std::filesystem::path> Index(std::filesystem::path path)
	{
		if (!std::filesystem::exists(path))
			return {};

		std::unordered_map<uint32, std::filesystem::path> paths;
		for (auto &i : std::filesystem::recursive_directory_iterator(path))
		{
			if (i.is_regular_file())
			{
				std::string name = i.path().stem().string();
				//uint32 checksum = Crc::GenerateCRCFromString(name.c_str());
				uint32 checksum;
				if (sscanf(name.c_str(), "%x", &checksum) != 1)
					continue; // pcm.dat and pcm.wad

				//uint32 checksum = std::stoi(name.c_str(), 0, 16);
				paths[checksum] = std::filesystem::relative(i.path(), File::DataPath());
			}
		}
		return paths;
	}

	void PCMAudio_Init( void )
	{
		// Initialize mixer
		Audio::Init();

		// Index music and streams
		const std::filesystem::path streams = File::Child(File::DataPath(), "streams");
		music_paths = Index(File::Child(File::DataPath(), "music"));
		stream_paths = Index(streams);

		// Retail music is in streams/music. Files in Data/music win, and these are not streams.
		for (auto &i : Index(File::Child(streams, "music")))
		{
			music_paths.insert(i);
			stream_paths.erase(i.first);
		}

		s_load_wad_index(File::Child(streams, "pcm"));
	}



	/******************************************************************/
	/*                                                                */
	/* Call every frame to make sure music and stream buffers are	  */
	/* loaded and current status is checked each frame...			  */
	/*                                                                */
	/******************************************************************/
	int PCMAudio_Update( void )
	{
		return 0;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	void PCMAudio_StopMusic( bool waitPlease )
	{
		(void)waitPlease;

		Audio::Lock();

		if (music_stream != nullptr)
		{
			delete music_stream;
			music_stream = nullptr;
		}

		Audio::Unlock();
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	void PCMAudio_StopStream( int whichStream, bool waitPlease )
	{
		(void)waitPlease;

		Audio::Lock();

		if (stream_stream[whichStream] != nullptr)
		{
			delete stream_stream[whichStream];
			stream_stream[whichStream] = nullptr;
		}

		Audio::Unlock();
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	void PCMAudio_StopStreams( void )
	{
		Audio::Lock();

		for (auto &i : stream_stream)
		{
			if (i != nullptr)
			{
				delete i;
				i = nullptr;
			}
		}

		Audio::Unlock();
	}


	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/

	// This is temp code for the preload streams.  It just calls the normal one.

	// static uint32 sPreLoadChecksum[NUM_STREAMS];
	// static uint32 sPreLoadMusicChecksum;

	/******************************************************************/
	/*                                                                */
	/* Get a stream loaded into a buffer, but don't play yet		  */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PreLoadStream( uint32 checksum, int whichStream )
	{
		PCMAudio_PlayStream(checksum, whichStream, nullptr, 0.0f, true);
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/* Returns true if preload done. Assumes that caller is calling	  */
	/* this on a preloaded, but not yet played, stream. The results	  */
	/* are meaningless otherwise.									  */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PreLoadStreamDone( int whichStream )
	{
		(void)whichStream;
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/* Tells a preloaded stream to start playing.					  */
	/* Must call PCMAudio_PreLoadStreamDone() first to guarantee that */
	/* it starts immediately.										  */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_StartPreLoadedStream( int whichStream, Sfx::sVolume *p_volume, float pitch )
	{
		PCMAudio_SetStreamVolume(p_volume, whichStream);

		Audio::Lock();

		if (stream_stream[whichStream] != nullptr)
		{
			stream_stream[whichStream]->SetPitch(pitch);
			stream_stream[whichStream]->Start();
		}

		Audio::Unlock();
		
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PreLoadMusicStream( uint32 checksum )
	{
		auto it = music_paths.find(checksum);
		if (it == music_paths.end())
			return false;

		Audio::Lock();

		if (music_stream != nullptr)
			delete music_stream;
		music_stream = OpenMusic(File::DataPath() / it->second);

		Audio::Unlock();
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PreLoadMusicStreamDone( void )
	{
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_StartPreLoadedMusicStream( void )
	{
		Audio::Lock();
		if (music_stream != nullptr)
			music_stream->Play();
		Audio::Unlock();
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	int PCMAudio_GetMusicStatus( void )
	{
		Audio::Lock();

		int status;
		if (music_stream == nullptr)
			status = PCM_STATUS_FREE;
		else if (music_stream->IsPlaying())
			status = PCM_STATUS_PLAYING;
		else
			status = PCM_STATUS_FREE;

		Audio::Unlock();
		return status;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	int PCMAudio_GetStreamStatus( int whichStream )
	{
		Audio::Lock();

		int status;
		Streamer *stream = stream_stream[whichStream];
		if (stream == nullptr)
			status = PCM_STATUS_FREE;
		else if (stream->IsPlaying())
			status = PCM_STATUS_PLAYING;
		else
			status = PCM_STATUS_FREE;
		
		Audio::Unlock();
		return status;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	void PCMAudio_Pause( bool pause, int ch )
	{
		(void)pause;
		(void)ch;

		Audio::Lock();

		if (ch == MUSIC_CHANNEL)
		{
			if (music_stream != nullptr)
			{
				if (pause)
					music_stream->Pause();
				else
					music_stream->Play();
			}
		}
		else
		{
			for (auto &i : stream_stream)
			{
				if (i != nullptr)
				{
					if (pause)
						i->Pause();
					else
						i->Play();
				}
			}
		}

		Audio::Unlock();
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_TrackExists( const char *pTrackName, int ch )
	{
		(void)pTrackName;
		(void)ch;

		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_LoadMusicHeader( const char *nameOfFile )
	{
		(void)nameOfFile;
		// Legacy call left over from PS2 code.
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_LoadStreamHeader( const char *nameOfFile )
	{
		(void)nameOfFile;
		// Legacy call left over from PS2 code.
		return true;
	}


	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	uint32 PCMAudio_FindNameFromChecksum( uint32 checksum, int ch )
	{
		if (ch != EXTRA_CHANNEL)
			return 0;

		uint32 offset;
		if (stream_paths.find(checksum) == stream_paths.end() && !s_find_in_wad(checksum, &offset))
			return 0;

		return checksum;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_SetStreamVolume( Sfx::sVolume *p_volume, int whichStream )
	{
		Audio::Lock();

		if (stream_stream[whichStream] != nullptr)
		{
			Spt::SingletonPtr< Sfx::CSfxManager > sfx_manager;

			float coef[5] = {};
			const float stream_gain = stream_stream[whichStream]->GetStreamGain();

			switch (p_volume->GetVolumeType())
			{
				case Sfx::VOLUME_TYPE_5_CHANNEL_DOLBY5_1:
				{
					// NOTE: sVolume is in the order of FL, C, FR, BL, BR
					// So we need to swap C and FR to match what the mixer expects
					coef[0] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(0)) / 100.0f * stream_gain;
					coef[1] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(1)) / 100.0f * stream_gain;
					coef[2] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(4)) / 100.0f * stream_gain;
					coef[3] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(2)) / 100.0f * stream_gain;
					coef[4] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(3)) / 100.0f * stream_gain;
					break;
				}
				case Sfx::VOLUME_TYPE_2_CHANNEL_DOLBYII:
				{
					coef[0] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(0)) / 100.0f * stream_gain;
					coef[1] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(1)) / 100.0f * stream_gain;
					break;
				}
				case Sfx::VOLUME_TYPE_BASIC_2_CHANNEL:
				{
					coef[0] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(0)) / 100.0f * stream_gain;
					coef[1] = PERCENT(sfx_manager->GetMainVolume(), p_volume->GetChannelVolume(1)) / 100.0f * stream_gain;
					break;
				}
			}

			// Scripts ask for over 100%. Clamp as sound effects do; the sign is DolbyII phase.
			for (int i = 0; i < 5; i++)
			{
				if (coef[i] > 1.0f)
				{
					coef[i] = 1.0f;
				}
				else if (coef[i] < -1.0f)
				{
					coef[i] = -1.0f;
				}
			}

			stream_stream[whichStream]->SetVolume(coef);
		}

		Audio::Unlock();
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	int PCMAudio_SetMusicVolume( float volume )
	{
		(void)volume;

		Audio::Lock();

		if (music_stream != nullptr)
		{
			float coef[5] = {};
			coef[0] = volume / 100.0f;
			coef[1] = volume / 100.0f;
			coef[2] = volume / 100.0f;
			coef[3] = volume / 100.0f;
			coef[4] = volume / 100.0f;
			music_stream->SetVolume(coef);
		}

		Audio::Unlock();
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_SetStreamPitch( float fPitch, int whichStream )
	{
		Audio::Lock();

		if (stream_stream[whichStream] != nullptr)
		{
			stream_stream[whichStream]->SetPitch(fPitch);
		}

		Audio::Unlock();
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PlayMusicTrack( uint32 checksum )
	{
		(void)checksum;
		Dbg_Assert(0);
		return false;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PlayMusicTrack( const char *filename )
	{
		// Remove path from filename
		const char *last_slash = strrchr(filename, '\\');
		if (last_slash != nullptr)
			filename = last_slash + 1;

		// Find music path from checksum
		uint32 hash = Crc::GenerateCRCFromString(filename);
		auto it = music_paths.find(Crc::GenerateCRCFromString(filename));
		if (it == music_paths.end())

			return false;

		Audio::Lock();
		
		// Play song
		if (music_stream != nullptr)
			delete music_stream;

		music_stream = OpenMusic(File::DataPath() / it->second);
		music_stream->Play();

		Audio::Unlock();
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PlaySoundtrackMusicTrack(size_t soundtrack, size_t track )
	{
		Audio::Lock();

		// Play song
		if (music_stream != nullptr)
			delete music_stream;

		const UserSoundtrack::Soundtracks &soundtracks = UserSoundtrack::GetSoundtracks();
		music_stream = new MusicDecoder(soundtracks.at(soundtrack).tracks.at(track).path);
		music_stream->Play();

		Audio::Unlock();
		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_StartStream( int whichStream )
	{
		Audio::Lock();

		if (stream_stream[whichStream] != nullptr)
			stream_stream[whichStream]->Start();

		Audio::Unlock();

		return true;
	}



	/******************************************************************/
	/*                                                                */
	/*                                                                */
	/******************************************************************/
	bool PCMAudio_PlayStream( uint32 checksum, int whichStream, Sfx::sVolume *p_volume, float fPitch, bool preload )
	{
		// Open before taking the lock: the audio thread shouldn't wait on the disk. Loose files win over the wad.
		Streamer *p_new_stream = nullptr;
		uint32 wad_offset;
		auto it = stream_paths.find(checksum);
		if (it != stream_paths.end())
		{
			p_new_stream = OpenMusic(File::DataPath() / it->second);
		}
		else if (s_find_in_wad(checksum, &wad_offset))
		{
			CBinkDecoder *p_bink = new CBinkDecoder(s_wad_path, wad_offset);
			if (!p_bink->IsOpen())
			{
				delete p_bink;
				return false;
			}
			p_new_stream = p_bink;
		}
		else
		{
			return false;
		}

		p_new_stream->SetPitch(fPitch);

		Audio::Lock();

		// Play stream
		if (stream_stream[whichStream] != nullptr)
			delete stream_stream[whichStream];

		stream_stream[whichStream] = p_new_stream;
		if (preload)
		{
			p_new_stream->Hold();
		}
		else
		{
			p_new_stream->Start();
		}

		if (p_volume != nullptr)
			PCMAudio_SetStreamVolume(p_volume, whichStream);

		Audio::Unlock();

		return true;
	}

	// User soundtracks
	size_t GetNumSoundtracks()
	{
		const UserSoundtrack::Soundtracks &soundtracks = UserSoundtrack::GetSoundtracks();
		return soundtracks.size();
	}

	size_t GetSoundtrackNumSongs(size_t i)
	{
		const UserSoundtrack::Soundtracks &soundtracks = UserSoundtrack::GetSoundtracks();
		if(soundtracks.empty())
			return 0;

		return soundtracks.at(i).tracks.size();
	}

	const char *GetSoundtrackName(size_t i)
	{
		const UserSoundtrack::Soundtracks &soundtracks = UserSoundtrack::GetSoundtracks();
		if (soundtracks.empty())
			return "";

		return soundtracks.at(i).name.c_str();
	}

} // namespace PCM
