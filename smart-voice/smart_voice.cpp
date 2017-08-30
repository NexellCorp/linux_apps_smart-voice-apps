#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>

#include "qbuff.h"
#include "wav.h"
#include "resample.h"
#include "audioplay.h"
#include "audiostream.h"

#include "util.h"
#include "nx_pdm.h"

#ifdef SUPPORT_PRE_PROCESS
extern "C" {
#include "libPreproc1.h"
}
#endif

/******************************************************************************/
#define MAX_STREAMS		20
#define MONITOR_PERIOD		(3 * 1000)	/* US */
#define VERIFY_TIME		(20)		/* SEC */

#define	AEC_INPUT_CH		(6)		/* MIC0(L)/MIC1(R), MIC2(L)/MIC3(R), REF0(L/R), REF1(L/R) */
#define	AEC_INPUT_BIT		(16)
#define	AEC_INPUT_SAMPLE	(256)		/* 256 sample * (2ch * 16bits) */

#define	PDM_PERIOD_BYTES_IN	(8192)
#define	PDM_PERIOD_BYTES_OUT	(2048)
#define	PDM_FILTER_FASTMODE	(0)
#define	PDM_FILTER_AGC_DB	(0)
#define PDM_FILTER_PDM_GAIN	(4)
#define	REF_SAMPLE_RATE		(48000)
#define	LOG_PROMPT		LogI("#>")

/******************************************************************************/
/* stream types */
#define	STREAM_TYPE_OUT		(1<<0) /* Play or Output */
#define	STREAM_TYPE_REF		(1<<1) /* Reference */
#define	STREAM_TYPE_PDM		(1<<2) /* PDM */
#define	STREAM_TYPE_FLT		(1<<3) /* PCM Filter */
#define	STREAM_TYPE_RES		(1<<4) /* resampler */
#define	STREAM_TYPE_PREP	(1<<5) /* Pre-Process */

#define	STYPE(s, t)		((reinterpret_cast<CAudioStream *>(s))->GetType() & t)

/* commands */
#define	CMD_STREAM_EXIT		(1<<0)
#define	CMD_STREAM_RESET	(1<<1)
#define	CMD_STREAM_NOREF	(1<<2)
#define	CMD_PRE_PROCESS		(1<<3)
#define	CMD_FILE_CAPT		(1<<4)
#define	CMD_FILE_STOP		(1<<5)
#define	CMD_FILE_PDM		(1<<6)
#define	CMD_FILE_PCM		(1<<7)
#define	CMD_FILE_PREP_IN	(1<<8)
#define	CMD_FILE_PREP_OUT	(1<<9)

typedef struct sound_device {
	int c = -1, d = -1;	/* card, device */
} SNDDEV_T;

/* program options */
typedef struct option_t {
	char file_path[256] = "";
	int dvfs_mhz = 0;
	bool do_run_cmd = true;
	bool do_preprocess = true;
	bool filter_fast_mode = false;
	int filter_agc_db = PDM_FILTER_AGC_DB;
	int filter_pdm_gain = PDM_FILTER_PDM_GAIN;
	int loop_time = 0;
	int moniter_period = MONITOR_PERIOD;
	int preprocess_out = 0;
	unsigned capture;
	int preprocess_ch = AEC_INPUT_CH;
	bool playback = false;
	pthread_mutex_t lock;
	SNDDEV_T play, ref, pdm;
} OP_T;

static list <CAudioStream *> g_List;

static inline void cmd_set(CAudioStream *S, unsigned CMD)
{
	S->SetCommand(CMD);
}

static inline unsigned cmd_get(CAudioStream *S, unsigned CMD)
{
	return S->GetCommand(CMD);
}

static inline unsigned cmd_get(CAudioStream *S)
{
	return S->GetCommand();
}

static inline void cmd_clear(CAudioStream *S, unsigned CMD)
{
	S->ClearCommand(CMD);
}

static inline bool cmd_value(CAudioStream *S, unsigned CMD)
{
	return CMD == (S->GetCommand(CMD) & CMD) ? true : false;
}

static void streams_command(unsigned cmd)
{
	int i = 0;

	for (auto ls = g_List.begin(); ls != g_List.end(); ++ls, i++)
		cmd_set((*ls), cmd);

	LogD("%s: CMD 0x%x\n", __func__, cmd);
}

static void streams_command(unsigned cmd, unsigned clear)
{
	int i = 0;

	for (auto ls = g_List.begin(); ls != g_List.end(); ++ls, i++) {
		if (clear)
			cmd_clear((*ls), clear);
		cmd_set((*ls), cmd);
	}

	LogD("%s: CMD 0x%x:0x%x\n", __func__, cmd, clear);
}

/******************************************************************************/
static void fn_cleanup(void *Data) { }

