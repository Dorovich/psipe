#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include "pnvl_wrappers.h"

extern struct pnvl_devices *pnvl_devs;

int pnvl_fd(int id)
{
	if (id >= pnvl_devs->num || id < 0)
		return -1;
	return pnvl_devs->fds[id];
}

int pnvl_num_devs(void)
{
	return pnvl_devs->num;
}

static int pnvl_count_devs(void)
{
	DIR *dir = opendir("/dev/pnvl");
	struct dirent *entry;
	int count = 0;

	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_CHR)
			count++;
	}

	closedir(dir);

	return count;
}

int pnvl_open_devs(void)
{
	struct dirent *entry;

	DIR *dir = opendir("/dev/pnvl");
	if (!dir) {
		perror("Could not open PNVL devices.");
		return -1;
	}

	int num = pnvl_count_devs();
	if (!num) {
		perror("No devices found.");
		return -1;
	}

	pnvl_devs = malloc(sizeof(*pnvl_devs));
	pnvl_devs->num = num;
	pnvl_devs->fds = malloc(num * sizeof(int));

	char path[512];
	int i = 0;
	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_CHR) {
			snprintf(path, sizeof(path), "/dev/pnvl/%s",
					entry->d_name);
			pnvl_devs->fds[i++] = open(path, O_RDWR | O_SYNC);
		}
	}

	closedir(dir);

	return 0;
}

int pnvl_close_devs(void)
{
	for (int i = 0; i < pnvl_devs->num; ++i)
		close(pnvl_devs->fds[i]);
	free(pnvl_devs->fds);
	free(pnvl_devs);
	return 0;
}

int pnvl_send(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PNVL_IOCTL_SEND, &data);
}

int pnvl_recv(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PNVL_IOCTL_RECV, &data);
}

int pnvl_wait(int fd, pnvl_handle_t id)
{
	return ioctl(fd, PNVL_IOCTL_WAIT, id);
}

int pnvl_flush(int fd)
{
	return ioctl(fd, PNVL_IOCTL_FLUSH);
}

int pnvl_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs)
{
	int params[5] = { sz_n, sz_t, sz_m, len, ofs };
	return pnvl_send(fd, params, sizeof(params));
}

int pnvl_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs)
{
	int rv, params[5];

	rv = pnvl_recv(fd, params, sizeof(params));
	*sz_n = params[0];
	*sz_t = params[1];
	*sz_m = params[2];
	*len = params[3];
	*ofs = params[4];

	return rv;
}
