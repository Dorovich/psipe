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
#include <signal.h>

static void sighup_handler(int signo)
{
	/* Just in case a SIGHUP is received */
	return;
}

#define TYPE double

struct pnvl_devices *pnvl_devs;

/* ORIGINAL FUNCTION
static void matmul_func(int sz_n, int sz_t, int sz_m,
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
*/

int main(int argc, char *argv[])
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sa, NULL);

	void *pt_A = NULL, *pt_B = NULL, *pt_C = NULL;
	size_t sz_A, sz_B, sz_C;
	int sz_n, sz_t, sz_m, g_len, g_ofs, fd;
	pnvl_handle_t id;

	pnvl_open_devs();
	fd = pnvl_devs->fds[0];

	id = pnvl_recv_args(fd, &sz_n, &sz_t, &sz_m, &g_len, &g_ofs);
	if (id < 0) {
		perror("pnvl_recv_args");
		exit(1);
	}

	if (pnvl_wait(fd, id) < 0) {
		perror("pnvl_wait(args)");
		exit(1);
	}

	printf("sz_n=%d, sz_t=%d, sz_m=%d, g_len=%d, g_ofs=%d\n",
			sz_n, sz_t, sz_m, g_len, g_ofs);

	sz_A = sz_n * sz_t * sizeof(TYPE);
	sz_B = sz_t * sz_m * sizeof(TYPE);
	sz_C = g_len * sizeof(TYPE);

	if (posix_memalign((void *)&pt_A, sizeof(TYPE), sz_A) < 0) {
		perror("posix_memalign(A)");
		exit(1);
	}
	if (posix_memalign((void *)&pt_B, sizeof(TYPE), sz_B) < 0) {
		perror("posix_memalign(B)");
		exit(1);
	}
	if (posix_memalign((void *)&pt_C, sizeof(TYPE), sz_C) < 0) {
		perror("posix_memalign(C)");
		exit(1);
	}

	id = pnvl_recv(fd, pt_A, sz_A);
	if ((long)id < 0) {
		perror("pnvl_recv(A)");
		exit(1);
	}
#if WAIT_ALL_OPS
	if (pnvl_wait(fd, id) < 0) {
		perror("pnvl_wait(A)");
		exit(1);
	}
#endif

	id = pnvl_recv(fd, pt_B, sz_B);
	if ((long)id < 0) {
		perror("pnvl_recv(B)");
		exit(1);
	}
#if WAIT_ALL_OPS
	if (pnvl_wait(fd, id) < 0) {
		perror("pnvl_wait(B)");
		exit(1);
	}
#endif

	id = pnvl_recv(fd, pt_C, sz_C);
	if ((long)id < 0) {
		perror("pnvl_recv(C)");
		exit(1);
	}
	if (pnvl_wait(fd, id) < 0) {
		perror("pnvl_wait(C)");
		exit(1);
	}

	/* FUNCTION START -------------------------------------- */
	TYPE *A = (TYPE *)pt_A, *B = (TYPE *)pt_B, *C = (TYPE *)pt_C;
	for (int g = g_ofs; g < g_ofs+g_len; g++) {
		int i = g / sz_m;
		int j = g % sz_m;
		for (int k=0; k < sz_t; k++) {
			int a_idx = i*sz_t+k;
			int b_idx = k*sz_m+j;
			int c_idx = g-g_ofs;

			if (a_idx < 0 || a_idx >= sz_n * sz_t)
				printf("incorrect A index for g=%d, i=%d, k=%d\n", g, i, k);
			else if (b_idx < 0 || b_idx >= sz_t * sz_m)
				printf("incorrect B index for g=%d, j=%d, k=%d\n", g, j, k);
			else if (c_idx < 0 || c_idx >= g_len)
				printf("incorrect C index for g=%d\n", g);
			else {
				TYPE a = A[i*sz_t+k];
				TYPE b = B[k*sz_m+j];
				C[g-g_ofs] += (TYPE)(a*b);
				//C[g-g_ofs] += A[i*sz_t+k] * B[k*sz_m+j];
			}
		}
	}
	/* FUNCTION END ---------------------------------------- */

	id = pnvl_send(fd, pt_C, sz_C);
	if ((long)id < 0) {
		perror("pnvl_send(C)");
		exit(1);
	}
	if (pnvl_wait(fd, id) < 0) {
		perror("pnvl_wait(C)");
		exit(1);
	}

	pnvl_flush(fd);
	pnvl_close_devs();
	free(pt_A);
	free(pt_B);
	free(pt_C);

	return 0;
}
