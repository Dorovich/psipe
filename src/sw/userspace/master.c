/* master.c - Userspace program to test the device (offload operation)
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "hw/pnvl_hw.h"
#include "sw/module/pnvl_ioctl.h"
#include "pnvl_util.h"

/*
static int offload_work_short(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};

	ioctl(fd, PNVL_IOCTL_WORK, &data);
	ioctl(fd, PNVL_IOCTL_WAIT, NULL);

	for (int i = 0; i < data.len/sizeof(int); ++i)
		printf("data[%d] =\t%d\n", i, ((int *)data.addr)[i]);

	return 0;
}
*/

static int offload_work(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};

	puts("Reading initial data...");

	for (int i = 0; i < data.len/sizeof(int); ++i)
		printf("data[%d] =\t%d\n", i, ((int *)data.addr)[i]);

	puts("Sending data...");

	if (ioctl(fd, PNVL_IOCTL_WORK, &data) < 0) {
		perror("PNVL_IOCTL_WORK failed!");
		return -1;
	}

	puts("Data sent! Waiting results...");

	if (ioctl(fd, PNVL_IOCTL_WAIT, NULL) < 0) {
		perror("PNVL_IOCTL_WAIT failed!");
		return -1;
	}

	puts("Results received! Reading results...");

	for (int i = 0; i < data.len/sizeof(int); ++i)
		printf("data[%d] =\t%d\n", i, ((int *)data.addr)[i]);

	return 0;
}

int main(int argc, char **argv)
{
	struct context ctx = parse_args(argc, argv);
	int *data;
	size_t len, data_len;

	if (open_pnvl_dev(&ctx) < 0)
		return -1;

	len = 10;
	data_len = len * sizeof(int);
	data = malloc(data_len);
	memset(data, 0, data_len);

	if (offload_work(ctx.fd, data, data_len)) {
		close(ctx.fd);
		return -1;
	}

	close(ctx.fd);
	return 0;
}
