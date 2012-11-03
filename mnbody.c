/*
 	CPSC 521: Parallel Algorithms and Architectures
	Assignment 3: Modeling
	Author: Hamid Mohamadi, mohamadi@alumni.ubc.ca
*/

#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <string.h>

typedef struct{
	double x,y;
	double m;
} body;

typedef struct{
	double x, y;
} fvp; // data type for force and velocity

void frcInit(const int , body *, fvp *); /* computing internal forces */
void frcUpdt(const int , body *, body *, fvp *); /* updating the forces */
void posUpdt(const int , body *, fvp *, fvp *); /* updating positions */
void scatter(const char *, int, int, const int, body *);
void simulate(const int, int, int, const int, body *);
void gather(int, int, const int, body *);

FILE *logFile;
double t_comp, t_comm, t_comm_total=0.0, t_comp_total=0.0;
double w_scatter=0.0, gh_scatter=0.0;
double w_simi=0.0, gh_simi=0.0;
double w_gather=0.0, gh_gather=0.0;

int main(int argc, char *argv[]){    
	const int tmax=atoi(argv[1]); /* number of simulation rounds */
	const int gran=atoi(argv[2]); /* granularity */
	const char *pName=argv[3]; /* bodies file name */
	double time, time1, time2, time3; /* total, scatter, simulate, and gather running times */ 
	int rank, size; /* rank of process, number of processes */
	
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	
	body *bodies = (body *) malloc(gran*sizeof(body));

	char logPath[20];
	sprintf(logPath, "log%d", rank);
	logFile = fopen(logPath, "w");

	time = MPI_Wtime();
	/****************************************************************
	1. Scatter:
	Reading the input from file and assigning each line to a process
	****************************************************************/
	time1 = MPI_Wtime();
	scatter(pName, rank, size, gran, bodies);
	time1 = MPI_Wtime() - time1;
	//if(rank==0) printf("Scatter time is %.4f seconds\n", time1);	
	/****************************************************************
	2. Simulate:
	Passing the data of each process to other processes and simulate
	****************************************************************/
	time2 = MPI_Wtime();
	simulate(tmax, rank, size, gran, bodies);
	time2 = MPI_Wtime() - time2;
	//if(rank==0) printf("Simulate time is %.4f seconds\n", time2);	
	/****************************************************************
	3. Gather:
	Writing the the last positions to "peval_out.txt" 
	****************************************************************/
	time3 = MPI_Wtime();
	gather(rank, size, gran, bodies);
	time3 = MPI_Wtime() - time3;
	//if(rank==0) printf("Gather time is %.4f seconds\n", time3);	
	
	/* Finalizing */
	free(bodies);
	time = MPI_Wtime() - time;
	//if(rank==0) printf("Computation time is %.4f seconds\n", time);	
	//fprintf(logFile, "W_TOTL: %.15f gH_TTL: %.15f \n", t_comp_total, t_comm_total);
	fclose(logFile);
	MPI_Finalize();
	return(0);
}

void frcInit(const int gran, body *bodies, fvp *f){
	int i,j;
	double r; 
	const double  G =  0.0000000000667384;
	for(i=0; i< gran; i++)
		f[i].x=f[i].y=0;
	for(i=0; i<gran-1; i++){
		r=0; //f[i].x=0; f[i].y=0;
		for(j=i+1; j < gran; j++)
			if(j!=i){
				r = sqrt((bodies[j].x-bodies[i].x)*(bodies[j].x-bodies[i].x)+
					     (bodies[j].y-bodies[i].y)*(bodies[j].y-bodies[i].y));
				f[i].x += G*bodies[i].m*bodies[j].m*(bodies[j].x-bodies[i].x)/(r*r*r);
				f[i].y += G*bodies[i].m*bodies[j].m*(bodies[j].y-bodies[i].y)/(r*r*r);
				f[j].x += -f[i].x;
				f[j].y += -f[i].y;
			}
	}
}

void frcUpdt(const int gran, body *bodies, body *rcvBodies, fvp *f){
	int i,j;
	double r;
	const double  G =  0.0000000000667384;
	for(i=0; i<gran; i++){
		r=0; 
		for(j=0; j < gran; j++){
			r = sqrt((rcvBodies[j].x-bodies[i].x)*(rcvBodies[j].x-bodies[i].x)+
				     (rcvBodies[j].y-bodies[i].y)*(rcvBodies[j].y-bodies[i].y));
			f[i].x += G*bodies[i].m*rcvBodies[j].m*(rcvBodies[j].x-bodies[i].x)/(r*r*r);
			f[i].y += G*bodies[i].m*rcvBodies[j].m*(rcvBodies[j].y-bodies[i].y)/(r*r*r);
		}
	}
}

