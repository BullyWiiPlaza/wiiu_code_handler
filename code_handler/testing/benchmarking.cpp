#include <bits/types/struct_timeval.h>
#include <sys/time.h>
#include "benchmarking.h"

double get_elapsed_time(struct timeval *start_time) {
	struct timeval end_time{};
	get_time(&end_time);

	double elapsedTime = (end_time.tv_sec - start_time->tv_sec) * 1000.0;
	elapsedTime += (end_time.tv_usec - start_time->tv_usec) / 1000.0;
	return elapsedTime;
}

int get_time(struct timeval *time) {
	return gettimeofday(time, nullptr);
}