#define _GNU_SOURCE

#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include"scheduler.h"

void SJF_Process_startwork_time(PROCESS* P,int num_p){
	int CPU_ready_time=0;
	CPU_ready_time+=(&P[0])->R; //align earliest ready time
	int finish_count=0;
	int finish[num_p];
	for(int i=0;i<num_p;i++){
		finish[i]=0;
	}
	while(finish_count!=num_p){
		unsigned int cur_shortest_job=0;
		unsigned int earliest_finish_time=-1; //maximum number of unsigned integer
		for(int i=0;i<num_p;i++){
			if(finish[i]==0){
				int finish_time;
				if(CPU_ready_time>=(&P[i])->R){
					finish_time=CPU_ready_time+(&P[i])->T;
				}
				else{
					continue;
				}
				if(finish_time<earliest_finish_time){
					cur_shortest_job=i;
					earliest_finish_time=finish_time;
				}
			}
		}
		(&P[cur_shortest_job])->startwork_time=CPU_ready_time;
		CPU_ready_time=earliest_finish_time;
		finish[cur_shortest_job]=1;
		finish_count++;
	}
}
int ready_time_sort(const void* a,const void* b){
	PROCESS* P1=(PROCESS*) a;
	PROCESS* P2=(PROCESS*) b;
	if(P1->R<P2->R){
		return -1;
	}
	if(P1->R>P2->R){
		return 1;
	}
	return 0;
}
void SJF(PROCESS* P,int num_p){
	int CPU_time=0;
	SJF_Process_startwork_time(P,num_p); //compute process' start time
	qsort(P,num_p,sizeof(PROCESS),ready_time_sort); //sort ready time with increasing order

	for(int i=0;i<num_p;i++){

		//wait until next process is ready to fork
		if(CPU_time<(&P[i])->R){
			for(int j=0;j<(&P[i])->R-CPU_time;j++){
				for(volatile unsigned long k=0;k<1000000UL;k++);
			}
			CPU_time=(&P[i])->R;
		}

		//fork
		if(((&P[i])->pid=fork())==0){

			//sleep
			for(int j=0;j<(&P[i])->startwork_time-(&P[i])->R;j++){
				for(volatile unsigned long k=0;k<1000000UL;k++);
			}

			//printk start time
			struct timespec ts;
			getnstimeofday(&ts);
			printk("%lld\n",ts.tv_nsec);
			
			//set CPU affinity
			cpu_set_t mask;
			CPU_ZERO(&mask);
			CPU_SET(0,&mask);
			sched_setaffinity(getpid(),sizeof(mask),&mask);
			
			//executing
			for(int j=0;j<(&P[i])->T;j++){
				for(volatile unsigned long k=0;k<1000000UL;k++);
			}

			//printk finish time
			getnstimeofday(ts);
			printf("%lld\n",ts.tv_nsec);

			exit(0);
		}
		
	}
	
	//wait for all child process
	for(int i=0;i<num_p;i++){
		wait(NULL);
	}



	return;
}