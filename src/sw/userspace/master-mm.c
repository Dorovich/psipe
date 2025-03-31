#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <errno.h>
#include "pnvl_wrappers.h"

#define TYPE double

struct _pnvl_devices *_pnvl_devs;

#define TRUNCATE 1
#define APPEND 2
#define SIZE_N 64
#define SIZE_T 100
#define SIZE_M 64

double now();
void show_time(char *msg, double t0, double t1);
void *matrix_allocate(char *name, int rows, int cols);
void save_matrix_on_file(char *msg, int rows, int cols, TYPE (*C)[cols], int fop);
void save_matrix_info(int sz_n, int sz_t, int sz_m);

void matmul(char *msg, int sz_n, int sz_t, int sz_m, 
		TYPE (* __restrict__ C)[sz_m], TYPE (* __restrict__ A)[sz_t],
		TYPE (* __restrict__ B)[sz_m])
{
	double t0, t1;

	{ /* PNVL PART START =================================== */
		_pnvl_open_devs();
	} /* PNVL PART END ===================================== */

	printf ("num_devs %d\n", _pnvl_num_devs());
	t0 = now();

	{ /* PNVL PART START =================================== */
		int rc, fd, num = _pnvl_num_devs();
		int part, sz_part, tot_C = sz_n * sz_m, ofs = 0;

		for (int i = 0; i < num; ++i) {
			fd = _pnvl_fd(i);
			part = PART_FOR_DEV(i, tot_C, num);
			sz_part = part * sizeof(TYPE);

			printf("Sending args to dev(%d): ", i);
			rc = _pnvl_send_args(fd, sz_n, sz_t, sz_m, ofs, part);
			printf("%d\n", rc);

			printf("Sending A to dev(%d): ", i);
			rc = _pnvl_send(fd, A, sz_n * sz_t * sizeof(TYPE));
			printf("%d\n", rc);

			printf("Sending B to dev(%d): ", i);
			rc = _pnvl_send(fd, B, sz_t * sz_m * sizeof(TYPE));
			printf("%d\n", rc);

			printf("Sending %d/%d to dev(%d): ", part, tot_C, i);
			rc = _pnvl_asend(fd, &C[ofs], sz_part);
			printf("%d\n", rc);

			printf("Waiting for dev(%d): ", i);
			rc = _pnvl_wait(fd);
			printf("%d\n", rc);

			ofs += part;
		}
	} /* PNVL PART END ===================================== */

	t1 = now();

	{ /* PNVL PART START =================================== */
		_pnvl_close_devs();
	} /* PNVL PART END ===================================== */

	show_time(msg, t0, t1);
	printf("\nMatrices: ( %d %d ) x ( %d %d ) -> ( %d %d )\n",
			sz_n, sz_t, sz_t, sz_m, sz_n, sz_m);
	double timetaken = t1 - t0;
	printf("Multiplication time: %lf s\n", timetaken);
	double operations = (double) sz_n*sz_t*sz_m*2.0;
	printf("Operations:\t%lf\n", operations);
	printf("MFlops:\t%lf\n", operations/(timetaken*1000000.0));
}

void matrix_init(char * msg, int rows, int cols, TYPE (* mat)[cols],
		TYPE random)
{
	int loop, i, j;
	//double t0, t1;

	// Repeat initialization 4 times to check if there is
	// any timing variability
	for (loop = 0; loop < 4; loop++) {
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
	}
	//printf("\n");
}

TYPE A[SIZE_N][SIZE_T];
TYPE B[SIZE_T][SIZE_M];
TYPE *C; 

int main(int argc, char *argv[])
{
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

	printf("Matrix sizes %d %d %d\n", sz_n, sz_t, sz_m);

	C = matrix_allocate("C", sz_n, sz_m);

	matrix_init("init(A)", sz_n, sz_t, A, 0.04);
	matrix_init("init(B)", sz_t, sz_m, B, 0.07);
	matrix_init("init(C)", sz_n, sz_m, (TYPE (*)[sz_m]) C, 0.0);

	printf("Iniciant matmul, pid = %d\n", getpid());

	matmul("C += AxB", sz_n, sz_t, sz_m, (TYPE (*)[sz_m]) C, A, B);

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
	int err = posix_memalign((void *)&m, getpagesize(), rows*cols*sizeof(TYPE) + getpagesize());
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
