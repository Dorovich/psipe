#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include "sw/module/pnvl_ioctl.h"

#define TYPE double
#define TRUNCATE 1
#define APPEND   2
#define SIZE_N 1224
#define SIZE_T 960
#define SIZE_M 1446

/* ============================================================================
 * AUXILIARY FUNCTIONS
 * ============================================================================
 */

struct _pnvl_devs {
	int num;
	int *fds;
};

static int _pnvl_matmul_count_devs()
{
	const char *devs_path = "/dev/pnvl";
	DIR *dir = opendir(devs_path);
	struct dirent *entry;
	int count = 0;

	while ((entry = readdir(dir))) {
		if (!strcmp(entry->d_name, ".")
				|| !strcmp(entry->d_name, "..")
				|| entry->d_type != DT_REG)
			continue;
		count++;
	}

	return count;
}

static void _pnvl_matmul_close_devs(struct _pnvl_devs *devs)
{
	for (int i = 0; i < devs->num; ++i) {
		if (devs->fds[i])
			close(devs->fds[i]);
	}
	free(devs->fds);
	free(devs);
}

static struct _pnvl_devs *_pnvl_matmul_open_devs()
{
	const char *devs_path = "/dev/pnvl";
	DIR *dir = opendir(devs_path);
	struct dirent *entry;
	struct _pnvl_devs *devs = malloc(sizeof(*devs));
	devs->num = _pnvl_matmul_count_devs();
	devs->fds = calloc(devs->num, sizeof(int));

	if (!dir) {
		perror("opendir");
		goto _open_err;
	}

	char path[512];
	int i = 0;
	while ((entry = readdir(dir))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		snprintf(path, sizeof(path), "%s/%s", devs_path, entry->d_name);
		devs->fds[i++] = open(path, O_RDWR | O_SYNC);
	}

	return devs;

_open_err:
	_pnvl_matmul_close_devs(devs);
	return NULL;
}

static void _pnvl_matmul_send_params(int fd, int sz_n, int sz_t, int sz_m)
{
	int params[3] = { sz_n, sz_t, sz_m };
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	ioctl(fd, PNVL_IOCTL_SEND, &data);
}

static void _pnvl_matmul_send_matrix(int fd, int sz_r, int sz_c, TYPE (*mat)[sz_c])
{
	struct pnvl_data data = {
		.addr = (unsigned long)mat,
		.len = (unsigned long)(sz_r * sz_c * sizeof(TYPE)),
	};
	ioctl(fd, PNVL_IOCTL_SEND, &data);
}

/*
static struct pnvl_data *_pnvl_matmul_asend_matrix(int fd, int sz_r, int sz_c,
		TYPE (*mat)[sz_c])
{
	struct pnvl_data *data = malloc(sizeof(*data));
	data->addr = (unsigned long)mat,
	data->len = (unsigned long)(sz_r * sz_c * sizeof(TYPE));
	ioctl(fd, PNVL_IOCTL_ARECV, data);
	return data;
}

static void _pnvl_matmul_wait(int fd, struct pnvl_data *data)
{
	ioctl(fd, PNVL_IOCTL_WAIT, data);
	free(data);
}
*/

/* ============================================================================
 * MAIN FUNCTIONS
 * ============================================================================
 */

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
	struct _pnvl_devs *devs = _pnvl_matmul_open_devs();
	int fd = devs->fds[0];

	printf ("num_devs %d\n", devs->num);
	t0 = now();

	_pnvl_matmul_send_params(fd, sz_n, sz_t, sz_m);
	_pnvl_matmul_send_matrix(fd, sz_n, sz_t, A);
	_pnvl_matmul_send_matrix(fd, sz_t, sz_m, B);
	_pnvl_matmul_send_matrix(fd, sz_n, sz_m, C);

	t1 = now();

	_pnvl_matmul_close_devs(devs);

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
	double t0, t1;

	// Repeat initialization 4 times to check if there is
	// any timing variability
	for (loop = 0; loop < 4; loop++) {
		t0 = now();

		for (i=0; i < rows; i++) {
			for (j=0; j < cols; j++) {
				TYPE value = random *
					((TYPE)(1.0/((TYPE)i+0.25)) - 0.005*j);
				mat[i][j] = value;
			}
		}

		t1 = now();

		show_time(msg, t0, t1);
	}
	printf("\n");
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
	TYPE *m = malloc(rows*cols*sizeof(TYPE));
	if (m == NULL) {
		fprintf(stderr, "malloc(%s): %s\n", name, strerror(errno));
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
