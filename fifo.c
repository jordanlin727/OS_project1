#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/kernel.h>

const long long t = 1E9;
int numps_finish = 0;
int numps_now = 0;

typedef struct {
    int st;
    int ft;
    pid_t pid;
    char name[32];
} ps;

int compare_process(const void *A, const void *B)
{
    ps *p1 = (ps *)A;
    ps *p2 = (ps *)B;
    return p1->st - p2->st;
}
/* add process to queue */
void push_process(ps **queue, ps *p)
{
    numps_now++;
    queue[numps_now - 1] = p;
}
/* remove process that is terminated */
void pop_process(ps **queue)
{
    numps_now--;
    queue[0] = NULL;
    for (int i = 0; i < numps_now; i++) {
        ps *temp = queue[i + 1];
        queue[i + 1] = queue[i];
        queue[i] = temp;
    }
}

int main()
{
    char policy[10];
    int num_ps;
    scanf("%s", policy);
    scanf("%d", &num_ps);
    ps *all_ps = malloc(num_ps * sizeof(ps));
    ps **queue = malloc(num_ps * sizeof(ps*));

    for (int i = 0; i < num_ps; i++)
        scanf("%s %d %d", all_ps[i].name, &all_ps[i].st, &all_ps[i].ft);
    qsort(all_ps, num_ps, sizeof(ps), compare_process); // sort by start time
    for (int i = 0; i < num_ps; i++)
        queue[i] = NULL;
    
    /* set CPU */
    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(0, &cpu_mask);
    
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask) != 0) {
        perror("sched_setaffinity error");
        exit(1);
    }
    
    struct sched_param param;
    const int low_prior = sched_get_priority_min(SCHED_FIFO);
    const int high_prior = sched_get_priority_max(SCHED_FIFO);
    
    /* set priority */
    param.sched_priority = (low_prior + high_prior) / 2;
    pid_t pid_p = getpid();
    if (sched_setscheduler(pid_p, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler() failed");
        exit(1);
    }
    
    int time_unit = 0;
    int fork_count = 0;
    while (num_ps > numps_finish) {
        /* push the process ready for execution */
        while (all_ps[fork_count].st <= time_unit && fork_count < num_ps) {
            push_process(queue, &all_ps[fork_count]);
            // printf("%s : start at time %d\n", all_ps[fork_count].name, time_unit);
            /* run the process */
            long int st, et;
            long st_s, st_ns, et_s, et_ns;
            st = syscall(336);
            st_s = st / t;
            st_ns = st % t;
            pid_t pid = fork();
            if (pid < 0) {
                perror("Fork Failed");
                exit(1);
            }
            /* child process */
            else if (pid == 0) {
                for (int j = 0; j < all_ps[fork_count].ft; j++) {
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
            all_ps[fork_count].pid = pid;
            fork_count++;
        }
        
        /* no process to run */
        if (queue[0] == NULL) {
            time_unit++;
            volatile unsigned long i;
            for(i=0;i<1000000UL;i++);
        } else {
            time_unit++;
            queue[0]->ft = queue[0]->ft - 1;
            volatile unsigned long i;
            for (i = 0; i < 1000000UL; i++);
        }

        /* process in queue[0] should terminate */
        if (queue[0] != NULL && queue[0]->ft == 0) {
            /* wait if the process is not terminated */
            int status;
            if (waitpid(queue[0]->pid, &status, 0) == -1) {
                perror("waitpid error");
                exit(1);
            }
            // printf("%s : end at time %d\n", queue[0]->name, time_unit);
            pop_process(queue);
            numps_finish++;
            /* set higher priority for the next running process*/
            if (queue[0] != NULL) {
        		// printf("set %s to high at time %d\n", queue[0]->name, time_unit);
            	pid_t pid = queue[0]->pid;
            	param.sched_priority = high_prior;
            	if(sched_setscheduler(pid, SCHED_FIFO, &param) != 0) {
                	perror("sched_setscheduler error");
                	exit(EXIT_FAILURE);
                }
            }
        }
    }
    
    for (int i = 0; i < num_ps; i++)
        printf("%s %d\n", all_ps[i].name, all_ps[i].pid);
    
    free(all_ps);
    free(queue);
    return 0;
}
