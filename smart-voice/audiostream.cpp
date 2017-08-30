#include "audiostream.h"

CAudioStream::CAudioStream(const char *Name,
			int Type,
			AUDIOPARAM_T Param,
			void *(*Fn)(void *),
			void *Arg)
{
	AUDIOPARAM_T *pParam = &Param;

	m_pName = Name, m_Type = Type, FN = Fn;
	m_pArgument = Arg;

	pthread_mutex_init(&m_Lock, NULL);
	SetParam(pParam);
}

CAudioStream::CAudioStream(const char *Name,
			void *(*Fn)(void *),
			void *Arg)
{
	m_pName = Name, FN = Fn;
	m_pArgument = Arg;

	pthread_mutex_init(&m_Lock, NULL);
}

void CAudioStream::ReleaseAll(void)
{
	m_Release = true;

	ClearBuffer();
	CloseAudio();
	DeleteWavFileAll();
	ReleaseBuffer();
}

bool CAudioStream::OpenAudio(enum AUDIO_STREAM_DIR Direction)
{
	AUDIOPARAM_T *pParam = &m_AudioParam;

	if (m_AudioPlayer)
		return true;

	if (pParam->Card == -1 || pParam->Device == -1)
		return false;

	CAudioPlayer *Audio = new CAudioPlayer;

	bool Ret = Audio->Open(m_pName, pParam->Card, pParam->Device,
			pParam->Channels, pParam->SampleRate, pParam->SampleBits,
			pParam->Periods, pParam->PeriodBytes, Direction);
	if (Ret)
		m_AudioPlayer = Audio;

	LogD("[%s:%d] %s : card:%d.%d %d ch %d hz %2d bits periods %3d bytes %4d, %s\n",
		m_pName, m_Type, Ret ? "DONE" : "FAIL",
		pParam->Card, pParam->Device,
		pParam->Channels, pParam->SampleRate, pParam->SampleBits,
		pParam->Periods, pParam->PeriodBytes,
		Direction == AUDIO_STREAM_PLAYBACK ? "PLAY" : "CAPT");

	return Ret;
}

void CAudioStream::CloseAudio(void)
{
	if (m_AudioPlayer) {
		m_AudioPlayer->Close();
		delete m_AudioPlayer;
		m_AudioPlayer = NULL;
	}
}

bool CAudioStream::StartAudio(void)
{
	if (!m_AudioPlayer)
		return false;

	return m_AudioPlayer->Start();
}

void CAudioStream::StopAudio(void)
{
	if (m_AudioPlayer)
		m_AudioPlayer->Stop();
}

int CAudioStream::PlayAudio(char *Buffer, int Bytes)
{
	AUDIOPARAM_T *pParam = &m_AudioParam;
	int PeriodBytes = pParam->PeriodBytes;

	if (!m_AudioPlayer)
		return 0;

	return m_AudioPlayer->PlayBack((unsigned char*)Buffer, PeriodBytes);
}

int CAudioStream::RecAudio(char *Buffer, int Bytes)
{
	if (!m_AudioPlayer)
		return 0;

	return m_AudioPlayer->Capture((unsigned char*)Buffer, Bytes);
}

void CAudioStream::SetParam(AUDIOPARAM_T *Param)
{
	AUDIOPARAM_T *pParam = &m_AudioParam;

	Lock();
	memcpy(pParam, Param, sizeof(*Param));
	UnLock();
}

const AUDIOPARAM_T *CAudioStream::GetParam(void)
{
	return &m_AudioParam;
};

bool CAudioStream::AllocBuffer(void)
{
	AUDIOPARAM_T *pParam = &m_AudioParam;
	const char *Name = m_pName;
	int Type = m_Type;
	int Periods = pParam->Periods;
	int PeriodBytes = pParam->PeriodBytes;
	bool Ret = true;

	if (!(PeriodBytes * PeriodBytes)) {
		LogE("[%s:%d] no size %d x %d ...\n",
			Name, Type, Periods, PeriodBytes);
		return false;
	}

	Lock();

	if (m_pBuffer)
		goto end_alloc;

	m_pBuffer = new CQueueBuffer(Periods, PeriodBytes, Name, Type);
	if (!m_pBuffer) {
		LogE("[%s:%d] Failed: Buffer allocate !!!\n",
			Name, Type);
		Ret = false;
	}

end_alloc:
	UnLock();
	return Ret;
}

void CAudioStream::ReleaseBuffer(void)
{
	if (!m_pBuffer)
		return;

	Lock();
	delete m_pBuffer;
	m_pBuffer = NULL;
	UnLock();
}

void CAudioStream::ClearBuffer(void)
{
	if (!m_pBuffer && !m_Release)
		AllocBuffer();

	m_pBuffer->ClearBuffer();
}

void CAudioStream::WaitCleanBuffer(void)
{
	if (!m_pBuffer && !m_Release)
		AllocBuffer();

	m_pBuffer->WaitForClear();
}

