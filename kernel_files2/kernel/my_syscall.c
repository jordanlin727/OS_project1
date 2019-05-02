#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/syscalls.h>

const long long t = 1E9;

SYSCALL_DEFINE5(my_print, int, pid, long, st_s, long, st_ns, long, et_s, long, et_ns)
{
	printk("[project1] %d %ld.%09ld %ld.%09ld\n", pid, st_s, st_ns, et_s, et_ns);
	return 0;
}

SYSCALL_DEFINE0(my_time)
{
	struct timespec now;
	long long time;
	getnstimeofday(&now);
	time = now.tv_sec * t + now.tv_nsec;
	return time;
}

SYSCALL_DEFINE2(my_add, int, a, int, b)
{
	printk("%d\n", a + b);
	return 0;
}
