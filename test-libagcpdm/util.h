#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <signal.h>
#include <sched.h>
#include <linux/sched.h> /* SCHED_NORMAL, SCHED_FIFO, SCHED_RR, SCHED_BATCH */
#include <sys/time.h>
#include <sys/resource.h>

#if __cplusplus
extern "C" {
#endif

/**
 * audio event
 */
int audio_event_init(void);
void audio_event_close(int fd);
int audio_event_msg(int fd, char *buffer, int length);

/**
 * RT scheduler
 */
int sched_new_scheduler(pid_t pid, int policy, int priority);

/**
 * dvfs
 */
int cpu_set_frequency(long khz);
long cpu_get_frequency(void);

/**
 * util
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
pid_t gettid(void);
int sys_write(const char *file, const char *buffer, int buflen);
int sys_read(const char *file, char *buffer, int buflen);

/**
 * times
 */
#define	msleep(m)	usleep(m*1000)

#define	RUN_TIMESTAMP_US(s) {		\
		struct timeval tv;	\
		gettimeofday(&tv, NULL);	\
		s = (tv.tv_sec*1000000) + (tv.tv_usec);	\
	}

#define	END_TIMESTAMP_US(s, d) { \
		struct timeval tv;	\
		gettimeofday(&tv, NULL);	\
		d = (tv.tv_sec*1000000) + (tv.tv_usec);	\
		d = d - s;	\
	}

/**
 * debug
 */
#ifndef ANDROID
#define	LogI(format, ...) do { \
		fprintf(stdout, format, ##__VA_ARGS__); \
	} while (0)
#define	LogE(format, ...) do { \
		fprintf(stderr, "Error : %s:%d: ", __func__, __LINE__); \
		fprintf(stderr, format, ##__VA_ARGS__); \
	} while (0)

#ifdef DEBUG
#define	LogD(format...) do { \
		fprintf(stdout, format); \
	} while (0)
#else
#define	LogD(m...)	do { } while (0)
#endif
#else
#define	LogI(format, ...) do { \
		printf(format, ##__VA_ARGS__); \
	} while (0)
#define	LogE(format, ...) do { \
		printf("Error : %s:%d: ", __func__, __LINE__); \
		printf(format, ##__VA_ARGS__); \
	} while (0)

#ifdef DEBUG
#define	LogD(format...) do { \
		printf(format); \
	} while (0)
#else
#define	LogD(m...)	do { } while (0)
#endif
#endif

#if __cplusplus
}
#endif
#endif /* _UTIL_H_ */

