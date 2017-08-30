#ifndef _Q_BUFF_H_
#define _Q_BUFF_H_

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#ifdef ANDROID
#else
#include <vector>
#include <iostream>
#endif
#include <fcntl.h>
#include <assert.h>
#include <sys/time.h>

#include "util.h"

#if __cplusplus
extern "C" {
#endif

#define	ATOMIC_LOCK
#define	TIMEOUT_INFINITE	(0xFFFFFFFF)

class CQueueBuffer {
	public:
	CQueueBuffer(int Frames, int FrameBytes) {
		Qbuffer(Frames, FrameBytes);
		memset(m_Name, 0, sizeof(m_Name));
		m_Type = 0;
	}

	virtual ~CQueueBuffer(void) { delete m_pBuffer;	}

	CQueueBuffer(int Frames, int FrameBytes,
			const char *Name, unsigned int Type) {
		Qbuffer(Frames, FrameBytes);
		if (Name && strlen(Name))
			strcpy(m_Name, Name);
		m_Type = Type;
	}

	public:
	int GetFrameCounts(void) { return m_FrameCounts; }
	int GetFrameBytes(void) { return m_FrameBytes; }
	int GetBufferBytes(void) { return m_BufferBytes; }

	char *GetName(void) { return m_Name; }
	int GetType(void) { return m_Type; }

	int GetAvailSize(void) { return m_PushAvailSize; }
	bool Is_PushAvail(void) { return m_bPushAvail; }
	bool Is_PopAvail(void) { return m_bPopAvail;  }

	public:
	bool  Is_Full(void) {
		pthread_mutex_lock(&m_Lock);
		bool full = (!m_PushAvailSize ? true : false);
		pthread_mutex_unlock(&m_Lock);

		return full;
	}

	bool  Is_Empty(void) {
		pthread_mutex_lock(&m_Lock);
		bool empty = (m_BufferBytes == m_PushAvailSize ? true : false);
		pthread_mutex_unlock(&m_Lock);

		return empty;
	}

	/* with frame bytes */
	unsigned char *PushBuffer(void) {
		unsigned char *Buffer = NULL;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPushReady) {
			pthread_mutex_unlock(&m_Lock);
			return Buffer;
		}

		if (!m_bPushAvail)
			pthread_cond_wait(&m_PopEvent, &m_Lock);

		if (m_bPushReady)
			Buffer = (unsigned char *)(m_pBuffer + m_HeadPos);

		pthread_mutex_unlock(&m_Lock);

		return Buffer;
	}

	/* with frame bytes */
	bool PushRelease(void) {
		bool bPushAvail = false;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPushReady) {
			pthread_mutex_unlock(&m_Lock);
			return bPushAvail;
		}

		if (!m_bPushAvail)
			pthread_cond_wait(&m_PopEvent, &m_Lock);

		if (m_bPushReady) {
			m_HeadPos += m_FrameBytes;

			if (m_HeadPos >= m_BufferBytes)
				m_HeadPos = 0;

			m_bPushAvail = m_HeadPos == m_TailPos ? false : true;
			m_bPopAvail = true;
			bPushAvail = m_bPushAvail;
		}

		pthread_cond_signal(&m_PushEvent);
		pthread_mutex_unlock(&m_Lock);

		return bPushAvail;
	}

	/* with frame bytes unit */
	unsigned char *PopBuffer(void) {
		unsigned char *Buffer = NULL;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPopReady) {
			pthread_mutex_unlock(&m_Lock);
			return Buffer;
		}

		if (!m_bPopAvail)
			pthread_cond_wait(&m_PushEvent, &m_Lock);

		if (m_bPopReady)
			Buffer = (unsigned char *)(m_pBuffer + m_TailPos);

		pthread_mutex_unlock(&m_Lock);

		return Buffer;
	}

	/* with frame bytes unit */
	bool PopRelease(void) {
		bool bPopAvail = false;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPopReady) {
			pthread_mutex_unlock(&m_Lock);
			return bPopAvail;
		}

		if (!m_bPopAvail)
			pthread_cond_wait(&m_PushEvent, &m_Lock);

		if (m_bPopReady) {
			m_TailPos += m_FrameBytes;

			if (m_TailPos >= m_BufferBytes)
				m_TailPos = 0;

			m_bPushAvail = true;
			m_bPopAvail = m_HeadPos == m_TailPos ? false : true;
			bPopAvail = m_bPopAvail;
		}

		pthread_cond_signal(&m_PopEvent);
		pthread_mutex_unlock(&m_Lock);

		return bPopAvail;
	}

	unsigned char *PushBuffer(int Size, int Wait) {	/* ms */
		unsigned char *Buffer = NULL;
		struct timespec ts;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPushReady || Size <= 0) {
			pthread_mutex_unlock(&m_Lock);
			return Buffer;
		}

		if ((m_HeadPos + Size) > m_BufferBytes) {
			LogE("%s : Error '%s' %d size %d pos, over buffer: %d, check align!!!\n",
				__func__, m_pBuffer, Size, m_HeadPos, m_BufferBytes);
			assert((m_HeadPos + Size) <= m_BufferBytes);
		}

		while (Size > m_PushAvailSize && m_bPushReady) {
			if ((unsigned int)Wait == TIMEOUT_INFINITE) {
				pthread_cond_wait(&m_PopEvent, &m_Lock);
				continue;
			}

			get_pthread_wait_time(&ts, Wait);
			int ret = pthread_cond_timedwait(&m_PopEvent, &m_Lock, &ts);
			if (ret == ETIMEDOUT) {
				pthread_mutex_unlock(&m_Lock);
				return Buffer;
			}
		}

		if (m_bPushReady)
			Buffer = (unsigned char *)(m_pBuffer + m_HeadPos);

		pthread_mutex_unlock(&m_Lock);

		return Buffer;
	}

	bool PushRelease(int Size) {
		bool bPushAvail = false;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPushReady || Size <= 0) {
			pthread_mutex_unlock(&m_Lock);
			return bPushAvail;
		}

		while (m_PushAvailSize < Size && m_bPushReady)
			pthread_cond_wait(&m_PopEvent, &m_Lock);

		if (m_bPushReady) {
			if ((m_HeadPos + Size) > m_BufferBytes) {
				LogE("%s : Error '%s' %d size %d pos, over buffer: %d, check align!!!\n",
					__func__, m_pBuffer, Size, m_HeadPos, m_BufferBytes);
				assert((m_HeadPos + Size) <= m_BufferBytes);
			}

			m_PushAvailSize -= Size;
			m_HeadPos = (m_HeadPos + Size) % m_BufferBytes;

			m_bPushAvail = m_PushAvailSize > 0 ? true : false;
			m_bPopAvail = true;
			bPushAvail = m_bPushAvail;
		}

		pthread_cond_signal(&m_PushEvent);
		pthread_mutex_unlock(&m_Lock);

		return bPushAvail;
	}

	unsigned char *PopBuffer(int Size, int Wait) { /* ms */
		unsigned char *Buffer = NULL;
		struct timespec ts;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPopReady) {
			pthread_mutex_unlock(&m_Lock);
			return Buffer;
		}

		if ((m_TailPos + Size) > m_BufferBytes) {
			LogE("%s : Error '%s' %d size %d pos, over buffer: %d, check align!!!\n",
				__func__, m_pBuffer, Size, m_TailPos, m_BufferBytes);
			assert((m_TailPos + Size) <= m_BufferBytes);
		}

		while ((m_BufferBytes - m_PushAvailSize) < Size && m_bPopReady) {
			if ((unsigned int)Wait == TIMEOUT_INFINITE) {
				pthread_cond_wait(&m_PushEvent, &m_Lock);
				continue;
			}

			get_pthread_wait_time(&ts, Wait);
			int ret = pthread_cond_timedwait(&m_PushEvent, &m_Lock, &ts);
			if (ret == ETIMEDOUT) {
				pthread_mutex_unlock(&m_Lock);
				return Buffer;
			}
		}

		if (m_bPopReady)
			Buffer = (unsigned char *)(m_pBuffer + m_TailPos);

		pthread_mutex_unlock(&m_Lock);

		return Buffer;
	}

	bool PopRelease(int Size) {
		bool bPopAvail = false;

		pthread_mutex_lock(&m_Lock);

		if (!m_pBuffer || !m_bPopReady || Size <= 0) {
			pthread_mutex_unlock(&m_Lock);
			return bPopAvail;
		}

		while ((m_BufferBytes - m_PushAvailSize) < Size && m_bPopReady)
			pthread_cond_wait(&m_PushEvent, &m_Lock);

		if (m_bPopReady) {
			if ((m_TailPos + Size) > m_BufferBytes) {
				LogE("%s : Error '%s' %d size %d pos, over buffer: %d, check align!!!\n",
					__func__, m_pBuffer, Size, m_TailPos, m_BufferBytes);
				assert((m_TailPos + Size) <= m_BufferBytes);
			}

			m_PushAvailSize += Size;
			m_TailPos = (m_TailPos + Size) % m_BufferBytes;

			m_bPushAvail = true;
			m_bPopAvail = (m_BufferBytes - m_PushAvailSize) > 0 ? true : false;
			bPopAvail = m_bPopAvail;
		}

		pthread_cond_signal(&m_PopEvent);
		pthread_mutex_unlock(&m_Lock);

		return bPopAvail;
	}

	/*
	 * PushBuffer side :
	 * must be call 'WaitForClear' at PopBuffer side
	 */
	void ClearBuffer(void) {
		pthread_mutex_lock(&m_Lock);

		m_bPopReady = false;
		pthread_cond_signal(&m_PushEvent);
		pthread_cond_signal(&m_PopEvent);

		/* initialize */
		m_HeadPos = 0, m_TailPos = 0;
		m_bPushAvail = true, m_bPopAvail = false;
		m_PushAvailSize = m_BufferBytes;
		memset(m_pBuffer, 0, m_AlignBufferBytes);

		assert(m_pBuffer);
		assert(m_AlignBufferBytes);

		m_bWaitReady = true;
		m_bPushReady = true;

		pthread_cond_signal(&m_ClearEvent);
		pthread_mutex_unlock(&m_Lock);
	}

	/*
	 * PopBuffer side:
	 * must be call 'ClearBuffer' at PushBuffer side
	 */
	void WaitForClear(void) {
		pthread_mutex_lock(&m_Lock);

		if (!m_bWaitReady) {
			m_bPushReady = false;
			pthread_cond_signal(&m_PushEvent);
			pthread_cond_signal(&m_PopEvent);
			/* wait clear event */
			pthread_cond_wait(&m_ClearEvent, &m_Lock);
		}

		m_bWaitReady = false;
		m_bPopReady = true;

		pthread_mutex_unlock(&m_Lock);
	}

	private:
	void Qbuffer(int Frames, int FrameBytes) {
		int align_bytes = (((Frames * FrameBytes) + 3) / 4) * 4;

		m_pBuffer = (unsigned char *)new unsigned int[align_bytes/4];

		m_FrameCounts = Frames;
		m_FrameBytes = FrameBytes;
		m_BufferBytes = Frames * FrameBytes;
		m_AlignBufferBytes = align_bytes;

		m_HeadPos = 0, m_TailPos = 0;
		m_bPushAvail = true, m_bPopAvail = false;
		m_PushAvailSize = m_BufferBytes;

		m_bWaitReady = false;
		m_bPushReady = true;
		m_bPopReady = true;

		assert(m_pBuffer);
		assert(m_AlignBufferBytes);

		memset(m_pBuffer, 0, m_AlignBufferBytes);

		pthread_mutex_init(&m_Lock, NULL);

		pthread_cond_init(&m_PushEvent, NULL);
		pthread_cond_init(&m_PopEvent, NULL);
		pthread_cond_init(&m_ClearEvent, NULL);
	}

	void get_pthread_wait_time(struct timespec *ts, int ms) {
		struct timeval tv;

		gettimeofday(&tv, NULL);

		ts->tv_sec = time(NULL) + ms / 1000;
		ts->tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (ms % 1000);
		ts->tv_sec += ts->tv_nsec / (1000 * 1000 * 1000);
		ts->tv_nsec %= (1000 * 1000 * 1000);
	}

	private:
	unsigned char *m_pBuffer;
	int m_FrameCounts;
	int m_FrameBytes;
	int m_BufferBytes;
	int m_AlignBufferBytes;
	char m_Name[256];
	unsigned int m_Type;

	/* Q buffer status */
	int m_HeadPos;	/* push */
	int m_TailPos;	/* Pop */
	bool m_bPushAvail;
	bool m_bPopAvail;
	int m_PushAvailSize;

	/* Q status */
	bool m_bWaitReady;
	bool m_bPushReady, m_bPopReady;

	pthread_mutex_t m_Lock;
	pthread_cond_t m_PushEvent;
	pthread_cond_t m_PopEvent;
	pthread_cond_t m_ClearEvent;
};

#if __cplusplus
}
#endif
#endif /* _Q_BUFF_H_ */

