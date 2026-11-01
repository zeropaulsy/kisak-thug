#pragma once

#include <cstddef>
#include <cstdio>

namespace Pcm
{

// Decodes the audio track of a retail .bik: Bink revision 'i', DCT audio, mono or stereo.
// Runs on the audio thread, so everything it owns comes from malloc, never the game heap.
class CBinkAudio
{
public:
						CBinkAudio();
						~CBinkAudio();

	// The movie starts at fileOffset, so this also opens an entry packed inside a wad.
	// False for anything that is not that format (including files that are not Bink at all).
	bool				Open(const char *p_path, unsigned int fileOffset = 0);
	void				Rewind();

	int					GetNumChannels() const		{ return m_num_channels; }
	unsigned int		GetSampleRate() const		{ return m_sample_rate; }

	// Writes interleaved floats, returns frames written. Short means the end of the track.
	size_t				Read(float *p_out, size_t numFrames);

private:
	struct STables;

	bool				decode_block();
	bool				next_packet();
	bool				read_bits(int count, unsigned int *p_value);
	void				inverse_dct(const float *p_in, float *p_out);

	std::FILE *			mp_file;
	unsigned int		m_file_offset;
	unsigned int *		mp_frame_index;
	unsigned int		m_num_frames;
	unsigned int		m_next_frame;

	unsigned char *		mp_packet;
	size_t				m_packet_capacity;
	size_t				m_packet_bits;
	size_t				m_bit_pos;

	STables *			mp_tables;					// transform tables and block buffers
	unsigned int		m_sample_rate;
	int					m_num_channels;
	int					m_frame_len;				// transform size
	int					m_overlap_len;
	int					m_block_frames;
	int					m_num_bands;
	float				m_root;
	bool				m_first_block;
	size_t				m_out_pos;
	size_t				m_out_count;
};

} // namespace Pcm
