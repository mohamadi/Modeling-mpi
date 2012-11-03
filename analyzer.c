/*
 	CPSC 521: Parallel Algorithms and Architectures
	Assignment 3: Modeling
	Author: Hamid Mohamadi, mohamadi@alumni.ubc.ca
*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
int main(int argc, const char *argv[]){
	const int fNum = atoi(argv[1]);
	const int iter = atoi(argv[2]);
	FILE *logFile[fNum];
	char logPath[20];
	int i,j;
	for (i = 0; i < fNum; i++){
	    sprintf(logPath, "log%d", i);
	    logFile[i] = fopen(logPath, "r");
	}
	double W=0.0, gH=0.0;
	double w, gh, w_max, gh_max;
	
	for(j=0;j<iter;j++){
		w_max=-1.0; gh_max=-1.0;
		for(i=0; i<fNum; i++){
			fscanf(logFile[i],"%*s %lf %*s %lf", &w, &gh);
			if(w>w_max) w_max=w;
			if(gh>gh_max) gh_max=gh;
		}
		W += w_max;
		gH += gh_max;
	}
	printf("W= %.4f gH= %.4f\n", W, gH);

	for (i = 0; i < fNum; i++)
		fclose(logFile[i]);
	return 0;
}