static void *fn_playback(void *Data)
{
	CAudioStream *S = reinterpret_cast<CAudioStream *>(Data);

	CAudioStream *I = S->LStream.front();
	int s_bytes = S->GetParam()->PeriodBytes;

	OP_T *op = reinterpret_cast<OP_T *>(S->GetArgument());
	TIMESTEMP_T *time = &S->time;
	long long ts = 0, td = 0;
	bool Ret = false;

	pthread_cleanup_push(fn_cleanup, reinterpret_cast<void *>(S));

	if (op->playback)
		Ret = S->OpenAudio(AUDIO_STREAM_PLAYBACK);

__reset:
	I->WaitCleanBuffer();
	cmd_clear(S, CMD_STREAM_RESET);

	while (!cmd_get(S, CMD_STREAM_EXIT | CMD_STREAM_RESET)) {
		char *Ptr = I->PopBuffer(s_bytes, 1);

		if (!Ptr)
			continue;

		if (Ret && !cmd_get(S, CMD_STREAM_NOREF)) {
			RUN_TIMESTAMP_US(ts);

			int Err = S->PlayAudio(Ptr, s_bytes);

			END_TIMESTAMP_US(ts, td);
			SET_TIME_STAT(time, td);

			if (Err < 0)
				msleep(1);
		}

		I->RelPopBuffer(s_bytes);
	}

	if (cmd_get(S, CMD_STREAM_RESET))
		goto __reset;

	S->CloseAudio();

	pthread_cleanup_pop(1);

	return NULL;
}

static void *fn_capture(void *Data)
{
	CAudioStream *S = reinterpret_cast<CAudioStream *>(Data);
	int Bytes = S->GetParam()->PeriodBytes;
	bool Ret;

	pthread_cleanup_push(fn_cleanup, reinterpret_cast<void *>(S));

__reset:
	S->ClearBuffer();
	cmd_clear(S, CMD_STREAM_RESET);

	Ret = S->OpenAudio(AUDIO_STREAM_CAPTURE);
	if (!Ret) {
		LogE("[%s: Failed Audio %s, kill %d]\n",
			__func__, S->GetName(), getpid());
		kill(getpid(), SIGTERM);
		return NULL;
	}

	Ret = S->StartAudio();
	if (!Ret)
		return NULL;

	while (!cmd_get(S, CMD_STREAM_EXIT | CMD_STREAM_RESET)) {
		if (STYPE(S, STREAM_TYPE_REF) && cmd_get(S, CMD_STREAM_NOREF)) {
			msleep(1);
			continue;
		}

		char *Ptr = S->PushBuffer(Bytes, TIMEOUT_INFINITE);
		if (!Ptr)
			continue;

		int Err = S->RecAudio(Ptr, Bytes);
		if (Err < 0)
			continue;

		S->RelPushBuffer(Bytes);
	}

	if (cmd_get(S, CMD_STREAM_RESET)) {
		S->StopAudio();
		goto __reset;
	}

	S->CloseAudio();

	pthread_cleanup_pop(1);

	return NULL;
}

static void *fn_resample(void *Data)
{
	CAudioStream *S = reinterpret_cast<CAudioStream *>(Data);
	CAudioStream *I = S->LStream.front();

	const AUDIOPARAM_T *sp = S->GetParam();
	const AUDIOPARAM_T *ip = I->GetParam();

	int s_ch = sp->Channels, s_rate = sp->SampleRate, s_bytes = sp->PeriodBytes;
	int i_ch = ip->Channels, i_rate = ip->SampleRate, i_bits = ip->SampleBits;

	char *I_Ptr, *O_Ptr, *R_Ptr;
	TIMESTEMP_T *time = &S->time;
	long long ts = 0, td = 0;

	int r_len = s_bytes*2;

	R_Ptr = new char[r_len];
	assert(R_Ptr);

	pthread_cleanup_push(fn_cleanup, reinterpret_cast<void *>(S));

__reset:
	/* clear resampler offset */
	int r_offs = 0;
	ReSampleContext *Resampler =
		audio_resample_init(s_ch, i_ch,
			static_cast<float>(s_rate), static_cast<float>(i_rate));
	assert(Resampler);

	S->ClearBuffer();
	I->WaitCleanBuffer();
	cmd_clear(S, CMD_STREAM_RESET);

	while (!cmd_get(S, CMD_STREAM_EXIT | CMD_STREAM_RESET)) {
		/* Get buffer from I2S output */
		I_Ptr = I->PopBuffer(s_bytes, TIMEOUT_INFINITE);
		if (!I_Ptr)
			continue;

		/* Resample */
		int framebytes = i_ch * (i_bits/8);
		int samples;

		assert(r_offs < r_len);
		RUN_TIMESTAMP_US(ts);

		samples = audio_resample(Resampler,
					reinterpret_cast<short *>(R_Ptr + r_offs),
					reinterpret_cast<short *>(I_Ptr),
					(s_bytes / framebytes));

		r_offs += (samples * framebytes);

		END_TIMESTAMP_US(ts, td);
		SET_TIME_STAT(time, td);

		/* Copy resample data to out buffer */
		if (r_offs >= s_bytes) {
			O_Ptr = S->PushBuffer(s_bytes, TIMEOUT_INFINITE);
			if (!O_Ptr)
				continue;

			int remain = r_offs - s_bytes;
			memcpy(O_Ptr, R_Ptr, s_bytes);
			memmove(R_Ptr, R_Ptr + s_bytes, remain);

			r_offs = remain;
			S->RelPushBuffer(s_bytes);
		}

		I->RelPopBuffer(s_bytes);
	}

	if (Resampler)
		audio_resample_close(Resampler);

	if (cmd_get(S, CMD_STREAM_RESET))
		goto __reset;

	pthread_cleanup_pop(1);

	return NULL;
}

