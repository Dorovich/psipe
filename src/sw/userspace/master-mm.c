#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <errno.h>
#include "psipe_wrappers.h"
#include <signal.h>

static void sighup_handler(int signo)
{
	return;
}

#define TYPE double

struct psipe_devices *psipe_devs;

#define TRUNCATE 1
#define APPEND 2

#define _SIZE_X 50
#define SIZE_N _SIZE_X
#define SIZE_T _SIZE_X
#define SIZE_M _SIZE_X
/*
#define SIZE_N 80 //64
#define SIZE_T 90 //100
#define SIZE_M 80 //64
*/

double now();
void show_time(char *msg, double t0, double t1);
void *matrix_allocate(char *name, int rows, int cols);
void save_matrix_on_file(char *msg, int rows, int cols, TYPE (*C)[cols], int fop);
void save_matrix_info(int sz_n, int sz_t, int sz_m);

static void matmul_gold(int sz_n, int sz_t, int sz_m,
		TYPE (* __restrict__ C)[sz_m],
		TYPE (* __restrict__ A)[sz_t],
		TYPE (* __restrict__ B)[sz_m])
{
	for (int j=0; j < sz_m; j++) {
		for (int i=0; i < sz_n; i++) {
			for (int k=0; k < sz_t; k++) {
				C[i][j] += A[i][k] * B[k][j];
			}
		}
	}
}

void matmul(char *msg, int sz_n, int sz_t, int sz_m, 
		TYPE (* __restrict__ C)[sz_m], TYPE (* __restrict__ A)[sz_t],
		TYPE (* __restrict__ B)[sz_m])
{
	double t0, t1;

	{ /* psipe PART START =================================== */
		psipe_open_devs();
	} /* psipe PART END ===================================== */

	printf ("num_devs %d\n", psipe_num_devs());
	t0 = now();

	{ /* psipe PART START =================================== */
		int fd, num = psipe_num_devs();
		int part, pi, pj, sz_part, ofs = 0;
		int last[num];
		psipe_handle_t id;

		for (int i = 0; i < num; ++i) {
			fd = psipe_fd(i);
			part = PART_FOR_DEV(i, sz_n * sz_m, num);
			sz_part = part * sizeof(TYPE);
			pi = ofs / sz_m;
			pj = ofs % sz_m;

			printf("dev=%d (fd=%d), part=%d (sz_part=%d), ofs=%d\n",
					i, fd, part, part, ofs);

			id = psipe_send_args(fd, sz_n, sz_t, sz_m, part, ofs);
			if ((long)id < 0) {
				perror("psipe_send_args");
				exit(1);
			}
#if WAIT_ALL_OPS
			if (psipe_wait(fd, id) < 0) {
				perror("psipe_wait(args)");
				exit(1);
			}
#endif

			id = psipe_send(fd, A, sz_n * sz_t * sizeof(TYPE));
			if ((long)id < 0) {
				perror("psipe_send(A)");
				exit(1);
			}
#if WAIT_ALL_OPS
			if (psipe_wait(fd, id) < 0) {
				perror("psipe_wait(A)");
				exit(1);
			}
#endif

			id = psipe_send(fd, B, sz_t * sz_m * sizeof(TYPE));
			if ((long)id < 0) {
				perror("psipe_send(B)");
				exit(1);
			}
#if WAIT_ALL_OPS
			if (psipe_wait(fd, id) < 0) {
				perror("psipe_wait(B)");
				exit(1);
			}
#endif

			id = psipe_send(fd, &C[pi][pj], sz_part);
			if ((long)id < 0) {
				perror("psipe_send(C)");
				exit(1);
			}
#if WAIT_ALL_OPS
			if (psipe_wait(fd, id) < 0) {
				perror("psipe_wait(C)");
				exit(1);
			}
#endif

			last[i] = psipe_recv(fd, &C[pi][pj], sz_part);
			if ((long)last[i] < 0) {
				perror("psipe_recv(C)");
				exit(1);
			}
			/*
			id = psipe_recv(fd, &C[pi][pj], sz_part);
			if ((long)id < 0) {
				perror("psipe_recv(C)");
				exit(1);
			}
			if (psipe_wait(fd, id) < 0) {
				perror("psipe_wait(C)");
				exit(1);
			}

			psipe_flush(fd);
			*/
			ofs += part;
			/*
			printf("matmul - part %d/%d ready\n", i+1, num);
			fflush(NULL);
			*/
		}
		for (int i = 0; i < num; ++i) {
			id = last[i];
			fd = psipe_fd(i);
			if (psipe_wait(fd, id) < 0) {
				perror("psipe_wait(C)");
				exit(1);
			}
			psipe_flush(fd);
			printf("matmul - part %d/%d ready\n", i+1, num);
		}
	} /* psipe PART END ===================================== */

	t1 = now();

	{ /* psipe PART START =================================== */
		psipe_close_devs();
	} /* psipe PART END ===================================== */

	show_time(msg, t0, t1);
	printf("\nMatrices: ( %d %d ) x ( %d %d ) -> ( %d %d )\n",
			sz_n, sz_t, sz_t, sz_m, sz_n, sz_m);
	double timetaken = t1 - t0;
	printf("Multiplication time: %lf s\n", timetaken);
	double operations = (double) sz_n*sz_t*sz_m*2.0;
	printf("Operations:\t%lf\n", operations);
	printf("MFlops:\t%lf\n", operations/(timetaken*1000000.0));
}

