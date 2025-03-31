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

/* ORIGINAL FUNCTION
void matmul_func(int sz_n, int sz_t, int sz_m, TYPE (* __restrict__ C)[sz_m],
		TYPE (* __restrict__ A)[sz_t], TYPE (* __restrict__ B)[sz_m])
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
	int sz_n, sz_t, sz_m;
	TYPE *pt_A, *pt_B, *pt_C;
	size_t sz_A, sz_B, sz_C;
	_pnvl_open_devs();
	int fd = _pnvl_devs->fds[0];
	int g_len, g_ofs;

	_pnvl_recv_args(fd, &sz_n, &sz_t, &sz_m, &g_len, &g_ofs);
	sz_A = sz_n * sz_t * sizeof(TYPE);
	sz_B = sz_t * sz_m * sizeof(TYPE);
	sz_C = g_len * sizeof(TYPE);
	pt_A = malloc(sz_A);
	pt_B = malloc(sz_B);
	pt_C = malloc(sz_C);
	_pnvl_recv(fd, pt_A, sz_A);
	_pnvl_recv(fd, pt_B, sz_B);
	_pnvl_arecv(fd, pt_C, sz_C);

	TYPE (* __restrict__ A)[sz_t] = (TYPE (*)[sz_t])pt_A;
	TYPE (* __restrict__ B)[sz_m] = (TYPE (*)[sz_m])pt_B;
	TYPE (* __restrict__ C)[sz_m] = (TYPE (*)[sz_m])pt_C;

	/* FUNCTION START -------------------------------------- */
	for (int g=0; g < g_len; g++) {
		int ci = g / sz_m;
		int cj = g % sz_m;
		int i = (g+g_ofs) / sz_m;
		int j = (g+g_ofs) % sz_m;
		for (int k=0; k < sz_t; k++) {
			C[ci][cj] += A[i][k] * B[k][j];
		}
	}
	/* FUNCTION END ---------------------------------------- */

	_pnvl_return(fd);
	_pnvl_close_devs();
	free(pt_A);
	free(pt_B);
	free(pt_C);

	return 0;
}