char *CAudioStream::PushBuffer(int Bytes, int WaitTime)
{
	char *Buffer = NULL;

	if (!m_pBuffer)
		return NULL;

	Buffer = reinterpret_cast<char *>(m_pBuffer->PushBuffer(Bytes, WaitTime));
	if (!Buffer && (unsigned int)WaitTime == TIMEOUT_INFINITE)
		msleep(1);

	return Buffer;
}

bool CAudioStream::RelPushBuffer(int Bytes)
{
	if (!m_pBuffer)
		return false;

	return m_pBuffer->PushRelease(Bytes);
}

char *CAudioStream::PopBuffer(int Bytes, int WaitTime)
{
	char *Buffer = NULL;

	if (!m_pBuffer)
		return NULL;

	Buffer = reinterpret_cast<char *>(m_pBuffer->PopBuffer(Bytes, WaitTime));
	if (!Buffer && (unsigned int)WaitTime == TIMEOUT_INFINITE)
		msleep(1);

	return Buffer;
}

bool CAudioStream::RelPopBuffer(int Bytes)
{
	if (!m_pBuffer)
		return false;

	return m_pBuffer->PopRelease(Bytes);
}

void CAudioStream::SetCommand(unsigned Cmd)
{
	Lock(), m_Command |= Cmd, UnLock();
}

unsigned CAudioStream::GetCommand(unsigned Cmd)
{
	unsigned Command;

	Lock(), Command = m_Command  & Cmd, UnLock();

	return Command;
}

unsigned CAudioStream::GetCommand(void)
{
	unsigned Command = 0;

	Lock(), Command = m_Command, UnLock();

	return Command;
}

void CAudioStream::ClearCommand(unsigned Cmd)
{
	Lock(), m_Command &= ~Cmd, UnLock();
}

WAVFILE_T *CAudioStream::CreateWavFile(int Ch, int Rate, int Bit,
			const char *Path)
{
	CWAVFile *Wav = new CWAVFile(WAV_CAPT_PERIOD);
	if (!Wav) {
		LogE("[%s:%d] Failed: wav file save perido size:%d !!!\n",
			m_pName, m_Type, WAV_CAPT_PERIOD);
		return NULL;
	}

	WAVFILE_T *Hnd = new WAVFILE_T(Wav, Ch, Rate, Bit, Path);
	if (!Hnd) {
		delete Wav;
		LogE("[%s:%d] Failed: wav file ch:%d, rate:%d, bit:%d\n",
			m_pName, m_Type, Ch, Rate, Bit);
		return NULL;
	}

	LWav.push_back(Hnd);

	return Hnd;
}

void CAudioStream::DeleteWavFile(WAVFILE_T *Hnd)
{
	auto lw = find(LWav.begin(), LWav.end(), Hnd);

	if (lw == LWav.end())
		return;

	LWav.erase(lw);
	if (Hnd->hWav)
		delete Hnd->hWav;
	delete Hnd;
}

void CAudioStream::DeleteWavFileAll(void)
{
	CloseAllWavFile();

	for (auto lw = LWav.begin(); lw != LWav.end();) {
		WAVFILE_T *Hnd = *lw;
		LWav.erase(lw++);
		if (Hnd->hWav)
			delete Hnd->hWav;
		delete Hnd;
	}
}

bool CAudioStream::OpenWavFile(WAVFILE_T *Hnd, const char *Fmt, ...)
{
	auto lw = find(LWav.begin(), LWav.end(), Hnd);

	if (lw == LWav.end())
		return false;

	CWAVFile *Wav = (*lw)->hWav;
	char Buf[256] = { 0, }, Path[256] = { 0, };

	va_list args;

	va_start(args, Fmt);
	vsprintf(Path, Fmt, args);
	va_end(args);

	strcat(Buf, Hnd->WavPath);
	strcat(Buf, Path);

	return Wav->Open(Hnd->Channels, Hnd->SampleRate, Hnd->SampleBits, Buf);
}

void CAudioStream::CloseWavFile(WAVFILE_T *Hnd)
{
	auto lw = find(LWav.begin(), LWav.end(), Hnd);

	if (lw == LWav.end())
		return;

	CWAVFile *Wav = (*lw)->hWav;
	Wav->Close();
}

void CAudioStream::CloseAllWavFile(void)
{
	for (auto lw = LWav.begin(); lw != LWav.end(); ++lw) {
		CWAVFile *Wav = (*lw)->hWav;
		Wav->Close();
	}
}

bool CAudioStream::WriteWavFile(WAVFILE_T *Hnd, void *Buffer, size_t Size)
{
	auto lw = find(LWav.begin(), LWav.end(), Hnd);

	if (lw == LWav.end())
		return false;

	CWAVFile *Wav = (*lw)->hWav;

	return Wav->Write(Buffer, Size);
}