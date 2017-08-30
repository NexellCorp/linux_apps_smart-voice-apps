#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/netlink.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/un.h>
#if !defined(ANDROID)
#include <execinfo.h>
#endif

#include "util.h"

/**
 * audio_event_init:
 */
int audio_event_init(void)
{
	struct sockaddr_nl addr;
	int sz = 64*1024;
	int fd;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	addr.nl_groups = 0xffffffff;

	fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (fd < 0)
		return -EINVAL;

	setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(fd);
		return -EINVAL;
	}

	return fd;
}

/**
 * audio_event_close:
 */
void audio_event_close(int fd)
{
	close(fd);
}

/**
 * audio_event_msg:
 */
int audio_event_msg(int fd, char *buffer, int length)
{
	struct pollfd pfd;
	int n, count;

	memset(buffer, 0, length);

	do {
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		n = poll(&pfd, 1, -1);
		if (n > 0  && (POLLIN & pfd.revents)) {
			count = recv(fd, buffer, length, 0);
			if (count > 0)
				return count;
		}
	} while (1);

	/* won't get here */
	return 0;
}

/**
 * sched_new_scheduler:
 * EX> sched_new_scheduler(getpid(), SCHED_FIFO, 99)
 */
int sched_new_scheduler(pid_t pid, int policy, int priority)
{
	struct sched_param param;
	int maxpri = 0, minpri = 0;
	int ret;

	switch (policy) {
	case SCHED_NORMAL:
	case SCHED_FIFO:
	case SCHED_RR:
	case SCHED_BATCH:
		break;
	default:
		LogE("invalid policy %d (0~3)\n", policy);
		return -1;
	}

	if (policy == SCHED_NORMAL) {
		/* #define NICE_TO_PRIO(nice)
		 * (MAX_RT_PRIO + (nice) + 20), MAX_RT_PRIO 100
		 */
		maxpri =  20;
		minpri = -20;	/* nice */
	} else {
		maxpri = sched_get_priority_max(policy);
		minpri = sched_get_priority_min(policy);
	}

	if ((priority > maxpri) || (minpri > priority)) {
		LogE("Invalid priority (%d ~ %d)...\n", minpri, maxpri);
		return -1;
	}

	if (policy == SCHED_NORMAL) {
		param.sched_priority = 0;
		ret = sched_setscheduler(pid, policy, &param);
		if (ret) {
			LogE("set scheduler (%d) %s\n", ret, strerror(errno));
			return ret;
		}
		ret = setpriority(PRIO_PROCESS, pid, priority);
		if (ret)
			LogE("set priority (%d) %s\n",
				ret, strerror(errno));
	} else {
		param.sched_priority = priority;
		ret = sched_setscheduler(pid, policy, &param);
		if (ret)
			LogE("set scheduler (%d) %s\n",
				ret, strerror(errno));
	}
	return ret;
}

/**
 * gettid:
 */
pid_t gettid(void)
{
	return syscall(__NR_gettid);
}

/**
 * sys_write:
 */
int sys_write(const char *file, const char *buffer, int buflen)
{
	int fd, ret = 0;

	if (access(file, F_OK)) {
		LogD("Cannot access file (%s).\n", file);
		return -errno;
	}

	fd = open(file, O_RDWR|O_SYNC);
	if (fd < 0) {
		LogE("Cannot open file (%s).\n", file);
		return -errno;
	}

	ret = write(fd, buffer, buflen);
	if (ret < 0) {
		LogE("Write (file=%s, data=%s)\n", file, buffer);
		ret = -errno;
	}

	close(fd);
	return ret;
}

/**
 * sys_read:
 */
int sys_read(const char *file, char *buffer, int buflen)
{
	int fd, ret = 0;

	if (access(file, F_OK)) {
		LogD("Cannot access file (%s).\n", file);
		return -errno;
	}

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		LogE("Cannot open file (%s).\n", file);
		return -errno;
	}

	ret = read(fd, buffer, buflen);
	if (ret < 0) {
		LogE("Read (file=%s, data=%s)\n", file, buffer);
		ret = -errno;
	}

	close(fd);
	return ret;
}

#define CPU_FREQUENCY_PATH	"/sys/devices/system/cpu"
/**
 * cpu_set_frequency:
 */
int cpu_set_frequency(long khz)
{
	char path[128], data[32];
	int ret;

	/*
	 * change governor to userspace
	 */
	sprintf(path,
		"%s/cpu0/cpufreq/scaling_governor", CPU_FREQUENCY_PATH);
	ret = sys_write(path, "userspace", strlen("userspace"));
	if (ret < 0)
		return ret;

	/*
	 *	change cpu frequency
	 */
	sprintf(path,
		"%s/cpu0/cpufreq/scaling_setspeed", CPU_FREQUENCY_PATH);
	sprintf(data, "%ld", khz);
	ret = sys_write(path, data, strlen(data));
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * cpu_get_frequency:
 */
long cpu_get_frequency(void)
{
	char path[128], data[32];
	int ret;

	sprintf(path, "%s/cpu0/cpufreq/scaling_cur_freq", CPU_FREQUENCY_PATH);
	ret = sys_read(path, data, sizeof(data));
	if (ret < 0)
		return ret;

	return strtol(data, NULL, 10);	 /* khz */
}