static void *fn_filter(void *Data)
{
	CAudioStream *S = reinterpret_cast<CAudioStream *>(Data);
	CAudioStream *I = S->LStream.front();

	const AUDIOPARAM_T *sp = S->GetParam();
	OP_T *op = reinterpret_cast<OP_T *>(S->GetArgument());

	/* per channel: 2048 PDM -> 512 PCM */
	int i_bytes = PDM_PERIOD_BYTES_IN;
	int s_bytes = PDM_PERIOD_BYTES_OUT;

	char *O_Ptr, *I_Ptr = { NULL, };

	WAVFILE_T *pWI, *pWO;

	pthread_cleanup_push(fn_cleanup, reinterpret_cast<void *>(S));

	/* Initialize PDM-AGC */
	pdm_STATDEF pdm_st;
	pdm_Init(&pdm_st);

	if (0 != pdm_SetParam(&pdm_st, PDM_PARAM_GAIN, op->filter_pdm_gain))
		LogE("Failed: pdm gain parmeter [%d]!!!\n", op->filter_pdm_gain);

	pWI = S->CreateWavFile(0, 0, 0, op->file_path);
	assert(pWI);

	pWO = S->CreateWavFile(sp->Channels,
			sp->SampleRate, sp->SampleBits,	op->file_path);
	assert(pWO);

__reset:
	S->ClearBuffer();
	I->WaitCleanBuffer();

	cmd_clear(S, CMD_STREAM_RESET);

	while (!cmd_get(S, CMD_STREAM_EXIT | CMD_STREAM_RESET)) {
		if (cmd_value(S, CMD_FILE_CAPT | CMD_FILE_PDM)) {
			S->OpenWavFile(pWI, "%s.pdm.raw", S->GetName());
			cmd_clear(S, CMD_FILE_PDM);
		}

		if (cmd_value(S, CMD_FILE_CAPT | CMD_FILE_PCM)) {
			S->OpenWavFile(pWO, "%s.pcm.wav", S->GetName());
			cmd_clear(S, CMD_FILE_PCM);
		}

		if (cmd_get(S, CMD_FILE_STOP)) {
			S->CloseAllWavFile();
			cmd_clear(S, CMD_FILE_STOP);
		}

		/* Get InBuffers */
		I_Ptr = I->PopBuffer(i_bytes, TIMEOUT_INFINITE);
		if (!I_Ptr)
			continue;

		/* Get OutBuffers */
		O_Ptr = S->PushBuffer(s_bytes, TIMEOUT_INFINITE);
		if (!O_Ptr)
			continue;

		/*
		 * PDM FILTER
		 */
		int agc_dB = op->filter_agc_db;
		TIMESTEMP_T *time = &S->time;
		long long ts = 0, td = 0;

		RUN_TIMESTAMP_US(ts);

		pdm_Run(&pdm_st, (short int*)O_Ptr, (int*)I_Ptr, agc_dB);

		END_TIMESTAMP_US(ts, td);
		SET_TIME_STAT(time, td);

		S->WriteWavFile(pWI, I_Ptr, i_bytes);
		S->WriteWavFile(pWO, O_Ptr, s_bytes);

		I->RelPopBuffer(i_bytes);
		S->RelPushBuffer(s_bytes);
	}

	if (cmd_get(S, CMD_STREAM_RESET))
		goto __reset;

	pthread_cleanup_pop(1);

	return NULL;
}

static inline void pcm_split(int *s, int *d0, int *d1, int size)
{
	int *S = s;
	int *D0 = d0, *D1 = d1;
	for (int i = 0; i < size; i += 8) {
		*d0++ = *s++;
		*d1++ = *s++;
	}

	assert(0 == (((long)s  - (long)S)  - size));
	assert(0 == (((long)d0 - (long)D0) - size/2));
	assert(0 == (((long)d1 - (long)D1) - size/2));
}

