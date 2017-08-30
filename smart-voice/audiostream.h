#ifndef _AUDIOSTREAM_H_
#define _AUDIOSTREAM_H_

#include "audioplay.h"
#include "qbuff.h"
#include "wav.h"
#include "util.h"

#ifndef ANDROID
#include <list>
#include <algorithm>

using std::list;
#else
#include "list.h"

using LOCAL::list;
#endif

typedef struct time_stemp_t {
	long long min;
	long long max;
	long long tot;
	long long cnt;
} TIMESTEMP_T;

#define	SET_TIME_STAT(t, d)	do { \
	if (t->min > d) t->min = d; \
	if (d > t->max) t->max = d; \
	t->cnt++, t->tot += d; \
	} while (0)

typedef struct audio_param_t {
	int Card;
	int Device;
	int Channels;
	int SampleBits;
	int SampleRate;
	int PeriodBytes;
	int Periods;
} AUDIOPARAM_T;

typedef struct wav_file_t {
	CWAVFile *hWav;
	int Channels;
	int SampleRate;
	int SampleBits;
	char WavPath[256];

	wav_file_t(
		CWAVFile *wav = NULL,
		int ch = 0,
		int rate = 0,
		int bit = 0,
		const char *path = NULL
	) {
		hWav = wav;
		Channels = ch;
		SampleRate = rate;
		SampleBits = bit;
		if (path) {
			char *p = WavPath;

			strcpy(p, path);
			if ('/' != p[strlen(p) - 1]) {
				p[strlen(p)] = '/';
				p[strlen(p)+1] = '0';
			}
		}
	}
} WAVFILE_T;

#define WAV_CAPT_PERIOD	(512 * 1024) /* 51Kbyte */

class CAudioStream {
	public:
	CAudioStream(const char *Name = NULL,
		int Type = -1,
		AUDIOPARAM_T Param = { -1, -1, 0, },
		void *(*Fn)(void *) = NULL,
		void *Arg = NULL);

	CAudioStream(const char *Name = NULL,
		void *(*Fn)(void *) = NULL,
		void *Arg = NULL);

	virtual ~CAudioStream(void) { ReleaseAll(); }

	void ReleaseAll(void);

	const char *GetName(void) { return m_pName; }
	int GetType(void) { return m_Type; }
	void Lock(void)   { pthread_mutex_lock(&m_Lock); }
	void UnLock(void) { pthread_mutex_unlock(&m_Lock); }

	/* Audio */
	bool OpenAudio(enum AUDIO_STREAM_DIR Direction);
	void CloseAudio(void);
	bool StartAudio(void);
	void StopAudio(void);
	int  PlayAudio(char *Buffer, int Bytes);
	int  RecAudio(char *Buffer, int Bytes);

	void SetParam(AUDIOPARAM_T *Param);
	const AUDIOPARAM_T *GetParam(void);

	/* Buffer */
	CQueueBuffer *GetBuffer(void) { return m_pBuffer; }
	bool AllocBuffer(void);
	void ReleaseBuffer(void);

	/* Buffer */
	char *PushBuffer(int Bytes, int WaitTime);
	bool RelPushBuffer(int Bytes);
	char *PopBuffer(int Bytes, int WaitTime);
	bool RelPopBuffer(int Bytes);

	void ClearBuffer(void);
	void WaitCleanBuffer(void);

	/* Command/Option */
	void SetCommand(unsigned Cmd);
	unsigned GetCommand(unsigned Cmd);
	unsigned GetCommand(void);
	void ClearCommand(unsigned Cmd);

	void SetArgument(void *Data) { m_pArgument = Data; }
	void *GetArgument(void) { return m_pArgument; }

	/* Wav File */
	WAVFILE_T *CreateWavFile(int Ch, int Rate, int Bit, const char *Path);
	void DeleteWavFile(WAVFILE_T *Hnd);
	void DeleteWavFileAll(void);

	bool OpenWavFile(WAVFILE_T *Hnd, const char *Fmt, ...);
	void CloseWavFile(WAVFILE_T *Hnd);
	void CloseAllWavFile(void);
	bool WriteWavFile(WAVFILE_T *Hnd, void *Buffer, size_t Size);

	public:
	TIMESTEMP_T time = { 1000 * 1000, 0, 0, 0 };
	list <CAudioStream *> LStream; /* Input streams */
	void *(*FN)(void *);
	pthread_t Handle;

	private:
	const char *m_pName = NULL;
	int m_Type = 0;
	AUDIOPARAM_T m_AudioParam = { -1, -1, 0, };
	CQueueBuffer *m_pBuffer = NULL;
	CAudioPlayer *m_AudioPlayer = NULL;
	unsigned m_Command = 0;
	void *m_pArgument = NULL;
	pthread_mutex_t m_Lock;
	list <WAVFILE_T *> LWav;
	bool m_Release = false;
};

#endif /* _AUDIOSTREAM_H_ */
