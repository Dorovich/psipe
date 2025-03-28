#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include "sw/module/pnvl_ioctl.h"
#include "pnvl_wrappers.h"

extern struct _pnvl_devices *_pnvl_devs;

int _pnvl_count_devs(void)
{
	DIR *dir = opendir("/dev/pnvl");
	struct dirent *entry;
	int count = 0;

	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_CHR)
			count++;
	}

	return count;
}

void _pnvl_close_devs(void)
{
	for (int i = 0; i < _pnvl_devs->num; ++i)
		close(_pnvl_devs->fds[i]);
	free(_pnvl_devs->fds);
	free(_pnvl_devs);
}

void _pnvl_open_devs(void)
{
	struct dirent *entry;

	DIR *dir = opendir("/dev/pnvl");
	if (!dir) {
		perror("Could not open PNVL devices.");
		exit(1);
	}

	int num = _pnvl_count_devs();
	if (!num) {
		perror("No devices found.");
		exit(1);
	}

	_pnvl_devs = malloc(sizeof(*_pnvl_devs));
	_pnvl_devs->num = num;
	_pnvl_devs->fds = malloc(num * sizeof(int));

	char path[512];
	int i = 0;
	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_CHR) {
			snprintf(path, sizeof(path), "/dev/pnvl/%s",
					entry->d_name);
			_pnvl_devs->fds[i++] = open(path, O_RDWR | O_SYNC);
		}
	}
}

void _pnvl_recv_matmul_params(int fd, int *sz_n, int *sz_t, int *sz_m,
		int *g_len, int *g_ofs)
{
	int params[5];
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	ioctl(fd, PNVL_IOCTL_RECV, &data);
	*sz_n = params[0];
	*sz_t = params[1];
	*sz_m = params[2];
	*g_len = params[3];
	*g_ofs = params[4];
}

void _pnvl_recv(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	ioctl(fd, PNVL_IOCTL_RECV, &data);
}

void _pnvl_arecv(int fd, void *addr, size_t len)
{
	struct pnvl_data *data = malloc(sizeof(*data));
	data->addr = (unsigned long)addr;
	data->len = (unsigned long)len;
	ioctl(fd, PNVL_IOCTL_ARECV, data);
	free(data);
}

void _pnvl_return(int fd)
{
	ioctl(fd, PNVL_IOCTL_RETURN);
}

void _pnvl_send_matmul_params(int fd, int sz_n, int sz_t, int sz_m)
{
	int params[3] = { sz_n, sz_t, sz_m };
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	ioctl(fd, PNVL_IOCTL_SEND, &data);
}

void _pnvl_send_matmul_params_all(int sz_n, int sz_t, int sz_m)
{
	int params[5] = { sz_n, sz_t, sz_m, 0, 0 };
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	for (int i = 0; i < _pnvl_devs->num; ++i) {
		// accumulated offset
		params[4] += params[3];
		// part length
		params[3] = PART_FOR_DEV(i, sz_n * sz_m, _pnvl_devs->num);
		printf("dev %d: offset = %d, length = %d\n", i,
				params[4], params[3]);
		ioctl(_pnvl_devs->fds[i], PNVL_IOCTL_SEND, &data);
	}
}

void _pnvl_send(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	ioctl(fd, PNVL_IOCTL_SEND, &data);
}

void _pnvl_send_all(void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	for (int i = 0; i < _pnvl_devs->num; ++i)
		ioctl(_pnvl_devs->fds[i], PNVL_IOCTL_SEND, &data);
}

void _pnvl_asend(int fd, void *addr, size_t len)
{
	struct pnvl_data *data = malloc(sizeof(*data));
	data->addr = (unsigned long)addr;
	data->len = (unsigned long)len;
	ioctl(fd, PNVL_IOCTL_ASEND, data);
	free(data);
}

void _pnvl_wait(int fd)
{
	ioctl(fd, PNVL_IOCTL_WAIT);
}
