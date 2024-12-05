/* watch.c - Userspace program to test the device (compute operation)
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "hw/pnvl_hw.h"
#include "pnvl_util.h"
#include "sw/module/pnvl_ioctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int ioctl_pnvl_watch(struct context *ctx, void *addr, size_t len)
{
	int ret;
	struct pnvl_data data;

	data.in_addr = (unsigned long)addr;
	data.in_len = (unsigned long)len;

	ret = ioctl(ctx->fd, PNVL_IOCTL_WATCH, &data);
	if (ret < 0) {
		perror("PNVL_IOCTL_WATCH failed!");
		return -1;
	}

	for (int i = 0; i < data.out_len/sizeof(int); ++i)
		((int *)data.out_addr)[i] = i;

	/* TODO: send results back */

	return 0;
}

int main(int argc, char **argv)
{
	struct context ctx = parse_args(argc, argv);
	int *data;
	size_t len, data_len;

	if (open_pnvl_dev(&ctx) == -1)
		return -1;

	len = 10;
	data_len = len * sizeof(int);
	data = malloc(data_len);

	if (ioctl_pnvl_watch(&ctx, data, data_len)) {
		close(ctx.fd);
		return -1;
	}

	for (int i = 0; i < len; ++i)
		printf("data[%d] =\t%d\n", i, data[i]);

	close(ctx.fd);
	return 0;
}