void matrix_init(char* msg, int rows, int cols, TYPE (* mat)[cols],
		TYPE random)
{
	//int loop, i, j;
	int i, j;
	//double t0, t1;

	// Repeat initialization 4 times to check if there is
	// any timing variability
	//for (loop = 0; loop < 4; loop++) {
		//t0 = now();

		for (i=0; i < rows; i++) {
			for (j=0; j < cols; j++) {
				TYPE value = random *
					((TYPE)(1.0/((TYPE)i+0.25)) - 0.005*j);
				mat[i][j] = value;
			}
		}

		//t1 = now();

		//show_time(msg, t0, t1);
	//}
	//printf("\n");
}

TYPE A[SIZE_N][SIZE_T] __attribute__((aligned(sizeof(TYPE))));
TYPE B[SIZE_T][SIZE_M] __attribute__((aligned(sizeof(TYPE))));
TYPE *C, *C_golden; 

int main(int argc, char *argv[])
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighup_handler;
	if (sigaction(SIGHUP, &sa, NULL) < 0) {
		perror("sigaction(sighup)");
		exit(EXIT_FAILURE);
	}

	int sz_n = SIZE_N, sz_t = SIZE_T, sz_m = SIZE_M;
	int save_matrices = 0;
	int i;
	for (i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-s")) {
			save_matrices = 1;
		} else if (!strcmp(argv[i], "-h")) {
			printf ("Us: %s [-s] [-h] [N [T [M]]]\n", argv[0]);
			exit(1);
		} else {
			break;
		}
	}

	if (i < argc)
		sz_n = strtol(argv[i++], NULL, 0);
	if (i < argc)
		sz_t = strtol(argv[i++], NULL, 0);
	if (i < argc)
		sz_m = strtol(argv[i++], NULL, 0);
	if (i < argc)
		fprintf(stderr, "Arguments ignored: %s...\n", argv[i]);

	if (sz_n <= 0 || sz_n > SIZE_N) {
		fprintf(stderr, "Invalid matrix dimension (N, %d)\n", sz_n);
		exit(1);
	}
	if (sz_t <= 0 || sz_t > SIZE_T) {
		fprintf(stderr, "Invalid matrix dimension (T, %d)\n", sz_t);
		exit(1);
	}
	if (sz_m <= 0 || sz_m > SIZE_M) {
		fprintf(stderr, "Invalid matrix dimension (M, %d)\n", sz_m);
		exit(1);
	}

	//printf("Matrix sizes %d %d %d\n", sz_n, sz_t, sz_m);

	C = matrix_allocate("C", sz_n, sz_m);
	C_golden = matrix_allocate("C_golden", sz_n, sz_m);

	matrix_init("init(A)", sz_n, sz_t, A, 0.04);
	matrix_init("init(B)", sz_t, sz_m, B, 0.07);
	matrix_init("init(C)", sz_n, sz_m, (TYPE (*)[sz_m])C, 0.0);
	memcpy(C_golden, C, sz_n * sz_m * sizeof(TYPE));

	//printf("Iniciant matmul, pid = %d\n", getpid());

	matmul("C += AxB", sz_n, sz_t, sz_m, (TYPE (*)[sz_m])C, A, B);
	matmul_gold(sz_n, sz_t, sz_m, (TYPE (*)[sz_m])C_golden, A, B);

	/*
	for (int i=0; i<sz_n; ++i) {
		double cnt = 0;
		for (int j=0; j<sz_m; ++j)
			cnt += C[i*sz_n+j];
		printf("[%d] %lf\t", i, cnt);
		if (i%5 == 4)
			printf("\n");
	}
	*/

	int errors = 0;
	for (int i = 0; i < sz_n * sz_m; ++i) {
		if (C[i] != C_golden[i])
			++errors;
	}
	printf("Multiplication errors: %d\n", errors);

	if (save_matrices) {
		printf ("Saving matrices... 1 "); fflush(NULL);
		save_matrix_on_file("A", sz_n, sz_t, A, TRUNCATE);
		printf (" ... 2 "); fflush(NULL);
		save_matrix_on_file("B", sz_t, sz_m, B, APPEND);
		printf (" ... 3 "); fflush(NULL);
		save_matrix_on_file("C", sz_n, sz_m, (TYPE (*)[sz_m]) C,
				APPEND);
		printf ("   Saved!\n");
		save_matrix_info (sz_n, sz_t, sz_m);
	}

	return 0;
}

