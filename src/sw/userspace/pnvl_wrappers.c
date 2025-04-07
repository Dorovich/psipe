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

int _pnvl_close_devs(void)
{
	for (int i = 0; i < _pnvl_devs->num; ++i)
		close(_pnvl_devs->fds[i]);
	free(_pnvl_devs->fds);
	free(_pnvl_devs);
	return 0;
}

int _pnvl_open_devs(void)
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

	return 0;
}

int _pnvl_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs)
{
	int rv, params[5];
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};

	rv = ioctl(fd, PNVL_IOCTL_RECV, &data);
	*sz_n = params[0];
	*sz_t = params[1];
	*sz_m = params[2];
	*len = params[3];
	*ofs = params[4];

	return rv;
}

int _pnvl_recv(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PNVL_IOCTL_RECV, &data);
}

int _pnvl_arecv(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PNVL_IOCTL_ARECV, &data);
}

int _pnvl_return(int fd)
{
	return ioctl(fd, PNVL_IOCTL_RETURN);
}

int _pnvl_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs)
{
	int params[5] = { sz_n, sz_t, sz_m, len, ofs };
	struct pnvl_data data = {
		.addr = (unsigned long)params,
		.len = (unsigned long)sizeof(params),
	};
	return ioctl(fd, PNVL_IOCTL_SEND, &data);
}

int _pnvl_send(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PNVL_IOCTL_SEND, &data);
}

int _pnvl_asend(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PNVL_IOCTL_ASEND, &data);
}

int _pnvl_wait(int fd)
{
	return ioctl(fd, PNVL_IOCTL_WAIT);
}

int _pnvl_num_devs(void)
{
	return _pnvl_devs->num;
}

int _pnvl_fd(int id)
{
	if (id >= _pnvl_devs->num || id < 0)
		return -1;
	return _pnvl_devs->fds[id];
}
