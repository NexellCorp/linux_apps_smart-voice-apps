#ifndef _WAV_FILE_H_
#define _WAV_FILE_H_

#include <stdio.h>
#include <stdarg.h>
#include <cstdint> /* include this header for uint64_t */

#include "util.h"

#if __cplusplus
extern "C" {
#endif

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

struct wav_header {
	uint32_t riff_id;
	uint32_t riff_sz;
	uint32_t riff_fmt;
	uint32_t fmt_id;
	uint32_t fmt_sz;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
	uint32_t data_id;
	uint32_t data_sz;
};

class CWAVFile {
	public:
	~CWAVFile(void) {
		if (m_hFile)
			Close();
	}

	CWAVFile(void) {
		m_hFile = NULL;
		m_SavePeriodBytes = 0;
		m_SavePeriodAvail = 0;
		m_Channels = 0;
		m_SampleRate = 0;
		m_SampleBits = 0;
		m_WriteBytes = 0;
		m_ReadBytes = 0;
		m_bWavFormat = true;

		memset(m_strName, 0, sizeof(m_strName));
	}

	CWAVFile(int save_period_size) {
		m_hFile = NULL;
		m_SavePeriodBytes = 0;
		m_SavePeriodAvail = 0;
		m_Channels = 0;
		m_SampleRate = 0;
		m_SampleBits = 0;
		m_WriteBytes = 0;
		m_ReadBytes = 0;
		m_bWavFormat = true;

		memset(m_strName, 0, sizeof(m_strName));
	}

	bool Open(int channels,
		int samplerate, int samplebits, char *path) {
		struct wav_header *header = &m_WavHeader;

		if (m_hFile) {
			LogE("Already opened (%s) ...\n", m_strName);
			return false;
		}

		Lock();

		m_hFile = fopen(path, "wb");
		if (!m_hFile) {
			LogE("Unable to create file '%s' !!!\n", path);
			UnLock();
			return false;
		}

		strcpy(m_strName, path);

		m_bWavFormat = true;

		if (channels == 0 || samplerate == 0 || samplebits == 0)
			m_bWavFormat = false;

		if (m_bWavFormat) {
			memset(header, 0, sizeof(struct wav_header));

			header->riff_id = ID_RIFF;
			header->riff_sz = 0;
			header->riff_fmt = ID_WAVE;
			header->fmt_id = ID_FMT;
			header->fmt_sz = 16;
			header->audio_format = FORMAT_PCM;
			header->num_channels = channels;
			header->sample_rate = samplerate;
			header->bits_per_sample = samplebits;
			header->byte_rate = (header->bits_per_sample / 8) *
				channels * samplerate;
			header->block_align = channels *
				(header->bits_per_sample / 8);
			header->data_id = ID_DATA;

			/* write defaut header */
			fseek(m_hFile, 0, SEEK_SET);
			fwrite(header, sizeof(struct wav_header), 1, m_hFile);

			/* leave enough room for header */
			fseek(m_hFile, sizeof(struct wav_header), SEEK_SET);
		}

		LogI(" %s open : %s %d ch, %d hz, %d bits\n",
			m_bWavFormat ? "WAV" : "RAW", m_strName, channels,
			samplerate, samplebits);

		m_Channels = channels;
		m_SampleRate = samplerate;
		m_SampleBits = samplebits;
		m_WriteBytes = 0;

		UnLock();
		return true;
	}

	bool Open(const char *path) {
		struct wav_header *header = &m_WavHeader;
		struct riff_wave_header riff_wave_header;
		struct chunk_header chunk_header;
    		struct chunk_fmt chunk_fmt;
		int more_chunks = 1;
		bool Ret = false;

		Lock();

		m_hFile = fopen(path, "rb");
		if (!m_hFile) {
			LogE("Unable to create file '%s' !!!\n", path);
			UnLock();
			return false;
		}

		if (fread(&riff_wave_header, sizeof(riff_wave_header), 1, m_hFile) != 1){
            		LogE("Error: '%s' does not contain a riff/wave header\n", path);
			goto wav_error;
        	}
	        if ((riff_wave_header.riff_id != ID_RIFF) ||
        	    (riff_wave_header.wave_id != ID_WAVE)) {
            		LogE("Error: '%s' is not a riff/wave file\n", path);
            		goto wav_error;
        	}

        	do {
            		if (fread(&chunk_header, sizeof(chunk_header), 1, m_hFile) != 1){
                		LogE("Error: '%s' does not contain a data chunk\n", path);
                		goto wav_error;
            		}

            		switch (chunk_header.id) {
            		case ID_FMT:
                		if (fread(&chunk_fmt, sizeof(chunk_fmt), 1, m_hFile) != 1){
                    			LogE("Error: '%s' has incomplete format chunk\n", path);
                    			goto wav_error;
                		}
                		/* If the format header is larger, skip the rest */
                		if (chunk_header.sz > sizeof(chunk_fmt))
                    			fseek(m_hFile, chunk_header.sz - sizeof(chunk_fmt), SEEK_CUR);
                		break;
            		case ID_DATA:
                		/* Stop looking for chunks */
                		more_chunks = 0;
                		break;
            		default:
                		/* Unknown chunk, skip bytes */
                		fseek(m_hFile, chunk_header.sz, SEEK_CUR);
            		}
        	} while (more_chunks);

		m_bWavFormat = true;
		strcpy(m_strName, path);

		header->riff_id = riff_wave_header.riff_id;
		header->riff_sz = riff_wave_header.riff_sz;
		header->riff_fmt = riff_wave_header.wave_id;
		header->fmt_id = chunk_header.id;
		header->fmt_sz = chunk_header.sz;
		header->audio_format = chunk_fmt.audio_format;
		header->num_channels = chunk_fmt.num_channels;
		header->sample_rate = chunk_fmt.sample_rate;
		header->byte_rate = chunk_fmt.byte_rate;
		header->block_align = chunk_fmt.block_align;
		header->bits_per_sample = chunk_fmt.bits_per_sample;

		m_Channels = chunk_fmt.num_channels;
		m_SampleRate = chunk_fmt.sample_rate;
		m_SampleBits = chunk_fmt.bits_per_sample;
		m_ReadBytes = 0;
		Ret = true;

		LogI(" %s open : %s %d ch, %d hz, %d bits\n",
			m_bWavFormat ? "WAV" : "RAW", m_strName, m_Channels,
			m_SampleRate, m_SampleBits);

	wav_error:
		UnLock();
		return Ret;
	}

	void Close(void) {
		struct wav_header *header = &m_WavHeader;

		if (!m_hFile)
			return;

		LogI(" %s close: %s %d ch, %d hz, %d bits, w %lld, r %lld bytes\n",
			m_bWavFormat ? "WAV" : "RAW", m_strName,
			m_Channels, m_SampleRate, m_SampleBits, m_WriteBytes, m_ReadBytes);

		Lock();

		if (m_bWavFormat && m_WriteBytes) {
			long long frames = m_WriteBytes /
				((m_SampleBits / 8) * m_Channels);
			/* write header now all information is known */
			header->data_sz = frames * header->block_align;
			header->riff_sz = header->data_sz + sizeof(header) - 8;

			fseek(m_hFile, 0, SEEK_SET);
			fwrite(header, sizeof(struct wav_header), 1, m_hFile);
		}

		fclose(m_hFile);
		UnLock();

		m_hFile = NULL;
		m_WriteBytes = 0;
		m_ReadBytes = 0;
		m_SavePeriodAvail = 0;

		memset(m_strName, 0, sizeof(m_strName));
	}

	bool Write(void *buffer, size_t size) {
		if (!m_hFile)
			return false;

		Lock();

		if (size != fwrite(buffer, 1, size, m_hFile)) {
			LogE("write wave file to %s size %d !!!\n",
				m_strName, (int)size);
			UnLock();
			return false;
		}

		m_WriteBytes += size;

		if (m_bWavFormat && m_SavePeriodBytes) {
			m_SavePeriodAvail -= m_WriteBytes;
			if (m_SavePeriodAvail < 0) {
				struct wav_header *header = &m_WavHeader;
				long long frames;

				frames = m_WriteBytes /
					((m_SampleBits / 8) * m_Channels);
				header->data_sz = frames * header->block_align;
				header->riff_sz = header->data_sz +
					sizeof(header) - 8;
				fseek(m_hFile, 0, SEEK_SET);
				fwrite(header,
					sizeof(struct wav_header), 1, m_hFile);
				fseek(m_hFile, 0, SEEK_END);
				m_SavePeriodAvail = m_SavePeriodBytes;
			}
		}
		UnLock();

		return true;
	}

	bool Read(void *buffer, size_t size) {
		if (!m_hFile)
			return false;

		Lock();

		if (size != fread(buffer, 1, size, m_hFile)) {
			UnLock();
			return false;
		}

		m_ReadBytes += size;

		UnLock();

		return true;
	}

	bool ReadLoop(void *buffer, size_t size, int delay_ms) {
		bool Ret = Read(buffer, size);

		if (!Ret) {
			fseek(m_hFile,  sizeof(struct wav_header), SEEK_SET);
			usleep(delay_ms * 1000);
			Ret = Read(buffer, size);
		}

		return Ret;
	}

	const char *GetName(void) { return m_strName; }
	const struct wav_header *GetWaveHeader(void) { return &m_WavHeader; }

	private:
	void Lock(void)   { }
	void UnLock(void) { }

	private:
	FILE *m_hFile;
	struct wav_header m_WavHeader;

	char m_strName[512];
	long long m_WriteBytes;
	long long m_ReadBytes;
	int  m_Channels, m_SampleRate, m_SampleBits;
	bool m_bWavFormat;
	long long m_SavePeriodBytes;
	long long m_SavePeriodAvail;
	pthread_mutex_t m_Lock;
};

#if 0
static FILE *open_wave(char *path, int channels,
			int rate, int bits, struct wav_header *header) {
	FILE *file;

	if (!header)
		return NULL;

	file = fopen(path, "wb");
	if (!file) {
		LogE("Unable to create file '%s'\n", path);
		return NULL;
	}
	memset(header, 0, sizeof(*header));

	header->riff_id = ID_RIFF;
	header->riff_sz = 0;
	header->riff_fmt = ID_WAVE;
	header->fmt_id = ID_FMT;
	header->fmt_sz = 16;
	header->audio_format = FORMAT_PCM;
	header->num_channels = channels;
	header->sample_rate = rate;
	header->bits_per_sample = bits;
	header->byte_rate = (header->bits_per_sample / 8) *
			channels * rate;
	header->block_align = channels * (header->bits_per_sample / 8);
	header->data_id = ID_DATA;

	/* leave enough room for header */
	fseek(file, sizeof(struct wav_header), SEEK_SET);
	LogI("wave open %s: %u ch, %d hz, %u bits\n",
		path, channels, rate, bits);

	return file;
}

static int write_wave(FILE *file, void *buffer, int size) {
	if (!file)
		return -1;

	int ret = fwrite(buffer, 1, size, file);

	if (ret != size) {
		LogE("Error capturing sample ....\n");
		return ret;
	}

	return ret;
}

static void close_wave(FILE *file, struct wav_header *header,
				int channels, int bits, unsigned long bytes) {
	long long frames;

	if (!file || bits == 0 || channels == 0)
		return;

	LogI("wave close: %u ch, %d hz, %u bits, %lu bytes\n",
		channels, header->sample_rate, bits, bytes);

	frames = bytes / ((bits / 8) * channels);

	/* write header now all information is known */
	header->data_sz = frames * header->block_align;
	header->riff_sz = header->data_sz + sizeof(header) - 8;

	fseek(file, 0, SEEK_SET);
	fwrite(header, sizeof(struct wav_header), 1, file);
	fclose(file);
	LogI("[done]\n");
}
#endif

#if __cplusplus
}
#endif

#endif
