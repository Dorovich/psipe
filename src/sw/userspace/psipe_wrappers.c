#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include "psipe_wrappers.h"

extern struct psipe_devices *psipe_devs;

int psipe_fd(int id)
{
	if (id >= psipe_devs->num || id < 0)
		return -1;
	return psipe_devs->fds[id];
}

int psipe_num_devs(void)
{
	return psipe_devs->num;
}

static int psipe_count_devs(void)
{
	DIR *dir = opendir("/dev/psipe");
	struct dirent *entry;
	int count = 0;

	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_CHR)
			count++;
	}

	closedir(dir);

	return count;
}

int psipe_open_devs(void)
{
	struct dirent *entry;

	DIR *dir = opendir("/dev/psipe");
	if (!dir) {
		perror("Could not open PSIPE devices.");
		return -1;
	}

	int num = psipe_count_devs();
	if (!num) {
		perror("No devices found.");
		return -1;
	}

	psipe_devs = malloc(sizeof(*psipe_devs));
	psipe_devs->num = num;
	psipe_devs->fds = malloc(num * sizeof(int));

	char path[512];
	int i = 0;
	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_CHR) {
			snprintf(path, sizeof(path), "/dev/psipe/%s",
					entry->d_name);
			psipe_devs->fds[i++] = open(path, O_RDWR | O_SYNC);
		}
	}

	closedir(dir);

	return 0;
}

int psipe_close_devs(void)
{
	for (int i = 0; i < psipe_devs->num; ++i)
		close(psipe_devs->fds[i]);
	free(psipe_devs->fds);
	free(psipe_devs);
	return 0;
}

int psipe_send(int fd, void *addr, size_t len)
{
	struct psipe_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PSIPE_IOCTL_SEND, &data);
}

int psipe_recv(int fd, void *addr, size_t len)
{
	struct psipe_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};
	return ioctl(fd, PSIPE_IOCTL_RECV, &data);
}

int psipe_wait(int fd, psipe_handle_t id)
{
	return ioctl(fd, PSIPE_IOCTL_WAIT, id);
}

int psipe_flush(int fd)
{
	return ioctl(fd, PSIPE_IOCTL_FLUSH);
}

int psipe_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs)
{
	int params[5] = { sz_n, sz_t, sz_m, len, ofs };
	return psipe_send(fd, params, sizeof(params));
}

int psipe_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs)
{
	int rv, params[5];

	rv = psipe_recv(fd, params, sizeof(params));
	*sz_n = params[0];
	*sz_t = params[1];
	*sz_m = params[2];
	*len = params[3];
	*ofs = params[4];

	return rv;
}
