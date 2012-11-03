#define BENCHMARK "OSU MPI Latency Test Modified by Hamid"
/*
 * Copyright (C) 2002-2012 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <mpi.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_MSG_SIZE 10//(1<<22)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define SKIP_LARGE  10
#define LOOP_LARGE  100
#define LARGE_MESSAGE_SIZE  8192
#define MESSAGE_ALIGNMENT 64
#define MY_DBL_NUM 2345456.564564

typedef struct{
	double x,y;
	double m;
} body;


int skip = 10000;
int loop = 100000;
#ifdef PACKAGE_VERSION
#	define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#	define HEADER "# " BENCHMARK "\n"
#endif

#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 15
#endif

int main(int argc, char *argv[])
{
    const int gran=atoi(argv[1]); /* granularity */

	int myid, numprocs, i;
    int size;
    MPI_Status reqstat;
    body *s_buf, *r_buf;
    int align_size;
    double t_start = 0.0, t_end = 0.0;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Datatype bodytype;
	MPI_Type_contiguous(3, MPI_DOUBLE, &bodytype);
	MPI_Type_commit(&bodytype);

    if(numprocs != 2) {
        if(myid == 0)
            fprintf(stderr, "This test requires exactly two processes\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    //align_size = MESSAGE_ALIGNMENT;
/**************Allocating Memory*********************/
    s_buf = (body *) malloc(gran*sizeof(body));
	r_buf = (body *) malloc(gran*sizeof(body));
	/*s_buf =
		(char *) (((unsigned long) s_buf_original + (align_size - 1)) /
		  align_size * align_size);

	r_buf =
		(char *) (((unsigned long) r_buf_original + (align_size - 1)) /
		  align_size * align_size);*/
/**************Memory Allocation Done*********************/

    if(myid == 0) {
        fprintf(stdout, HEADER);

        fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
        fflush(stdout);
    }

    //for(size = 0; size <= MAX_MSG_SIZE; size = (size ? size * 2 : 1)) {
        /* touch the data */
    size = gran*3;
		for(i = 0; i < gran; i++) {
                s_buf[i].x = MY_DBL_NUM;
                s_buf[i].y = MY_DBL_NUM;
                s_buf[i].m = MY_DBL_NUM;
                r_buf[i].x = MY_DBL_NUM;
                r_buf[i].y = MY_DBL_NUM;
                r_buf[i].m = MY_DBL_NUM;
		}

        if(size > LARGE_MESSAGE_SIZE) {
            loop = LOOP_LARGE;
            skip = SKIP_LARGE;
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if(myid == 0) {
            for(i = 0; i < loop + skip; i++) {
                if(i == skip) t_start = MPI_Wtime();
                MPI_Send(s_buf, gran, bodytype, 1, 1, MPI_COMM_WORLD);
                MPI_Recv(r_buf, gran, bodytype, 1, 1, MPI_COMM_WORLD, &reqstat);
            }
            t_end = MPI_Wtime();
        }

        else if(myid == 1) {
            for(i = 0; i < loop + skip; i++) {
                MPI_Recv(r_buf, gran, bodytype, 0, 1, MPI_COMM_WORLD, &reqstat);
				MPI_Send(s_buf, gran, bodytype, 0, 1, MPI_COMM_WORLD);
            }
        }

        if(myid == 0) {
            double latency = (t_end - t_start) * 1e6 / (2.0 * loop);
            fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH,
                    FLOAT_PRECISION, latency);
            fflush(stdout);
        }
    //}
    free(s_buf);
    free(r_buf);
    MPI_Finalize();
    return EXIT_SUCCESS;
}

/* vi: set sw=4 sts=4 tw=80: */
