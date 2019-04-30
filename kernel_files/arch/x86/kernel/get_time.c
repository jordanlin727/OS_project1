#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
asmlinkage int get_time(unsigned int *sec, long *nsec)
{
	struct timespec tv;
	getnstimeofday(&tv);
	*sec = tv.tv_sec;
	*nsec = tv.tv_nsec;
	return 1;
}
