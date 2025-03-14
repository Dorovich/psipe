#include <sys/ioctl.h>
#include "sw/module/pnvl_ioctl.h"
#include "matmul.h"

void _pnvl_matmul_recv_params(int fd, int *sz_n, int *sz_t, int *sz_m)
{
	int params[3];
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	ioctl(fd, PNVL_IOCTL_RECV, &data);
	*sz_n = params[0];
	*sz_t = params[1];
	*sz_m = params[2];
}

void _pnvl_matmul_recv_matrix(int fd, TYPE *mat, size_t cells)
{
	mat = malloc(cells * sizeof(TYPE));
	struct pnvl_data data = {
		.addr = (unsigned long)mat,
		.len = (unsigned long)(cells*sizeof(TPYE)),
	};
	ioctl(fd, PNVL_IOCTL_RECV, &data);
}

struct pnvl_data *_pnvl_matmul_arecv_matrix(int fd, TYPE *mat, size_t cells)
{
	mat = malloc(cells * sizeof(TYPE));
	struct pnvl_data *data = malloc(sizeof(*data));
	data->addr = (unsigned long)mat,
	data->len = (unsigned long)(cells*sizeof(TPYE)),
	ioctl(fd, PNVL_IOCTL_ARECV, data);
	return data;
}

void _pnvl_matmul_return(struct pnvl_data *data)
{
	ioctl(fd, PNVL_IOCTL_RETURN, data);
	free(data);
}

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

int main(int argc, char *argv [])
{
	int fd; // TODO: open cdev file
	int sz_n, sz_t, sz_m;
	TYPE **A, **B, **C;
	struct pnvl_data *handle;

	_pnvl_matmul_recv_params(fd, &sz_n, &sz_t, &sz_m);
	_pnvl_matmul_recv_matrix(fd, A, sz_n * sz_t);
	_pnvl_matmul_recv_matrix(fd, B, sz_t * sz_m);
	handle = _pnvl_matmul_arecv_matrix(fd, C, sz_n * sz_m);
	matmul_func(sz_n, sz_t, sz_m, C, A, B);
	_pnvl_matmul_return(handle);

	free(A);
	free(B);
	free(C);

	return 0;
}
