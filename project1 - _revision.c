#define _GNU_SOURCE
#include <sched.h>
#include <linux/sched.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags)
{
    return syscall(314, pid, attr, flags);
}

int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags)
{
    return syscall(315, pid, attr, size, flags);
}
int main(int argc, char* argv[])
{
    int ret;
    int status;
    //define process structure
    struct array
    {
        char process_name[20]; // process name from file
        int pid; // record pid number
        int ready_time; // ready time from file
        int run_time; // run time from file
    } arr[20];
    //define schedule attribute
    struct sched_attr
    {
        unsigned int size;
        unsigned int sched_policy;
        unsigned long long sched_flags;
        /* SCHED_NORMAL, SCHED_BATCH */
        unsigned int sched_nice;
        /* SCHED_FIFO, SCHED_RR */
        unsigned int sched_priority;
        /* SCHED_DEADLINE (nsec) */
        unsigned long long sched_runtime;
        unsigned long long sched_deadline;
        unsigned long long sched_period;
    };
    FILE *fp=NULL;
    int process_count=0;
    char buff[500];
    char model[20];
    int count=1,node=0;


    //read file and dispatch to structure
    if((fp = fopen(argv[1], "r")) == NULL)
    {
        printf("open file error!\n");
        exit(EXIT_FAILURE);

    }

    while(ret = fscanf(fp," %s ", buff))   // use of fscanf
    {
        if(ret == EOF)
        {
            break;
        }

        if(count == 1)
            snprintf(model, 20, "%s", buff);
        else if (count ==2)
            process_count = atoi(buff);
        else
        {
            if(count%3 ==0)
            {
                snprintf(arr[node].process_name,20,"%s",buff);
            }
            else if(count%3 ==1)
            {
                arr[node].ready_time = atoi(buff);
            }
            else
            {
                arr[node].run_time = atoi(buff);
                node +=1;
            }
        }
        count+=1;
    }
    //CPU_ZERO(&set);

    struct sched_param param;
    int maxpri;
    maxpri = sched_get_priority_max(SCHED_RR);
    param.sched_priority = maxpri;

    if(sched_setscheduler(getpid(), SCHED_RR, &param) == -1)
    {
        perror("sched_setscheduler() failed");
        exit(1);
    }

    //accroding count to create fork
    volatile unsigned long x=0;
    /*for(int i=0;i<process_count;i++)
    {
        for(x=0;x<1000000UL;x++)
    */
    for(x=0; x<1000000UL; x++)

    {

        for(int i=0; i<process_count; i++)
        {
            if(arr[i].ready_time==x)
            {
                status = fork();

                if(status == 0)
                {
                    unsigned int start_sec,end_sec;
                    long start_nsec, end_nsec;
                    syscall(327,&start_sec, &start_nsec);
                    arr[i].pid=getpid();
                    //printf("start %ld.%ld\n",start_sec,start_nsec);
                    int j=1;
                    int pid_index;
                    for(int l=0; l<process_count; l++)
                    {

                        if(arr[l].pid==getpid())
                        {
                            pid_index = l;
                            //printf("pid_index=%d,l=%d\n",pid_index,l);

                        }
                    }
                    //printf("start pid_index=%d\n",pid_index);

                    while(j<1000000)
                    {

                        if(j==arr[pid_index].run_time)
                        {
                            //printf("j=%d,pid=%d\n",j,arr[pid_index].pid);
                            syscall(327,&end_sec, &end_nsec);
                            //printf("end_sec %ld.%ld\n",end_sec,end_nsec);
                            syscall(326,argv[0],arr[pid_index].pid,start_sec,start_nsec,end_sec,end_nsec);
                            //printf("exit\n");
                            exit(0);
                        }
                        j+=1;
                    }
                    exit(0);
                }
                else
                {
                    printf("%s %d\n",arr[i].process_name, status); // show the process and pid on the screen
                }
            }
        }
    }
    for(int i=0; i<5; i++) // loop will run n times (n=5)
        wait(NULL);

}
