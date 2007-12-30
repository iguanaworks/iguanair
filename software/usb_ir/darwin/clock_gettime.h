/*
 * noah wiliamsson / noah(at)hd.se
 * March 10 '07
 *
 */
#ifndef _IGUANAWORKS_EMUL_CLOCK_GETTIME
#define _IGUANAWORKS_EMUL_CLOCK_GETTIME

#include <mach/mach_time.h>

#define CLOCK_MONOTONIC 1

static inline int clock_gettime(int UNUSED(clock), struct timespec *tp) {
	static mach_timebase_info_data_t mtid;
	static struct timeval tv;
	static uint64_t first_mat;
	uint64_t elapsed_ns;

	if(!mtid.denom) {
		mach_timebase_info(&mtid);
		gettimeofday(&tv, NULL);
		first_mat = mach_absolute_time();
	}

	elapsed_ns = (mach_absolute_time() - first_mat) * mtid.numer / mtid.denom;
	tp->tv_sec = tv.tv_sec + elapsed_ns / 1000000000;
	tp->tv_nsec = tv.tv_usec*1000 + elapsed_ns % 1000000000;
	if(tp->tv_nsec >= 1000000000) {
		tp->tv_sec++;
		tp->tv_nsec -= 1000000000;
	}

	return 0;
}

/*
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include "clock_gettime.h"

int main(int a, char **b) {
        struct timespec t1, t2;

        clock_gettime(0, &t1);
        printf("t1: sec:%lu ns:%lu\n", t1.tv_sec, t1.tv_nsec);
        sleep(1);
        clock_gettime(0, &t2);
        printf("t2: sec:%lu ns:%lu\n", t2.tv_sec, t2.tv_nsec);
        return 0;
}
*/

#endif