void posUpdt(const int gran, body *bodies, fvp *f, fvp *v){
	int i;
	const double dt= 1.0;
	for(i=0; i<gran; i++){
		v[i].x += f[i].x * dt / bodies[i].m;
		v[i].y += f[i].y * dt / bodies[i].m;
		bodies[i].x += v[i].x * dt;
		bodies[i].y += v[i].y * dt;
	}
}

void scatter(const char *pName, int rank, int size, const int gran, body *bodies){
	t_comp = MPI_Wtime();
	int i, j;
	int sendto = (rank + 1) % size;
	int recvfrom = ((rank + size) - 1) % size;
	
	MPI_Datatype bodytype;
	MPI_Type_contiguous(3, MPI_DOUBLE, &bodytype);
	MPI_Type_commit(&bodytype);
	MPI_Status status;
	
	body *outbuf = (body *) malloc(gran*sizeof(body));

	t_comp = MPI_Wtime() - t_comp;
	t_comp_total+=t_comp;
	w_scatter += t_comp;
	//fprintf(logFile, "Comp0: %.15f\n", t_comp);

	if(rank==0){
		t_comp = MPI_Wtime();
		FILE *pFile;
		pFile = fopen(pName, "rb");
		for(j=0; j<gran; j++)
			fscanf(pFile,"%lf %lf %lf", &bodies[j].x, &bodies[j].y, &bodies[j].m);					
		t_comp = MPI_Wtime() - t_comp;
		t_comp_total+=t_comp;
		w_scatter += t_comp;
		//fprintf(logFile, "Comp0: %.15f\n", t_comp);

		for(i=0; i<size-rank-1; i++){
			t_comp = MPI_Wtime();
			for(j=0; j<gran; j++)
				fscanf(pFile,"%lf %lf %lf", &outbuf[j].x, &outbuf[j].y, &outbuf[j].m);
			t_comp = MPI_Wtime() - t_comp;
			t_comp_total+=t_comp;
			w_scatter += t_comp;
			//fprintf(logFile, "Comp0: %.15f\n", t_comp);

			t_comm = MPI_Wtime();
			MPI_Send(outbuf, gran, bodytype, sendto, 0, MPI_COMM_WORLD);
			t_comm = MPI_Wtime() - t_comm;
			t_comm_total+=t_comm;
			gh_scatter+= t_comm;
			//fprintf(logFile, "Comm: %.15f\n", t_comm);
		}	

		fclose(pFile);
	}	
	else{
		t_comm = MPI_Wtime();
		MPI_Recv(bodies, gran, bodytype, recvfrom, 0, MPI_COMM_WORLD, &status);
		for(i=0; i<size-rank-1; i++){
			MPI_Recv(outbuf, gran, bodytype, recvfrom, 0, MPI_COMM_WORLD, &status);
			MPI_Send(outbuf, gran, bodytype, sendto, 0, MPI_COMM_WORLD);
		}	
		t_comm = MPI_Wtime() - t_comm;
		t_comm_total+=t_comm;
		gh_scatter+= t_comm;
		//fprintf(logFile, "Comm: %.15f\n", t_comm);
	}
	free(outbuf);
	fprintf(logFile, "w_scat: %.15f gh_scat: %.15f\n", w_scatter, gh_scatter);
}

