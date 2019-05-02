#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/kernel.h>
#include <errno.h>
#include <limits.h>

const long long t = 1E9;
const int time_quantum = 5E2;

typedef struct {
	char name[32];
	int ready_time;
	int exec_time;
	int forked;
	int startwork_time;
	pid_t pid;
} process;
// sort by ready time
int compare_process(const void *A, const void *B)
{
	process *p1 = (process *)A;
	process *p2 = (process *)B;
	return (p1->ready_time - p2->ready_time);
}
// add process to queue
void push_process(process **queue, process *p, int *num_ps_now)
{
	*num_ps_now = *num_ps_now + 1;
	queue[(*num_ps_now - 1)] = p;
}
// remove process that is terminated
void pop_process(process **queue, int  *num_ps_now)
{
	*num_ps_now = *num_ps_now - 1;
	queue[0] = NULL;
	for (int i = 0; i < *num_ps_now; i++) {
		process *temp = queue[i + 1];
		queue[i + 1] = queue[i];
		queue[i] = temp;
	}
}
// decide the order of the queue for round robin
int pop_rr(process **queue, int *num_ps_now) // return how long the process should be executed
{
	int exec_rr;
	if (queue[0]->exec_time > time_quantum) {
		exec_rr = time_quantum;
		queue[0]->exec_time -= time_quantum;
	} else {
		exec_rr = queue[0]->exec_time;
		queue[0]->exec_time = 0;
		queue[0] = NULL; // execution is finished
	}
	for (int i = 1; i < *num_ps_now; i++) {
		process *temp = queue[i];
		queue[i] = queue[i - 1];
		queue[i - 1] = temp;
	}
	if (queue[*num_ps_now - 1] == NULL) {
		*num_ps_now = *num_ps_now - 1;
	}
	return exec_rr;
}
// exit with printing error message
void err_sys(char *errmsg) 
{
	perror(errmsg);
	exit(EXIT_FAILURE);    
}
// fifo scheduling 
void fifo(process *ps, int num_ps)
{
	process **queue = malloc(num_ps * sizeof(process*)); // queue for scheduling 
	for (int i = 0; i < num_ps; i++)
		queue[i] = NULL;    
	/* set priority for the process */
	struct sched_param param;
	const int low_prior = sched_get_priority_min(SCHED_FIFO);
	const int high_prior = sched_get_priority_max(SCHED_FIFO);
	param.sched_priority = (low_prior + high_prior) / 2;
	pid_t pid_p = getpid();
	if (sched_setscheduler(pid_p, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler() failed");
		exit(1);
	}

	int time_unit = 0, fork_count = 0, num_ps_finish = 0, num_ps_now = 0;
	while (num_ps > num_ps_finish) {
		/* push the process ready for execution */
		while (ps[fork_count].ready_time <= time_unit && fork_count < num_ps) {
			push_process(queue, &ps[fork_count], &num_ps_now);
			/* run the process */
			// printf("%s : start at time %d\n", ps[fork_count].name, time_unit);
			long int st, et;
			long st_s, st_ns, et_s, et_ns;
			st = syscall(336);
			st_s = st / t;
			st_ns = st % t;
			pid_t pid = fork();
			if (pid < 0) {
				perror("fork failed");
				exit(1);
			}
			/* child process */
			else if (pid == 0) {
				for (int j = 0; j < ps[fork_count].exec_time; j++) {
					volatile unsigned long i;
					for (i = 0; i < 1000000UL; i++);
				}
				et = syscall(336);
				et_s = et / t;
				et_ns = et % t;
				syscall(335, getpid(), st_s, st_ns, et_s, et_ns);
				// printf("%d end\n", getpid());
				exit(0);
			}
			/* parent process */
			/* set CPU for child process to CPU 1 */
			cpu_set_t cpu_mask;
			CPU_ZERO(&cpu_mask);
			CPU_SET(1, &cpu_mask);
			if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_mask) == -1) {
				perror("sched_setaffinity error");
				exit(1);
			}
			ps[fork_count].pid = pid;
			fork_count++;
		}

		/* no process to run */
		if (queue[0] == NULL) {
			time_unit++;
			volatile unsigned long i;
			for(i = 0; i < 1000000UL; i++);
		} else {
			time_unit++;
			queue[0]->exec_time = queue[0]->exec_time - 1;
			volatile unsigned long i;
			for (i = 0; i < 1000000UL; i++);
		}

		/* process in queue[0] should terminate */
		if (queue[0] != NULL && queue[0]->exec_time == 0) {
			/* wait if the process is not terminated */
			int status;
			if (waitpid(queue[0]->pid, &status, 0) == -1) {
				perror("waitpid error");
				exit(1);
			}
			// printf("%s : end at time %d\n", queue[0]->name, time_unit);
			pop_process(queue, &num_ps_now);
			num_ps_finish++;
		}
	}
	free(queue);
}
// decide time to start execution for each process
void sfj_process_startwork_time(process *ps, int num_ps)
{
	int CPU_ready_time = 0, finish_count = 0;
	CPU_ready_time += ps[0].ready_time; // align earliest ready time
	int finish[num_ps];
	for (int i = 0; i < num_ps; i++)
		finish[i] = 0;
	while (finish_count != num_ps) {
		unsigned int cur_shortest_job = 0;
		unsigned int earliest_finish_time = -1; //maximum number of unsigned integer
		for (int i = 0; i < num_ps; i++) {
			if (finish[i] == 0) {
				int finish_time;
				if (CPU_ready_time >= ps[i].ready_time)
					finish_time = CPU_ready_time + ps[i].exec_time;
				else
					continue;
				if (finish_time < earliest_finish_time) {
					cur_shortest_job = i;
					earliest_finish_time = finish_time;
				}
			}
		}
		ps[cur_shortest_job].startwork_time = CPU_ready_time;
		CPU_ready_time = earliest_finish_time;
		finish[cur_shortest_job] = 1;
		finish_count++;
	}
}
// shortest job first scheduling
void sjf(process *ps, int num_ps)
{
	int CPU_time = 0;
	sfj_process_startwork_time(ps, num_ps); //compute process' start time
	/* main process */
	for (int i = 0; i < num_ps; i++) {
		//wait until next process is ready to fork
		if (CPU_time < ps[i].ready_time) {
			for (int j = 0; j < ps[i].ready_time - CPU_time; j++)
				for (volatile unsigned long k = 0; k < 1000000UL; k++);
			CPU_time = ps[i].ready_time;
		}
		// printf("%s : start at time %d\n", ps[i].name, CPU_time);
		long int st, et;
		long st_s, st_ns, et_s, et_ns;
		st = syscall(336);
		st_s = st / t;
		st_ns = st % t;
		// fork
		if ((ps[i].pid = fork()) == 0) {
			// sleep
			for (int j = 0; j < ps[i].startwork_time - ps[i].ready_time; j++)
				for (volatile unsigned long k = 0;k < 1000000UL; k++);

			// set CPU affinity
			cpu_set_t mask;
			CPU_ZERO(&mask);
			CPU_SET(0, &mask);
			sched_setaffinity(getpid(), sizeof(mask), &mask);

			// executing
			for (int j = 0; j < ps[i].exec_time; j++) {
				for(volatile unsigned long k=0;k<1000000UL;k++);
			}

			//print finish time
			et = syscall(336);
			et_s = et / t;
			et_ns = et % t;
			syscall(335, getpid(), st_s, st_ns, et_s, et_ns);
			exit(0);
		}		
	}

	//wait for all child process
	for (int i = 0; i < num_ps; i++)
		wait(NULL);
}
// preemptive shortest job first scheduling
int psjf(process *ps, int num_ps)
{
	int status, current_process, min_ready_time = INT_MAX;
	pid_t pid = getpid(), p_pid = getpid();

	/* find the time for first process to start */
	for (int i = 0; i < num_ps; i++) {
		if (ps[i].ready_time < min_ready_time){
			min_ready_time = ps[i].ready_time;
			current_process = i;
		}
		ps[i].forked = 0;
	}
	int cur_time;
	/* wait for first process to start */
	for (cur_time = 0; cur_time < min_ready_time; cur_time++) {
		volatile unsigned long j; for (j = 0; j < 1000000UL; j++);
	}
	cur_time += min_ready_time;

	waitpid(pid, &status, WUNTRACED);
	long int st[num_ps], et[num_ps];
	long st_s[num_ps], st_ns[num_ps], et_s[num_ps], et_ns[num_ps];
	int order[num_ps]; // finish order
	int index = 0, found = 1;
	while (found) {
		found = 0;
		int min_exectime = INT_MAX;
		for (int i = 0;i < num_ps; i++) {
			if ((cur_time >= ps[i].ready_time) && (ps[i].exec_time > 0) && (ps[i].exec_time < min_exectime)) {
				current_process = i;
				min_exectime = ps[i].exec_time;
				found = 1;
			}
		}
		if (found == 0) break;
		if (!ps[current_process].forked) {
			// printf("%s : start at time %d\n", ps[current_process].name, cur_time);
			st[current_process] = syscall(336);
			st_s[current_process] = st[current_process] / t;
			st_ns[current_process] = st[current_process] % t;
			if ((ps[current_process].pid = pid = fork()) < 0)
				err_sys("fork");
			if (pid == 0) {
				raise(SIGSTOP);
				while (1) {
					volatile unsigned long i; for(i = 0; i < 1000000UL; i++);
					raise(SIGSTOP);
				}       
			} else {
				if (p_pid != getpid()) return 0;
				if (waitpid(ps[current_process].pid, &status, WUNTRACED) < -1)
					err_sys("waitpid");
				if (WIFSTOPPED(status))
					kill(ps[current_process].pid, SIGCONT);
				if (waitpid(ps[current_process].pid, &status, WUNTRACED) < -1)
					err_sys("waitpid");
				ps[current_process].forked = 1;
			}
		} else {
			kill(ps[current_process].pid, SIGCONT);
			if (waitpid(ps[current_process].pid, &status, WUNTRACED) < -1)
				err_sys("waitpid");            
		}
		cur_time++;
		ps[current_process].exec_time--;
		if (ps[current_process].exec_time == 0) {
			kill(ps[current_process].pid, SIGINT);
			//print finish time
			et[current_process] = syscall(336);
			et_s[current_process] = et[current_process] / t;
			et_ns[current_process] = et[current_process] % t;
			order[index] = current_process;
			index++;
		}
	}
	int j;
	for (int i = 0; i < num_ps; i++) {
		j = order[i];
		syscall(335, ps[j].pid, st_s[j], st_ns[j], et_s[j], et_ns[j]);
	}
	return 0;
}
// round robin scheduling
void rr(process *ps, int num_ps) 
{
	process **queue = malloc(num_ps * sizeof(process*)); // queue for scheduling 
	for (int i = 0; i < num_ps; i++)
		queue[i] = NULL;    
	/* set priority for the process */
	struct sched_param param;
	const int low_prior = sched_get_priority_min(SCHED_FIFO);
	const int high_prior = sched_get_priority_max(SCHED_FIFO);
	param.sched_priority = (low_prior + high_prior) / 2;
	pid_t pid_p = getpid();
	if (sched_setscheduler(pid_p, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler() failed");
		exit(1);
	}

	int time_count = 0, fork_count = 0, exec_rr = 0;
	int num_ps_now = 0, num_ps_finish = 0;
	process *ps_now = NULL;
	process *ps_last = NULL;
	while (!(num_ps_finish == num_ps && exec_rr == 0)) {
		/* push the process ready for execution */
		while (ps[fork_count].ready_time <= time_count && fork_count < num_ps) {
			push_process(queue, &ps[fork_count], &num_ps_now);
			// printf("%s : start at time %d\n", ps[fork_count].name, time_count);
			/* run the process */
			long int st, et;
			long st_s, st_ns, et_s, et_ns;
			st = syscall(336);
			st_s = st / t;
			st_ns = st % t;
			pid_t pid = fork();
			if (pid < 0) {
				perror("fork failed");
				exit(1);
			}
			// child
			else if (pid == 0) {
				for (int j = 0; j < ps[fork_count].exec_time; j++) {
					volatile unsigned long i;
					for (i = 0; i < 1000000UL; i++);
				}
				et = syscall(336);
				et_s = et / t;
				et_ns = et % t;
				syscall(335, getpid(), st_s, st_ns, et_s, et_ns);
				// printf("%d end\n", getpid());
				exit(0);
			}
			// parent
			// run the child process on CPU 1
			cpu_set_t cpu_mask;
			CPU_ZERO(&cpu_mask);
			CPU_SET(1, &cpu_mask);
			if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_mask) == -1) {
				perror("sched_setaffinity error");
				exit(1);
			}
			ps[fork_count].pid = pid;
			fork_count++;
		}

		// no process is waiting
		if (queue[0] == NULL && exec_rr == 0) {
			time_count++;
			volatile unsigned long i;
			for (i = 0; i < 1000000UL; i++);
			ps_last = NULL;
		} else {
			if (exec_rr == 0) {
				ps_now = queue[0]; // execute next process in the queue
				exec_rr = pop_rr(queue, &num_ps_now);
				//change priority if a different process is going to run
				if (ps_last == NULL || ps_last->exec_time == 0) {
					param.sched_priority = high_prior;
					if (sched_setscheduler(ps_now->pid, SCHED_RR, &param) != 0) {
						perror("sched_setscheduler error");
						exit(1);
					}
				} else {
					// lower the priority of last process
					param.sched_priority = low_prior;
					if(sched_setscheduler(ps_last->pid, SCHED_RR, &param) != 0) {
						perror("sched_setscheduler error");
						exit(EXIT_FAILURE);
					}
					param.sched_priority = high_prior;
					if(sched_setscheduler(ps_now->pid, SCHED_FIFO, &param) != 0) {
						perror("sched_setscheduler error");
						exit(EXIT_FAILURE);
					}
				}
			}

			exec_rr--;
			time_count++;
			volatile unsigned long i;
			for (i = 0; i < 1000000UL; i++);

			if (exec_rr == 0) {
				// if a process should terminate
				if (ps_now->exec_time == 0) {
					// printf("%s : end at time %d\n", ps_now->name, time_count);
					int status;
					if (waitpid(ps_now->pid, &status, 0) == -1) {
						perror("waitpid error");
						exit(EXIT_FAILURE);
					}
					num_ps_finish++;
				}
				ps_last = ps_now;
			}
		}
	}
	free(queue);
}