static void make_wav_format(int ch, int *R0, int *R1, int *D0, int *D1, int *Out, int Size)
{
	int *d = Out;
	int *s0 = R0, *s1 = R1;
	int *s2 = D0, *s3 = D1;

	if (ch == 8) {
		for (int i = 0; i < Size; i += 4) {
			*d++ = *s0++, *d++ = *s1++;
			*d++ = *s2++, *d++ = *s3++;
		}
	} else if (ch == 6) {
		for (int i = 0; i < Size; i += 4) {
			*d++ = *s0++, *d++ = *s2++;
			*d++ = *s3++;
		}
	} else if (ch == 4) {
		for (int i = 0; i < Size; i += 4)
			*d++ = *s0++, *d++ = *s2++;
	} else {
		LogE("Failed %s not support ch %d\n", __func__, ch);
	}
}

static void *fn_process(void *Data)
{
	CAudioStream *S = reinterpret_cast<CAudioStream *>(Data);
	list <CAudioStream *>::iterator ls;

	const AUDIOPARAM_T *sp = S->GetParam();
	OP_T *op = reinterpret_cast<OP_T *>(S->GetArgument());

	int i_links = S->LStream.size();
	int s_bytes = sp->PeriodBytes;
	int rate = sp->SampleRate;
	int bits = sp->SampleBits;

	char *O_Ptr, *I_Ptr[i_links];
	int wait = 1, i, r;

	/* L(16bit)/R(16bit) * sample */
	int *Dummy = reinterpret_cast<int *>(new char[AEC_INPUT_SAMPLE * AEC_INPUT_BIT/8 * 2]);
	int *O_PCM = reinterpret_cast<int *>(new char[AEC_INPUT_SAMPLE * AEC_INPUT_BIT/8 * 2]);
	int *I_Dat[2] = { Dummy, Dummy };
	int *I_Ref[2] = { Dummy, Dummy };

#ifdef SUPPORT_PRE_PROCESS
	int *O_AEC1 = new int[256];
	int *O_AEC2 = new int[256];
#endif

	int i_ch = op->preprocess_ch;
	int o_ch = sp->Channels;
	bool is_DumpIn = false;
	WAVFILE_T *pWO, *pWI;

	pthread_cleanup_push(fn_cleanup, reinterpret_cast<void *>(S));

	memset(Dummy, 0, 1024);

	pWI = S->CreateWavFile(i_ch, rate, bits, op->file_path);
	pWO = S->CreateWavFile(o_ch, rate, bits, op->file_path);
	assert(pWI && pWO);

__reset:
	S->ClearBuffer();
	for (ls = S->LStream.begin(); ls != S->LStream.end(); ++ls)
		(*ls)->WaitCleanBuffer();

	cmd_clear(S, CMD_STREAM_RESET);

	while (!cmd_get(S, CMD_STREAM_EXIT | CMD_STREAM_RESET)) {
		if (cmd_value(S, CMD_FILE_CAPT | CMD_FILE_PREP_IN)) {
			static int index = 0;
			S->OpenWavFile(pWI, "%s.i.%d.wav", S->GetName(), index++);
			cmd_clear(S, CMD_FILE_PREP_IN);
			is_DumpIn = true;
		}

		if (cmd_value(S, CMD_FILE_CAPT | CMD_FILE_PREP_OUT)) {
			S->OpenWavFile(pWO, "%s.o.wav", S->GetName());
			cmd_clear(S, CMD_FILE_PREP_OUT);
		}

		if (cmd_get(S, CMD_FILE_STOP)) {
			S->CloseAllWavFile();
			cmd_clear(S, CMD_FILE_STOP);
			is_DumpIn = false;
		}

		/* Get InBuffers */
		for (i = 0, r = 0,
			ls = S->LStream.begin(); ls != S->LStream.end(); ++ls, i++) {
			int bytes = STYPE((*ls), STREAM_TYPE_PDM) ? s_bytes *  2 : s_bytes;
			do {
				I_Ptr[i] = (*ls)->PopBuffer(bytes, wait);

				if (STYPE((*ls), STREAM_TYPE_REF) && cmd_get(S, CMD_STREAM_NOREF))
					break; /* next */
				if (cmd_get(S, CMD_STREAM_RESET | CMD_STREAM_EXIT))
					break; /* exit */
			} while (!I_Ptr[i]);

			if (STYPE((*ls), STREAM_TYPE_PDM)) {
				/* for SPLIT copy  [L0/R0/L1/R1] -> [L0/R0 .....][L1/R1 ....]*/
				pcm_split((int *)I_Ptr[i],
					(int *)I_Dat[0], (int *)I_Dat[1], bytes);
			} else  if (STYPE((*ls), STREAM_TYPE_REF)) {
				I_Ref[r++] = I_Ptr[i] ? reinterpret_cast<int *>(I_Ptr[i]) : Dummy;
			} else {
				LogE("***** %s: not support input %s *****\n",
					S->GetName(), (*ls)->GetName());
			}
		}

		if (cmd_get(S, CMD_STREAM_EXIT | CMD_STREAM_RESET))
			continue;

		if (is_DumpIn) {
			/* 4096 = 256 sample * 8ch * 2 (16bit) */
			char Data[AEC_INPUT_SAMPLE * i_ch * AEC_INPUT_BIT/8];

			make_wav_format(i_ch,
				I_Ref[0], I_Ref[1], I_Dat[0], I_Dat[1],
				reinterpret_cast<int *>(Data), s_bytes);

			S->WriteWavFile(pWI, reinterpret_cast<void *>(Data), sizeof(Data));
		}

		/*
		 * PRE PROCESS
		 */
		if (cmd_get(S, CMD_PRE_PROCESS)) {
#ifdef SUPPORT_PRE_PROCESS
			TIMESTEMP_T *time = &S->time;
			long long ts = 0, td = 0;

			assert(I_Dat[0]), assert(I_Dat[1]);
			assert(I_Ref[0]), assert(I_Ref[1]);

			RUN_TIMESTAMP_US(ts);

			preProc((short int*)I_Dat[0], (short int*)I_Dat[1],
				(short int*)I_Ref[0], (short int*)I_Ref[1],
				(short int*)O_AEC1, (short int*)O_AEC2,
				(short int*)O_PCM);

			END_TIMESTAMP_US(ts, td);
			SET_TIME_STAT(time, td);
#endif
		}

		/* Release InBuffers */
		for (i = 0, ls = S->LStream.begin(); ls != S->LStream.end(); ++ls, i++) {
			if (I_Ptr[i]) {
				int bytes = STYPE((*ls), STREAM_TYPE_PDM) ? s_bytes *  2 : s_bytes;
				(*ls)->RelPopBuffer(bytes);
			}
		}

		/* Copy to OutBuffer and release */
		O_Ptr = S->PushBuffer(s_bytes, 1);
		if (O_Ptr) {
			int *src = O_PCM;
			switch(op->preprocess_out) {
				case  1: src = reinterpret_cast<int *>(I_Dat[0]); break;
				case  2: src = reinterpret_cast<int *>(I_Dat[1]); break;
				case  3: src = reinterpret_cast<int *>(I_Ref[0]); break;
				case  4: src = reinterpret_cast<int *>(I_Ref[1]); break;
				default: src = reinterpret_cast<int *>(O_PCM);
					 op->preprocess_out = 0; break;
			}
			memcpy(O_Ptr, src, s_bytes);

			S->WriteWavFile(pWO, reinterpret_cast<void *>(O_Ptr), s_bytes);
			S->RelPushBuffer(s_bytes);
		}
	}

	if (cmd_get(S, CMD_STREAM_RESET))
		goto __reset;

	pthread_cleanup_pop(1);

	return NULL;
}