void simulate(const int tmax, int rank, int size, const int gran, body *bodies){
	double w_simi0 = 0.0;
	t_comp = MPI_Wtime();
	int t=tmax, i, round;
	int sendto = (rank + 1) % size;
	int recvfrom = ((rank + size) - 1) % size;

	MPI_Datatype bodytype;
	MPI_Type_contiguous(3, MPI_DOUBLE, &bodytype);
	MPI_Type_commit(&bodytype);
	MPI_Status status;
	

	body *inbuf = (body *) malloc(gran*sizeof(body));
	body *outbuf = (body *) malloc(gran*sizeof(body));
	fvp *f = (fvp *) malloc(gran*sizeof(fvp));
	fvp *v = (fvp *) malloc(gran*sizeof(fvp));
	for(i=0; i < gran; i++){
		v[i].x=0;
		v[i].y=0;
	}
	t_comp = MPI_Wtime() - t_comp;
	t_comp_total+=t_comp;
	w_simi0 += t_comp;
	//fprintf(logFile, "Comp0: %.15f\n", t_comp);
	
	while(1){
		w_simi = 0.0;
		gh_simi = 0.0;
		t_comp = MPI_Wtime();
		--t;
		round=size;
		memcpy(outbuf, bodies, gran*sizeof(body));		
		frcInit(gran, bodies, f);
		t_comp = MPI_Wtime() - t_comp;
		t_comp_total+=t_comp;
		w_simi += t_comp;
		//fprintf(logFile, "Comp1: %.15f\n", t_comp);
		while (round > 1) {
			if (!(rank % 2)){
				t_comm = MPI_Wtime();
				MPI_Send(outbuf, gran, bodytype, sendto, 0, MPI_COMM_WORLD);
				MPI_Recv(inbuf, gran, bodytype, recvfrom, 0, MPI_COMM_WORLD, &status);
				t_comm = MPI_Wtime() - t_comm;
				t_comm_total+=t_comm;
				gh_simi+=t_comm;
				//fprintf(logFile, "Comm: %.15f\n", t_comm);
			}
			else
			{
				t_comm = MPI_Wtime();
				MPI_Recv(inbuf, gran, bodytype, recvfrom, 0, MPI_COMM_WORLD, &status);
				MPI_Send(outbuf, gran, bodytype, sendto, 0, MPI_COMM_WORLD);
				t_comm = MPI_Wtime() - t_comm;
				t_comm_total+=t_comm;
				gh_simi+=t_comm;
				//fprintf(logFile, "Comm: %.15f\n", t_comm);
			}
			t_comp = MPI_Wtime();
			memcpy(outbuf, inbuf, gran*sizeof(body));
			frcUpdt(gran, bodies, inbuf, f);
			--round;
			t_comp = MPI_Wtime() - t_comp;
			t_comp_total+=t_comp;
			w_simi += t_comp;
			//fprintf(logFile, "Comp2: %.15f\n", t_comp);
		}
		t_comp = MPI_Wtime();
		posUpdt(gran, bodies, f, v);
		t_comp = MPI_Wtime() - t_comp;
		t_comp_total+=t_comp;
		w_simi += t_comp;
		//fprintf(logFile, "Comp3: %.15f\n", t_comp);
		if(t==tmax-1)
			fprintf(logFile, "w_simi: %.15f gh_simi: %.15f\n", w_simi+w_simi0, gh_simi);
		else
			fprintf(logFile, "w_simi: %.15f gh_simi: %.15f\n", w_simi, gh_simi);
		if(t==0) break;
	}
	
	free(inbuf);
	free(outbuf);
	free(f);
	free(v);
}

void gather(int rank, int size, const int gran, body *bodies){
	t_comp = MPI_Wtime();
	int i, j;
	int sendto = (rank + 1) % size;
	int recvfrom = ((rank + size) - 1) % size;
	
	MPI_Datatype bodytype;
	MPI_Type_contiguous(3, MPI_DOUBLE, &bodytype);
	MPI_Type_commit(&bodytype);
	MPI_Status status;
	
	body *outbuf = (body *) malloc(gran*sizeof(body));
	t_comp = MPI_Wtime() - t_comp;
	t_comp_total+=t_comp;
	w_gather += t_comp;
	//fprintf(logFile, "Comp0: %.15f\n", t_comp);

	if (rank != 0) {
		t_comm = MPI_Wtime();
		MPI_Send(bodies, gran, bodytype, recvfrom, 0, MPI_COMM_WORLD);
		for(i=0; i<size-rank-1; i++){
			MPI_Recv(outbuf, gran, bodytype, sendto, 0, MPI_COMM_WORLD, &status);
			MPI_Send(outbuf, gran, bodytype, recvfrom, 0, MPI_COMM_WORLD);
		}
		t_comm = MPI_Wtime() - t_comm;
		t_comm_total+=t_comm;
		gh_gather += t_comm;
		//fprintf(logFile, "Comm: %.15f\n", t_comm);
	}
	else {
		t_comp = MPI_Wtime();
		FILE *oFile;
		oFile = fopen("peval_out.txt", "w");
		for(j=0; j<gran; j++)
			fprintf(oFile, "%15.10f %15.10f %15.10f\n", bodies[j].x, bodies[j].y, bodies[j].m);
		t_comp = MPI_Wtime() - t_comp;
		t_comp_total+=t_comp;
		w_gather += t_comp;
		//fprintf(logFile, "Comp0: %.15f\n", t_comp);

		for(i=0; i<size-rank-1; i++){
			t_comm = MPI_Wtime();
			MPI_Recv(outbuf, gran, bodytype, sendto, 0, MPI_COMM_WORLD, &status);
			t_comm = MPI_Wtime() - t_comm;
			t_comm_total+=t_comm;
			gh_gather += t_comm;
			//fprintf(logFile, "Comm: %.15f\n", t_comm);

			t_comp = MPI_Wtime();
			for(j=0; j<gran; j++)
				fprintf(oFile, "%15.10f %15.10f %15.10f\n", outbuf[j].x, outbuf[j].y, outbuf[j].m);
			t_comp = MPI_Wtime() - t_comp;
			t_comp_total+=t_comp;
			w_gather += t_comp;
			//fprintf(logFile, "Comp0: %.15f\n", t_comp);
		}	
		fclose(oFile);
	}
	free(outbuf);
	fprintf(logFile, "w_gath: %.15f gh_gath: %.15f\n", w_gather, gh_gather);
}
