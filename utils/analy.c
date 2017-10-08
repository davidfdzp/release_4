#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
       This file make the analysis of trace created
       with ns command for tracing queues:
       $ns trace-queue $node1 $node2 $fileId
 	
       For links that use RED gateways, there are additional trace
       records as follows:
			 <code> <time> <value>
	      where
		     <code> := [Qap] Q=queue size, a=average queue size,
			  p=packet dropping probability
		     <time> := simulation time in seconds
		     <value> := value

	      Trace records for link dynamics are of the form:

			 <code> <time> <state> <src> <dst>
	      where
		     <code> := [v]
		     <time> := simulation time in seconds
		     <state> := [link-up | link-down]
		     <src> := first node address of link
		     <dst> := second node address of link

	by ThePresident
 */


#define DIM 100000

int
main(int argc, char* argv[]) {
struct packet{
	double ts;
	int pktID;
	int busy;
};

FILE* fd;
FILE* fo;
char* outn;
double smp=0.1;		// sampling interval

// fields of th e trace
char code;		// [hd+-r] h=hop d=drop +=enque -=deque r=receive
double time; 		// simulation time in seconds
int hsrc;		// first node address of hop/queuing link
int hdst;		// second node address of hop/queuing link
// <packet> :=  <type> <size> <flags> <flowID> <src.sport> <dst.dport> <seq> <pktID>
	      
char type[20];		// tcp|telnet|cbr|ack etc.
int size;		// packet size in bytes

char flags[10];		// [CP]  C=congestion, P=priority
int flowID; 		// flow identifier field as defined for IPv6
char srcsport[20]; 	// transport address (src=node,sport=agent)
char dstsport[20]; 	// transport address (dst=node,dport=agent)
int seq;		// packet sequence number
int pktID; 		// unique identifer for every new packet
int i, ic;

// output variables
double timestamp=0;
int inputBIT=0;
int inputPKT=0;
int outputBIT=0;
int outputPKT=0;
double queuesizeBIT=0;
double queuesizePKT=0;
struct packet backlog[DIM];
int dropBIT=0;
int dropPKT=0;

int fid;
double delay;

	if (argc<2 || argc>4 ) {
		printf("usage: analy [filename] [samp=0.1] [flowid=<none>]\n\n");
		printf("\t[filename]\tname of input file\n");
		printf("\t[samp]\t\tsampling interval (sec)\n");
		printf("\t[flowid]\tIPv6 identifier of flow to analyze\n\n");
		printf("Output Format:\n");
		printf("[timestamp.1] [inputBIT.2] [inputPKT.3] [outputBIT.4] \\\\\n");
		printf("\t[outputPKT.5] [queuesizeBIT.6] [queuesizePKT.7] [dropBIT.8] [dropPKT.9] [delay.10]\n\n");
		printf("[queuesizePKT]   the number of packets waiting for service at the end of the interval\n");
		printf("\nnotes:\n\tWe assume an initial empty queue.\n");
		exit(1);
	}
	
	fd = fopen(argv[1],"r");
	if (fd == NULL ) {
		printf("analy: bad file name\n");
		exit(1);
	}
	
	outn = (char*)malloc(strlen(argv[1])+5);
	outn = strcat(strcpy(outn,argv[1]),".dat");
	fo = fopen(outn,"w");

	if ( argc>2 ) {	
		smp = atof(argv[2]);
		if (smp <= 0 ) {
			printf("analy: the sampling interval is invalid\n");
			exit(1);
		}
	}
	
	if (argc == 4) {
		fid = atoi(argv[3]);
	} 
	
	for(i=0; i<DIM; i++)
		backlog[i].busy = 0;
	i=0;
		
	while(fscanf(fd,"%c %lf %d %d %s %d %s %d %s %s %d %d\n", 
		&code, &time, &hsrc, &hdst, type, &size, flags, &flowID, srcsport, dstsport, &seq, &pktID)  > 0){

		if (flowID != fid && argc==4 ) 
			continue;
		
		while (timestamp < time-smp ) {
			timestamp += smp;
			if (outputPKT==0) 
				fprintf(fo, "%lf %d %d %d %d %lf %lf %d %d\n",
					timestamp,inputBIT,inputPKT,outputBIT,outputPKT,
					queuesizeBIT,queuesizePKT,dropBIT,dropPKT);
			else 	
				fprintf(fo, "%lf %d %d %d %d %lf %lf %d %d %lf\n",
					timestamp,inputBIT,inputPKT,outputBIT,outputPKT,
					queuesizeBIT,queuesizePKT,dropBIT,dropPKT,delay/outputPKT);
			inputBIT=0;
			inputPKT=0;
			outputBIT=0;
			outputPKT=0;
			dropBIT=0;
			dropPKT=0;
			delay=0;
		}

		if (code == '+') {
			inputBIT += size*8;
			inputPKT++;
			queuesizeBIT += size*8;
			queuesizePKT++;
			while(backlog[i].busy == 1)
				i = (i+1)%DIM;
			backlog[i].busy = 1;
			backlog[i].ts = time;
			backlog[i].pktID = pktID;
		} else if (code == '-') {
			while(backlog[i].busy == 0 || backlog[i].pktID != pktID)
				i = (i+1)%DIM;
			delay += time - backlog[i].ts;
			backlog[i].busy = 0;
			outputBIT += size*8;
			outputPKT++;
			queuesizeBIT -= size*8;
			queuesizePKT--;
		} else if (code == 'd') {

			ic = (i+DIM-1)%DIM;
			while(backlog[i].pktID != pktID && i!=ic)
				i = (i+1)%DIM;

			dropBIT += size*8;
			dropPKT++;
			if (backlog[i].pktID == pktID) {
				backlog[i].busy = 0;
				queuesizeBIT -= size*8;
				queuesizePKT--;
			}
		}
	}
	exit(0);
}

