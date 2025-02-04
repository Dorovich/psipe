/* chiplet.c - Userspace program to test the device (compute operation)
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

#define VEC_LEN 3000

static int handle_work(int fd, void *addr, size_t len)
{
	struct pnvl_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};

	puts("Waiting for data...");

	if (ioctl(fd, PNVL_IOCTL_RECV, &data) < 0) {
		perror("PNVL_IOCTL_RECV failed!");
		return -1;
	}

	puts("Data received! Processing data...");

	for (int i = 0; i < data.len/sizeof(int); ++i)
		((int *)data.addr)[i] = i;

	puts("Data processed. Sending results...");

	if (ioctl(fd, PNVL_IOCTL_RETURN) < 0) {
		perror("PNVL_IOCTL_RETURN failed!");
		return -1;
	}

	puts("Results sent.");

	return 0;
}

int main(int argc, char **argv)
{
	struct context ctx = parse_args(argc, argv);
	int *data;
	size_t data_len;

	if (open_pnvl_dev(&ctx) < 0)
		return -1;

	data_len = VEC_LEN * sizeof(int);
	data = malloc(data_len);
	memset(data, 0, data_len);

	if (handle_work(ctx.fd, data, data_len)) {
		close(ctx.fd);
		return -1;
	}

	close(ctx.fd);
	return 0;
}