void *matrix_allocate(char *name, int rows, int cols)
{
	//TYPE *m = malloc(rows*cols*sizeof(TYPE));
	TYPE *m = NULL;
	int size = rows * cols * sizeof(TYPE);
	int err = posix_memalign((void *)&m, sizeof(TYPE), size);
	//if (m == NULL) {
	if (err != 0) {
		//fprintf(stderr, "malloc(%s): %s\n", name, strerror(errno));
		fprintf(stderr, "posix_memalign(%s): %s\n", name, strerror(err));
		exit(1);
	}
	return m;
}


double now()
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0)
		perror("gettimeofday");
	return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

void show_time(char *msg, double t0, double t1)
{
	fprintf(stderr, "%s: %lf s\n", msg, t1 - t0);
}

void save_matrix_on_file(char * msg, int rows, int cols, TYPE (*C)[cols],
		int fop)
{
	int i, j;
	FILE *saved_matrix = fopen("matrices.dat", (fop == 1)? "w" : "a");

	if (saved_matrix == NULL) {
		fprintf(stderr, "fopen(matrices.dat): %s\n", strerror(errno));
		return;
	}

	fprintf (saved_matrix, "%s [", msg);

	for (i = 0; i < rows; i++) {
		fprintf (saved_matrix, "[");
		for (j = 0; j < cols-1; j++)
			fprintf (saved_matrix, "%.17lf, ", C[i][j]);
		if (i < (rows-1))
			fprintf (saved_matrix, "%.17lf],\n", C[i][j]);
		else
			fprintf (saved_matrix, "%.17lf]", C[i][j]);
	}

	fprintf (saved_matrix, "]\n");

	fclose(saved_matrix);
}

void save_matrix_info(int sz_n, int sz_t, int sz_m)
{
	FILE *info = fopen("matrix-info.dat", "w");
	if (info == NULL) {
		fprintf(stderr, "fopen(matrix-info.dat): %s\n",
				strerror(errno));
		return;
	}

	fprintf(info, "#    Rows    Columns/Rows    Columns    Iterations\n");
	fprintf(info, "      %d       %d            %d          1\n",
			sz_n, sz_t, sz_m);
	fclose(info);
}
