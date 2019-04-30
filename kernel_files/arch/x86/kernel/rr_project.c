#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/time.h>
//asmlinkage int rr_project(char *project_name, int pid, struct timeval *start, struct timeval *end)
asmlinkage int rr_project(char *project_name, int pid, unsigned int start_sec, long start_nsec, unsigned int end_sec, long end_nsec)
{
	printk("[%s] %d %ld.%ld %ld.%ld\n",project_name, pid, start_sec, start_nsec, end_sec, end_nsec);
	return 1;
}
