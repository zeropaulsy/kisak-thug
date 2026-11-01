/*
 * Bink Audio decoder, reduced from FFmpeg's libavcodec/binkaudio.c (kept alongside as Gel/Music/binkaudio.c)
 * Copyright (c) 2007-2011 Peter Ross (pross@xvid.org)
 * Copyright (c) 2009 Daniel Verkamp (daniel@drv.nu)
 *
 * This file is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser
 * General Public License as published by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for details.
 */

#include "BinkAudio.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace Pcm
{

namespace
{

const uint32_t	vFLAG_STEREO = 0x2000;
const uint32_t	vFLAGS_REQUIRED = 0x5000;			// 16 bit, DCT

const int		vMAX_FRAME_LEN = 2048;				// transform size at 44.1 kHz and up
const int		vMAX_OVERLAP_LEN = vMAX_FRAME_LEN / 16;
const int		vMAX_CHANNELS = 2;
const int		vMAX_BANDS = 25;

const int		vWMA_CRITICAL_FREQS[vMAX_BANDS] = {
	100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000,
	2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 24500,
};
const unsigned char	vRLE_LENGTHS[16] = { 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64 };

const double	vPI = 3.14159265358979323846;

////////////////////////////////////////////////////////////////////////
uint32_t read_le32(const unsigned char *p_data)
{
	return (uint32_t)p_data[0] | ((uint32_t)p_data[1] << 8) | ((uint32_t)p_data[2] << 16) | ((uint32_t)p_data[3] << 24);
}

} // namespace

struct CBinkAudio::STables
{
	float				quant[96];
	int					bands[vMAX_BANDS + 1];

	// Inverse DCT by a complex FFT of the same size
	float				dct_cos[vMAX_FRAME_LEN], dct_sin[vMAX_FRAME_LEN];
	float				fft_cos[vMAX_FRAME_LEN / 2], fft_sin[vMAX_FRAME_LEN / 2];
	int					bit_reverse[vMAX_FRAME_LEN];
	float				re[vMAX_FRAME_LEN], im[vMAX_FRAME_LEN];

	float				coeffs[vMAX_FRAME_LEN];
	float				channel[vMAX_CHANNELS][vMAX_FRAME_LEN];
	float				previous[vMAX_CHANNELS][vMAX_OVERLAP_LEN];
	float				output[vMAX_FRAME_LEN * vMAX_CHANNELS];
};

////////////////////////////////////////////////////////////////////////
CBinkAudio::CBinkAudio()
	: mp_file(nullptr), m_file_offset(0), mp_frame_index(nullptr), m_num_frames(0), m_next_frame(0), mp_packet(nullptr),
	  m_packet_capacity(0), m_packet_bits(0), m_bit_pos(0), mp_tables(nullptr), m_sample_rate(0), m_num_channels(0),
	  m_frame_len(0), m_overlap_len(0), m_block_frames(0), m_num_bands(0), m_root(0.0f), m_first_block(true),
	  m_out_pos(0), m_out_count(0)
{
}

////////////////////////////////////////////////////////////////////////
CBinkAudio::~CBinkAudio()
{
	if (mp_file != nullptr)
	{
		std::fclose(mp_file);
	}
	std::free(mp_frame_index);
	std::free(mp_packet);
	std::free(mp_tables);
}

////////////////////////////////////////////////////////////////////////
bool CBinkAudio::Open(const char *p_path, unsigned int fileOffset)
{
	mp_file = std::fopen(p_path, "rb");
	if (mp_file == nullptr)
	{
		return false;
	}
	if (std::fseek(mp_file, (long)fileOffset, SEEK_SET) != 0)
	{
		return false;
	}
	m_file_offset = fileOffset;

	// Container: 44 byte header, then per audio track a max size, rate + flags and id, then the frame offsets
	unsigned char header[56];
	if (std::fread(header, 1, sizeof(header), mp_file) != sizeof(header) || std::memcmp(header, "BIKi", 4) != 0)
	{
		return false;
	}

	const uint32_t frames = read_le32(header + 0x08);
	const uint32_t audio_tracks = read_le32(header + 0x28);
	const uint32_t rate_and_flags = read_le32(header + 0x30);
	const uint32_t flags = rate_and_flags >> 16;
	m_sample_rate = rate_and_flags & 0xffff;
	if (frames == 0 || frames > 0x100000 || audio_tracks != 1 || m_sample_rate == 0 ||
		(flags & vFLAGS_REQUIRED) != vFLAGS_REQUIRED)
	{
		return false;
	}
	m_num_channels = (flags & vFLAG_STEREO) ? 2 : 1;

	mp_frame_index = (unsigned int *)std::malloc((frames + 1) * sizeof(unsigned int));
	if (mp_frame_index == nullptr)
	{
		return false;
	}
	unsigned char entry[4];
	for (uint32_t i = 0; i <= frames; i++)
	{
		if (std::fread(entry, 1, 4, mp_file) != 4)
		{
			return false;
		}
		mp_frame_index[i] = read_le32(entry) & ~1u;	// bit 0 marks a video keyframe
	}
	m_num_frames = frames;

	mp_tables = (STables *)std::malloc(sizeof(STables));
	if (mp_tables == nullptr)
	{
		return false;
	}

	int log2_len = 11;
	if (m_sample_rate < 22050)
	{
		log2_len = 9;
	}
	else if (m_sample_rate < 44100)
	{
		log2_len = 10;
	}
	m_frame_len = 1 << log2_len;
	m_overlap_len = m_frame_len / 16;
	m_block_frames = m_frame_len - m_overlap_len;

	m_root = (float)(m_frame_len / (std::sqrt((double)m_frame_len) * 32768.0));
	for (int i = 0; i < 96; i++)
	{
		mp_tables->quant[i] = std::exp(i * 0.15289164787221953823f) * m_root;
	}

	const int half_rate = (m_sample_rate + 1) / 2;
	for (m_num_bands = 1; m_num_bands < vMAX_BANDS; m_num_bands++)
	{
		if (half_rate <= vWMA_CRITICAL_FREQS[m_num_bands - 1])
		{
			break;
		}
	}
	mp_tables->bands[0] = 2;
	for (int i = 1; i < m_num_bands; i++)
	{
		mp_tables->bands[i] = (vWMA_CRITICAL_FREQS[i - 1] * m_frame_len / half_rate) & ~1;
	}
	mp_tables->bands[m_num_bands] = m_frame_len;

	for (int i = 0; i < m_frame_len; i++)
	{
		int reversed = 0;
		for (int b = 0; b < log2_len; b++)
		{
			if (i & (1 << b))
			{
				reversed |= 1 << (log2_len - 1 - b);
			}
		}
		mp_tables->bit_reverse[i] = reversed;

		mp_tables->dct_cos[i] = (float)std::cos(vPI * i / (2.0 * m_frame_len));
		mp_tables->dct_sin[i] = (float)std::sin(vPI * i / (2.0 * m_frame_len));
	}
	for (int i = 0; i < m_frame_len / 2; i++)
	{
		mp_tables->fft_cos[i] = (float)std::cos(2.0 * vPI * i / m_frame_len);
		mp_tables->fft_sin[i] = (float)std::sin(2.0 * vPI * i / m_frame_len);
	}

	Rewind();
	return true;
}

////////////////////////////////////////////////////////////////////////
void CBinkAudio::Rewind()
{
	m_next_frame = 0;
	m_packet_bits = 0;
	m_bit_pos = 0;
	m_first_block = true;
	m_out_pos = 0;
	m_out_count = 0;
}

////////////////////////////////////////////////////////////////////////
// Bits are stored least significant first. Past the end of the packet is an error.
bool CBinkAudio::read_bits(int count, unsigned int *p_value)
{
	if (m_bit_pos + count > m_packet_bits)
	{
		return false;
	}

	unsigned int result = 0;
	for (int i = 0; i < count; i++, m_bit_pos++)
	{
		result |= (unsigned int)((mp_packet[m_bit_pos >> 3] >> (m_bit_pos & 7)) & 1) << i;
	}
	*p_value = result;
	return true;
}

////////////////////////////////////////////////////////////////////////
// x[n] = (2/N) * (X[0]/2 + sum of X[k] * cos(pi * k * (n + 1/2) / N)), the DCT-III FFmpeg uses
void CBinkAudio::inverse_dct(const float *p_in, float *p_out)
{
	STables *p_t = mp_tables;
	const int len = m_frame_len;

	// V[k] = (X[k] - i * X[N-k]) * e^(i*pi*k/2N), stored bit reversed for the FFT
	for (int k = 0; k < len; k++)
	{
		const float a = p_in[k];
		const float b = (k == 0) ? 0.0f : p_in[len - k];
		const int to = p_t->bit_reverse[k];
		p_t->re[to] = a * p_t->dct_cos[k] + b * p_t->dct_sin[k];
		p_t->im[to] = a * p_t->dct_sin[k] - b * p_t->dct_cos[k];
	}

	// Inverse FFT
	for (int size = 2; size <= len; size <<= 1)
	{
		const int half = size >> 1;
		const int step = len / size;
		for (int start = 0; start < len; start += size)
		{
			for (int j = 0; j < half; j++)
			{
				const float wr = p_t->fft_cos[j * step];
				const float wi = p_t->fft_sin[j * step];
				const int p = start + j;
				const int q = p + half;
				const float xr = p_t->re[q] * wr - p_t->im[q] * wi;
				const float xi = p_t->re[q] * wi + p_t->im[q] * wr;
				p_t->re[q] = p_t->re[p] - xr;
				p_t->im[q] = p_t->im[p] - xi;
				p_t->re[p] += xr;
				p_t->im[p] += xi;
			}
		}
	}

	// Undo the even/odd reordering
	const float scale = 1.0f / len;
	for (int n = 0; n < len / 2; n++)
	{
		p_out[2 * n] = p_t->re[n] * scale;
		p_out[2 * n + 1] = p_t->re[len - 1 - n] * scale;
	}
}

////////////////////////////////////////////////////////////////////////
bool CBinkAudio::next_packet()
{
	while (m_next_frame < m_num_frames)
	{
		const unsigned int frame = m_next_frame++;
		if (mp_frame_index[frame + 1] <= mp_frame_index[frame] + 4)
		{
			continue;
		}
		const size_t frame_size = mp_frame_index[frame + 1] - mp_frame_index[frame];

		unsigned char entry[4];
		if (std::fseek(mp_file, (long)(m_file_offset + mp_frame_index[frame]), SEEK_SET) != 0 ||
			std::fread(entry, 1, 4, mp_file) != 4)
		{
			return false;
		}

		// The audio packet comes first, video follows. Starts with its decoded size, which isn't needed.
		const size_t size = read_le32(entry);
		if (size <= 4 || size > frame_size - 4)
		{
			continue;
		}

		if (size > m_packet_capacity)
		{
			unsigned char *p_grown = (unsigned char *)std::realloc(mp_packet, size);
			if (p_grown == nullptr)
			{
				return false;
			}
			mp_packet = p_grown;
			m_packet_capacity = size;
		}
		if (std::fread(mp_packet, 1, size, mp_file) != size)
		{
			return false;
		}

		m_packet_bits = size * 8;
		m_bit_pos = 32;
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////
bool CBinkAudio::decode_block()
{
	STables *p_t = mp_tables;

	if (m_bit_pos >= m_packet_bits && !next_packet())
	{
		return false;
	}

	unsigned int v;
	if (!read_bits(2, &v))
	{
		return false;
	}

	for (int ch = 0; ch < m_num_channels; ch++)
	{
		float *p_coeffs = p_t->coeffs;

		for (int i = 0; i < 2; i++)
		{
			unsigned int power, mantissa, sign;
			if (!read_bits(5, &power) || !read_bits(23, &mantissa) || !read_bits(1, &sign))
			{
				return false;
			}
			const float f = std::ldexp((float)mantissa, (int)power - 23);
			p_coeffs[i] = (sign ? -f : f) * m_root;
		}

		float quant[vMAX_BANDS];
		for (int i = 0; i < m_num_bands; i++)
		{
			if (!read_bits(8, &v))
			{
				return false;
			}
			quant[i] = p_t->quant[v > 95 ? 95 : v];
		}

		int k = 0;
		float q = quant[0];
		int i = 2;
		while (i < m_frame_len)
		{
			int j;
			if (!read_bits(1, &v))
			{
				return false;
			}
			if (v)
			{
				if (!read_bits(4, &v))
				{
					return false;
				}
				j = i + vRLE_LENGTHS[v] * 8;
			}
			else
			{
				j = i + 8;
			}
			if (j > m_frame_len)
			{
				j = m_frame_len;
			}

			unsigned int width;
			if (!read_bits(4, &width))
			{
				return false;
			}

			if (width == 0)
			{
				std::memset(p_coeffs + i, 0, (j - i) * sizeof(float));
				i = j;
				while (p_t->bands[k] < i)
				{
					q = quant[k++];
				}
			}
			else
			{
				while (i < j)
				{
					if (p_t->bands[k] == i)
					{
						q = quant[k++];
					}

					unsigned int coeff;
					if (!read_bits((int)width, &coeff))
					{
						return false;
					}
					if (coeff)
					{
						if (!read_bits(1, &v))
						{
							return false;
						}
						p_coeffs[i] = v ? -q * coeff : q * coeff;
					}
					else
					{
						p_coeffs[i] = 0.0f;
					}
					i++;
				}
			}
		}

		p_coeffs[0] /= 0.5f;
		inverse_dct(p_coeffs, p_t->channel[ch]);
	}

	// Crossfade over the tail of the previous block
	for (int ch = 0; ch < m_num_channels; ch++)
	{
		const int count = m_overlap_len * m_num_channels;
		if (!m_first_block)
		{
			int j = ch;
			for (int i = 0; i < m_overlap_len; i++, j += m_num_channels)
			{
				p_t->channel[ch][i] = (p_t->previous[ch][i] * (count - j) + p_t->channel[ch][i] * j) / count;
			}
		}
		std::memcpy(p_t->previous[ch], &p_t->channel[ch][m_block_frames], m_overlap_len * sizeof(float));
	}
	m_first_block = false;

	for (int i = 0; i < m_block_frames; i++)
	{
		for (int ch = 0; ch < m_num_channels; ch++)
		{
			p_t->output[i * m_num_channels + ch] = p_t->channel[ch][i];
		}
	}
	m_out_pos = 0;
	m_out_count = m_block_frames;

	// Blocks start on a 32 bit boundary
	m_bit_pos = (m_bit_pos + 31) & ~(size_t)31;
	return true;
}

////////////////////////////////////////////////////////////////////////
size_t CBinkAudio::Read(float *p_out, size_t numFrames)
{
	size_t written = 0;
	while (written < numFrames)
	{
		if (m_out_pos == m_out_count && !decode_block())
		{
			break;
		}

		size_t n = m_out_count - m_out_pos;
		if (n > numFrames - written)
		{
			n = numFrames - written;
		}
		std::memcpy(p_out + written * m_num_channels, mp_tables->output + m_out_pos * m_num_channels,
			n * m_num_channels * sizeof(float));
		m_out_pos += n;
		written += n;
	}
	return written;
}

} // namespace Pcm
