#include <sys/ioctl.h>
#include "sw/module/pnvl_ioctl.h"
#include "matmul.h"

void _pnvl_matmul_send_params(int fd, int sz_n, int sz_t, int sz_m)
{
	int params[3] = { sz_n, sz_t, sz_m };
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	ioctl(fd, PNVL_IOCTL_SEND, &data);
}

void _pnvl_matmul_send_matrix(int fd, TYPE *mat, size_t cells)
{
	struct pnvl_data data = {
		.addr = (unsigned long)mat,
		.len = (unsigned long)(cells*sizeof(TPYE)),
	};
	ioctl(fd, PNVL_IOCTL_SEND, &data);
}

struct pnvl_data *_pnvl_matmul_asend(int fd, TYPE *mat, size_t cells)
{
	struct pnvl_data *data = malloc(sizeof(*data));
	data->addr = (unsigned long)mat,
	data->len = (unsigned long)(cells*sizeof(TPYE)),
	ioctl(fd, PNVL_IOCTL_ARECV, data);
	return data;
}

void _pnvl_matmul_wait(struct pnvl_data *data)
{
	ioctl(fd, PNVL_IOCTL_WAIT, data);
	free(data);
}