int main(int argc, char *argv[])
{
	// input
	char schedule[10];
	scanf("%s",schedule);
	int num_ps;
	scanf("%d",&num_ps);
	process *ps = malloc(num_ps * sizeof(process));
	for (int i = 0; i < num_ps; i++)
		scanf("%s %d %d", ps[i].name, &ps[i].ready_time, &ps[i].exec_time);
	qsort(ps, num_ps, sizeof(process), compare_process); // sort by start time

	/* setting CPU affinity */
	cpu_set_t cpu_mask;
	CPU_ZERO(&cpu_mask);
	CPU_SET(0, &cpu_mask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask) != 0) {
		perror("sched_setaffinity error");
		exit(1);
	}

	// which scheduling process to run
	switch (schedule[0]) {
		case 'F':
			if (strcmp("FIFO", schedule) == 0)
				fifo(ps, num_ps);
			break;
		case 'R':
			if (strcmp("RR", schedule) == 0)
				rr(ps, num_ps);
			break;
		case 'S':
			if (strcmp("SJF", schedule) == 0)
				sjf(ps, num_ps);
			break;
		case 'P':
			if (strcmp("PSJF", schedule) == 0)
				psjf(ps, num_ps);
			break;
	}

	//output
	for (int i = 0; i < num_ps; i++)
		printf("%s %d\n", ps[i].name, ps[i].pid);
	free(ps);
	return 0;
}
