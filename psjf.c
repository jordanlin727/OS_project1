#define _GNU_SOURCE
#include <limits.h>
#include <time.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct{
    char name[32];
    int readytime;
    int exectime;
    int forked;
    struct timespec start_time;
    struct timespec finish_time;
    pid_t pid;
}Process;

void err_sys(char *errmsg){
    perror(errmsg);
    exit(EXIT_FAILURE);    
}

int main(void){
    /*setting affinity*/
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    /*main code*/
    int n, status, currentprocess, minreadytime = INT_MAX;
    scanf("%d", &n);
    Process process[n];
    pid_t pid = getpid(), ppid = getpid();

    /*input process properties*/
    for(int i = 0;i < n;i++){
        scanf("%s %d %d", process[i].name, &process[i].readytime, &process[i].exectime);
    }

    /*find the time for first process to start*/
    for(int i = 0;i < n;i++){
        if(process[i].readytime < minreadytime){
            minreadytime = process[i].readytime;
            currentprocess = i;
        }
        process[i].forked = 0;
    }
    int curtime;
    /*wait for first process to start*/
    for(curtime = 0;curtime < minreadytime;curtime++){
        volatile unsigned long j; for(j=0;j<1000000UL;j++);
    }
    curtime+= minreadytime;

    waitpid(pid, &status, WUNTRACED);
    int found = 1;
    while(found){
        found = 0;
        int minexectime = INT_MAX;
        for(int i = 0;i < n;i++){
            if((curtime >= process[i].readytime) && (process[i].exectime > 0) && (process[i].exectime < minexectime)){
                currentprocess = i;
                minexectime = process[i].exectime;
                found = 1;
            }
        }
        if(found == 0) break;
        if(!process[currentprocess].forked){
            clock_gettime(CLOCK_REALTIME, &process[currentprocess].start_time);
            if((process[currentprocess].pid = pid = fork()) < 0){
                err_sys("fork");
            }
            if(pid == 0){
                raise(SIGSTOP);
                while(1){
                    volatile unsigned long i; for(i=0;i<1000000UL;i++);
                    raise(SIGSTOP);
                }       
            }
            else{
                if(ppid != getpid()) return 0;
                if(waitpid(process[currentprocess].pid, &status, WUNTRACED) < -1){
                    err_sys("waitpid");
                }
                if(WIFSTOPPED(status)){
                    kill(process[currentprocess].pid, SIGCONT);
                }
                if(waitpid(process[currentprocess].pid, &status, WUNTRACED) < -1){
                    err_sys("waitpid");
                }
                process[currentprocess].forked = 1;
            }
        }
        else{
            kill(process[currentprocess].pid, SIGCONT);
            if(waitpid(process[currentprocess].pid, &status, WUNTRACED) < -1){
                err_sys("waitpid");
            }                
        }
        curtime++;
        process[currentprocess].exectime--;
        if(process[currentprocess].exectime == 0){
            kill(process[currentprocess].pid, SIGINT);
            clock_gettime(CLOCK_REALTIME, &process[currentprocess].finish_time);
        }
    }
    for(int i = 0;i < n;i++){
        printf("%s %d\n", process[i].name, process[i].pid);
    }
    for(int i = 0;i < n;i++){
        printf("[Project1] %d %ld.%ld %ld.%ld\n", process[i].pid, process[i].start_time.tv_sec,
            process[i].start_time.tv_nsec, process[i].finish_time.tv_sec, process[i].finish_time.tv_nsec);
    }
}