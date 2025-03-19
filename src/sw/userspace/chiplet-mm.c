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

static void _pnvl_matmul_recv_params(int fd, int *sz_n, int *sz_t, int *sz_m)
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

static void _pnvl_matmul_recv(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	ioctl(fd, PNVL_IOCTL_RECV, &data);
}

static struct pnvl_data *_pnvl_matmul_arecv(int fd, void *addr, size_t len)
{
	struct pnvl_data *data = malloc(sizeof(*data));
	data->addr = (unsigned long)addr;
	data->len = (unsigned long)len;
	ioctl(fd, PNVL_IOCTL_ARECV, data);
	return data;
}

static void _pnvl_matmul_return(int fd, struct pnvl_data *data)
{
	ioctl(fd, PNVL_IOCTL_RETURN, data);
	free(data);
}

/* ============================================================================
 * MAIN FUNCTIONS
 * ============================================================================
 */

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
	int sz_n, sz_t, sz_m;
	TYPE *A, *B, *C;
	size_t sz_A, sz_B, sz_C;
	struct pnvl_data *handle;
	struct _pnvl_devs *devs = _pnvl_matmul_open_devs();
	if (!devs)
		return -1;
	int fd = devs->fds[0];

	_pnvl_matmul_recv_params(fd, &sz_n, &sz_t, &sz_m);
	sz_A = sz_n * sz_t * sizeof(TYPE);
	sz_B = sz_t * sz_m * sizeof(TYPE);
	sz_C = sz_n * sz_m * sizeof(TYPE);
	A = malloc(sz_A);
	B = malloc(sz_B);
	C = malloc(sz_C);
	_pnvl_matmul_recv(fd, A, sz_A);
	_pnvl_matmul_recv(fd, B, sz_B);
	handle = _pnvl_matmul_arecv(fd, C, sz_C);

	matmul_func(sz_n, sz_t, sz_m, (TYPE (*)[sz_n])C, (TYPE (*)[sz_t])A,
			(TYPE (*)[sz_m])B);

	_pnvl_matmul_return(fd, handle);
	_pnvl_matmul_close_devs(devs);
	free(A);
	free(B);
	free(C);

	return 0;
}
