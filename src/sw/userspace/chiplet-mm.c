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

	int sz_n, sz_t, sz_m, g_len, g_ofs, fd;
	void *pt_A = NULL, *pt_B = NULL, *pt_C = NULL;
	size_t sz_A, sz_B, sz_C;
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
	if (id < 0) {
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
	if (id < 0) {
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
	if (id < 0) {
		perror("pnvl_recv(C)");
		exit(1);
	}
	if (pnvl_wait(fd, id) < 0) {
		perror("pnvl_wait(C)");
		exit(1);
	}

	TYPE *A = (TYPE *)pt_A;
	TYPE *B = (TYPE *)pt_B;
	TYPE *C = (TYPE *)pt_C;

	/* FUNCTION START -------------------------------------- */
	for (int g=0; g < g_len; g++) {
		int ci = g / sz_m;
		int cj = g % sz_m;
		int i = (g+g_ofs) / sz_m;
		int j = (g+g_ofs) % sz_m;
		for (int k=0; k < sz_t; k++) {
			/* Access to C is offset:
			 * 	C[ci][cj] += A[i][k] * B[k][j];
			 */
			C[ci*sz_m+cj] += A[i*sz_t+k] * B[k*sz_m+j];
		}
	}
	/* FUNCTION END ---------------------------------------- */

	/*
	for (int i=0; i<sz_n; ++i) {
		double cnt = 0;
		for (int j=0; j<sz_m; ++j)
			cnt += ((TYPE *)pt_C)[i*sz_n+j];
		printf("[%d] %lf\t", i, cnt);
		if (i%5 == 4)
			printf("\n");
	}
	*/

	id = pnvl_send(fd, pt_C, sz_C);
	if (id < 0) {
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