static void *fn_monitor(void *Data)
{
	OP_T *op = reinterpret_cast<OP_T *>(
			(reinterpret_cast<CAudioStream *>(Data))->GetArgument());
	int AvailSize, BufferBytes;
	bool Ret = true;
	int TestTime = op->loop_time;
	int LoopTime = 0;

	assert(op);

	while (1) {
		msleep(op->moniter_period);
		LoopTime += op->moniter_period/1000;

		LogI("\n================================================================\n");
		LogI(" FILTER %s mode, agc %d DB, pdm %d gain\n",
			op->filter_fast_mode ? "fast" : "no fast",
			op->filter_agc_db, op->filter_pdm_gain);

		int i = 0;
		for (auto ls = g_List.begin(); ls != g_List.end(); ++ls, i++) {
			CAudioStream *S = (*ls);
			CQueueBuffer *pBuf = S->GetBuffer();
			TIMESTEMP_T *time = &S->time;

			if (cmd_get(S, CMD_STREAM_EXIT))
				goto exit;

			LogI("REF:%s [%6s]", cmd_get(S, CMD_STREAM_NOREF) ? "NO" : "IN", S->GetName());
			if (pBuf) {
				AvailSize = pBuf->GetAvailSize();
				BufferBytes = pBuf->GetBufferBytes();
				LogI(" AvailSize = %7d/%7d", AvailSize, BufferBytes);
			}

			if (time->cnt)
				LogI(" min:%2llu.%03llu ms, max:%2llu.%03llu ms, avr:%2llu.%03llu ms\n",
					time->min/1000, time->min%1000, time->max/1000, time->max%1000,
					(time->tot/time->cnt)/1000, (time->tot/time->cnt)%1000);
			else
				LogI("\n");

			if (!pBuf)
				continue;

			if (0 == AvailSize) {
				LogE("\tRESULT : FAILED : NO BUFFER [%s]\n", S->GetName());
				Ret = false;
			}

			if (AvailSize < (BufferBytes/4)*3)
				LogE("\tRESULT : WARN   : Out of Sync, %s ???\n", Ret ? "OK" : "FAIL");
		}
		LogI("================================================================\n");
		LOG_PROMPT;

		if (!Ret || (TestTime && LoopTime >= TestTime)) {
			LogI("RESULT : %s [%d:%d sec]...\n", Ret ? "OK" : "FAIL", TestTime, LoopTime);
			streams_command(CMD_STREAM_EXIT);
			exit(-1);	/* EXIT program */
		}
	}
exit:
	return NULL;
}

