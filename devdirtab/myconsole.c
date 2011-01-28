/*
  Helper functions for consolefs
*/

#include <stdio.h>
#include <sys/time.h>
#include "npfs.h"
#include "myconsole.h"

u64
msec(void)
{
	struct timeval t;
	u64 ms;

	gettimeofday(&t, NULL);

	ms = ((u64)t.tv_sec * 1000) + (u64)(t.tv_usec/1000);
	return ms;
}

u64
getclock(void)
{
	return time(0);
}
