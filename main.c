#include<stdio.h>
#include<string.h>
#include"scheduler.h"

int main(int argc,char*argv[])
{
	//input
	char Schedule[10];
	scanf("%s",Schedule);
	int n;
	scanf("%d",&n);
	PROCESS P[n];
	for(int i=0;i<n;i++){
		scanf("%s%u%u",P[i].name,&(P[i].R),&(P[i].T));
	}
	
	switch(Schedule[0]){
		case 'F':
			if(strcmp("FIFO",Schedule)==0){
				FIFO(P,n);
			}
			break;
		case 'R':
			if(strcmp("RR",Schedule)==0){
				RR(P,n);
			}
			break;
		case 'S':
			if(strcmp("SJF",Schedule)==0){
				SJF(P,n);
			}
			break;
		case 'P':
			if(strcmp("PSJF",Schedule)==0){
				PSJF(P,n);
			}
			break;
	}

	//output
	for(int i=0;i<n;i++){
		printf("%s %d\n",P[i].name,P[i].pid);
	}
	
	
	
	return 0;
}