typedef struct udev_event_t {
	const char *sample_msg;
	int sample_rate;
} EVENTMSG_T;

static void event_parse(const char *Data, EVENTMSG_T *evt)
{
	evt->sample_msg = "";
	evt->sample_rate = 0;

	/* currently ignoring SEQNUM */
    	while (*Data) {
		const char *index = "SAMPLERATE_CHANGED=";

        	if (!strncmp(Data, index, strlen(index))) {
            		Data += strlen(index);
            		evt->sample_rate = strtoll(Data, NULL, 10);
            		LogD("[%s] %d\n", index, evt->sample_rate);
        	}

		index = "SAMPLE_NO_DATA=";
        	if (!strncmp(Data, index, strlen(index))) {
            		Data += strlen(index);
            		evt->sample_msg = Data;
            		LogD("[%s] %s\n", index, evt->sample_msg);
        	}

        	/* advance to after the next \0 */
        	while(*Data++) { }
    	}
}

static void *fn_event(void *Data)
{
	int Fd;
	EVENTMSG_T evt;
	char Buffer[256];
	int Size = ARRAY_SIZE(Buffer);
	struct timeval tv;
	CAudioStream *S = reinterpret_cast<CAudioStream *>(Data);

	Fd = audio_event_init();
	if (Fd < 0) {
		LogE("FAILED: %s ....\n", S->GetName());
		return NULL;
	}

	while (!cmd_get(S, CMD_STREAM_EXIT)) {
		if (!audio_event_msg(Fd, Buffer, Size))
			continue;

		event_parse(Buffer, &evt);
		gettimeofday(&tv, NULL);

		if (evt.sample_rate) {
			LogI("\n***** (%6ld.%06ld s) Rate [%dhz]*****\n",
				tv.tv_sec, tv.tv_usec, evt.sample_rate);
			streams_command(CMD_STREAM_RESET, CMD_STREAM_NOREF);
		}

		if (evt.sample_msg && !strcmp(evt.sample_msg, "YES")) {
			LogI("\n***** (%6ld.%06ld s) NO REF *****\n",
				tv.tv_sec, tv.tv_usec);
			streams_command(CMD_STREAM_NOREF);
		}
	}

	audio_event_close(Fd);
	return NULL;
}

/******************************************************************************/
static void print_help(const char *name, OP_T *op)
{
	LogI("\n");
	LogI("usage: options\n");
	LogI("\t-i : no wait input argument\n");
	LogI("\t-s : dvfs cpu frequency (mhz)\n");
#ifdef SUPPORT_PRE_PROCESS
	LogI("\t-e : don't pre-process\n");
#endif
	LogI("\t-f : change filter fastmode (%s)\n", op->filter_fast_mode ? "true" : "false");
	LogI("\t-a : agc gain parameter(default:%d 0:disable, unit:dB)\n", op->filter_agc_db);
	LogI("\t-g : pdm gain parameter(default:%d, range : 2~6)\n", op->filter_pdm_gain);

	LogI("\t-c : set file capture path\n");
	LogI("\t-w : capture preprocess input data\n");
	LogI("\t-r : capture pdm input raw data \n");
	LogI("\t-m : capture pdm pcm data\n");
	LogI("\t-o : capture preprocess output data\n");

	LogI("\t-P : select play sound device (card.%d, dev.%d)\n", op->play.c, op->play.d);
	LogI("\t-I : select ref(i2s) sound device (card.%d, dev.%d)\n", op->ref.c, op->ref.d);
	LogI("\t-S : select spi  sound device (card.%d, dev.%d)\n", op->pdm.c, op->pdm.d);

	LogI("\t-t : verify function with %d sec\n", VERIFY_TIME);
	LogI("\t     %s -g 3\n", name);
	LogI("\t ex1) agc on (10dB) & pdm gain 3\n");
	LogI("\t     %s -a 10 -g 3\n", name);
}

