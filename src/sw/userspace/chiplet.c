/* chiplet.c - Userspace program to test the device (compute operation)
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "hw/pnvl_hw.h"
#include "sw/module/pnvl_ioctl.h"
#include "pnvl_util.h"

static int handle_work(struct context *ctx, void *addr, size_t len)
{
	int ret;
	struct pnvl_data data;

	data.addr = (unsigned long)addr;
	data.len = (unsigned long)len;

	puts("Waiting for data...");

	ret = ioctl(ctx->fd, PNVL_IOCTL_WAIT, &data);
	if (ret < 0) {
		perror("PNVL_IOCTL_WAIT failed!");
		return -1;
	}

	puts("Data received! Processing data...");

	for (int i = 0; i < data.len/sizeof(int); ++i)
		((int *)data.addr)[i] = i;

	puts("Data processed. Sending results...");

	ret = ioctl(ctx->fd, PNVL_IOCTL_WORK, &data);
	if (ret < 0) {
		perror("PNVL_IOCTL_WORK failed!");
		return -1;
	}

	puts("Results sent.");

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
	bzero(data, data_len);

	if (handle_work(&ctx, data, data_len)) {
		close(ctx.fd);
		return -1;
	}

	close(ctx.fd);
	return 0;
}