static OP_T *parse_options(int argc, char **argv, unsigned int *cmd)
{
	OP_T *op = new OP_T;
	int opt;
	char *c;

	*cmd = CMD_PRE_PROCESS;
	pthread_mutex_init(&op->lock, NULL);

	op->play.c = -1;
	op->play.d  = -1;

#ifdef ANDROID
	op->ref.c = 2, op->ref.d = 0;
	op->pdm.c = 1, op->pdm.d = 0;
#else
	op->ref.c = 0, op->ref.d = 2;
	op->pdm.c = 0, op->pdm.d = 4;
#endif

	while (-1 != (opt = getopt(argc, argv, "his:efa:g:c:wrmotP:S:I:"))) {
		switch(opt) {
        	case 'i':
        		op->do_run_cmd = false;
        		break;
        	case 's':
        		op->dvfs_mhz = strtoul(optarg, NULL, 10);
        		break;
        	case 'e':
        		op->do_preprocess = false;
        		*cmd &= ~CMD_PRE_PROCESS;
        		break;
        	case 'f':
        		op->filter_fast_mode = !op->filter_fast_mode;
        		break;
        	case 'a':
        		op->filter_agc_db = strtoul(optarg, NULL, 10);
        		break;
        	case 'g':
        		op->filter_pdm_gain = strtoul(optarg, NULL, 10);
        		break;
       		case 'c':
       			strcpy(op->file_path, optarg);
       			break;
       		case 'w':
       			*cmd |= (CMD_FILE_CAPT | CMD_FILE_PREP_IN);
       			op->capture |= CMD_FILE_PREP_IN;
       			break;
       		case 'r':
       			*cmd |= (CMD_FILE_CAPT | CMD_FILE_PDM);
       			op->capture |= CMD_FILE_PDM;
       			break;
       		case 'm':
       			*cmd |= (CMD_FILE_CAPT | CMD_FILE_PCM);
       			op->capture |= CMD_FILE_PCM;
       			break;
       		case 'o':
       			*cmd |= (CMD_FILE_CAPT | CMD_FILE_PREP_OUT);
       			op->capture |= CMD_FILE_PREP_OUT;
       			break;
       		case 't':
			op->loop_time = VERIFY_TIME;
			break;
		case 'P':
			c = optarg;
			op->play.c = strtol(c, NULL, 10);
			c = strchr(c, ',');
			if (!c)
				break;
			op->play.d = strtol(++c, NULL, 10);
			break;
		case 'S':
			c = optarg;
			op->pdm.c = strtol(c, NULL, 10);
			c = strchr(c, ',');
			if (!c)
				break;
			op->pdm.d = strtol(++c, NULL, 10);
			break;
		case 'I':
			c = optarg;
			op->ref.c = strtol(c, NULL, 10);
			c = strchr(c, ',');
			if (!c)
				break;
			op->ref.d = strtol(++c, NULL, 10);
			break;
        	default:
        		print_help(argv[0], op);
        		exit(EXIT_FAILURE);
        		break;
	      	}
	}
	return op;
}

static void print_help_cmd(OP_T *op)
{
	LogI("\n");
	LogI("usage: options\n");
	LogI("\t'q'   : Quiet\n");
	LogI("\t's'   : start file capture\n");
	LogI("\t't'   : stop  file capture\n");
	LogI("\t'num' : play out channel (0:AEC, 1:PDM0, 2:PDM1, 3:I2S0, 4:I2S1)\n");
}

static void run_command(OP_T *op)
{
	if (!op->do_run_cmd) {
		while (1) sleep(1);
		return;
	}

	msleep(500);
	for (;;) {
		LOG_PROMPT;
		char s[16], *c = &s[0];
		fgets(s, sizeof(s), stdin);

		switch (*c) {
		case 'h':
			print_help_cmd(op);
			break;
		case 'q':
			return;
		case 's':
			streams_command(op->capture | CMD_FILE_CAPT);
			break;
		case 't':
			streams_command(CMD_FILE_STOP);
			break;
		case 'n':
			streams_command(CMD_STREAM_RESET);
			break;
		case 'd':
			streams_command(CMD_STREAM_NOREF);
			break;
		case 0xa:
			break;
		default:
			op->preprocess_out = atoi(reinterpret_cast<char *>(c));
			break;
		}
	}
}

#ifndef ANDROID
#include <execinfo.h>
static void backtrace_dump(void)
{
	void *array[10];
	size_t size;

	size = backtrace(array, 10);
	backtrace_symbols_fd(array, size, 2);
}
#else
static void backtrace_dump(void) { }
#endif

static void signal_handler(int sig)
{
	switch(sig) {
	case SIGSEGV:
		backtrace_dump();
		exit(EXIT_FAILURE);
		break;
	default:
		break;
	}

	streams_command(CMD_STREAM_EXIT);

	for (auto ls = g_List.begin(); ls != g_List.end(); ++ls) {
		(*ls)->ReleaseAll();
#ifndef ANDROID
		pthread_cancel((*ls)->Handle);
#else
		pthread_kill((*ls)->Handle, SIGUSR1);
#endif
		pthread_join((*ls)->Handle, NULL);
	}

	exit(EXIT_SUCCESS);
}

static void signal_register(void)
{
	struct sigaction sact;

	sact.sa_handler = signal_handler;
  	sigemptyset(&sact.sa_mask);
  	sact.sa_flags = 0;

	sigaction(SIGINT , &sact, 0);
	sigaction(SIGTERM, &sact, 0);
	sigaction(SIGSEGV, &sact, 0);
}

static void app_prepare(OP_T *op)
{
	int mhz = cpu_get_frequency();
	if (!(mhz < 0)) {
		char val[32] = "NO DFS";
		sprintf(val, "%d", mhz/1000);
		if (op->dvfs_mhz) {
			LogI("USERSPACE DVFS [%d][%s -> %d mhz]\n",
				getpid(), val, op->dvfs_mhz);
			cpu_set_frequency(op->dvfs_mhz*1000);
		}
	}

	signal_register();
}

static void stream_dump(void)
{
	int i = 0;
	for (auto ls = g_List.begin(); ls != g_List.end(); ++ls, i++) {
		CAudioStream *S = (*ls);
		const AUDIOPARAM_T *Par = S->GetParam();

		LogI("[%2d] %s \t: 0x%02x, %2d, %2d, %d, %d, %d, %d, %d\n",
			i, S->GetName(), S->GetType(),
			Par->Card, Par->Device, Par->Channels,
			Par->SampleRate, Par->SampleBits,
			Par->Periods, Par->PeriodBytes);
	}
}

static void stream_link(CAudioStream *SS, ...)
{
	int max = MAX_STREAMS;
	CAudioStream *S;
	CAudioStream *Sarray[max];
	int i = 0;

	for (i = 0; i < max; i++)
		Sarray[i] = NULL;

	va_list args;
	va_start(args, SS);

	for (S = SS, i = 0; i < max; S = va_arg(args, CAudioStream *), i++) {
		if (!S) {
			if (i != 0 && Sarray[i-1])
				LogI("[%s]\n", Sarray[i-1]->GetName());
			break;
		}

		Sarray[i] = S;
		if (i == 0)
			continue;

		LogI("[%s] --> ", Sarray[i-1]->GetName());

		auto lw = find(S->LStream.begin(), S->LStream.end(), Sarray[i-1]);
		if (lw != S->LStream.end())
			continue;

		S->LStream.push_back(Sarray[i-1]);
	}

	va_end(args);
}

int main(int argc, char **argv)
{
	OP_T *op;
	unsigned command = 0;
	int err = EXIT_SUCCESS;
	int i = 0;

	op = parse_options(argc, argv, &command);
	if (!op)
		exit(EXIT_FAILURE);

	app_prepare(op);

	CAudioStream SS[] = {
		[0] = { "REF0", STREAM_TYPE_REF, {  op->ref.c, op->ref.d, 2, 16, 48000, 4096, 16 }, fn_capture, op },
		[1] = { "PDM0", STREAM_TYPE_PDM, {  op->pdm.c, op->pdm.d, 2, 16, 64000, 8192,  8 }, fn_capture, op },

		[2] = { "RES0", STREAM_TYPE_REF | STREAM_TYPE_RES, { -1, -1, 2, 16, 16000, 2048, 16 }, fn_resample, op },
		[3] = { "FLT0", STREAM_TYPE_PDM | STREAM_TYPE_FLT, { -1, -1, 4, 16, 16000, 2048, 16 }, fn_filter, op },

		[4] = { "PREP", STREAM_TYPE_PREP, { -1, -1, 2, 16, 16000, 1024, 64 }, fn_process, op },
		[5] = { "PLAY", STREAM_TYPE_OUT , { op->play.c, op->play.d, 2, 16, 16000, 4096, 16 }, fn_playback, op },

		[6] = { "MON" , fn_monitor, op },
		[7] = { "EVT" , fn_event, op },
	};

	for (i = 0; i < static_cast<int>ARRAY_SIZE(SS); i++)
		g_List.push_back(&SS[i]);

	stream_dump();

	/* REF  -> RESAMPLER 0 -> PRE-PRO -> PLAY */
	stream_link(&SS[0], &SS[2], &SS[4], &SS[5], NULL);
	/* PDM0 -> FILTER 0 -> PRE-PRO -> PLAY */
	stream_link(&SS[1], &SS[3], &SS[4], &SS[5], NULL);

#ifdef SUPPORT_PRE_PROCESS
	if (op->do_preprocess) {
		LogI("[DO] pre-processor ...\n");
		aec_mono_init();
	}
#endif
	pthread_setname_np(pthread_self(), "main");

	for (auto ls = g_List.begin(); ls != g_List.end(); ++ls, i++) {
		CAudioStream *S = (*ls);
		pthread_t *th = &S->Handle;

		if (!S->FN)
			continue;

		if (0 != pthread_create(th, NULL, S->FN, reinterpret_cast<void *>(S))) {
	    		LogE("Fail: thread.%d create, %s (%s)!\n",
	    			i, S->GetName(), strerror(errno));
	    		err = EXIT_FAILURE;
  			goto exit_threads;
		}

		cmd_set(S, command);
		pthread_setname_np(S->Handle, S->GetName());
	}

	run_command(op);

exit_threads:
	streams_command(CMD_STREAM_EXIT);

	for (auto ls = g_List.begin(); ls != g_List.end(); ++ls) {
		(*ls)->ReleaseAll();
#ifndef ANDROID
		pthread_cancel((*ls)->Handle);
#else
		pthread_kill((*ls)->Handle, SIGUSR1);
#endif
		pthread_join((*ls)->Handle, NULL);
	}

	return err;
}
