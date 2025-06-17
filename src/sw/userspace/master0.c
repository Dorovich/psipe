/* master.c - Userspace program to test the device (offload operation)
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "hw/psipe_hw.h"
#include "sw/module/psipe_ioctl.h"
#include "psipe_util.h"

static int offload_work(int fd, void *addr, size_t len)
{
	struct timeval t1, t2, t3;
	struct psipe_data data = {
		.addr = (unsigned long)addr,
		.len = (unsigned long)len,
	};

	puts("Sending data...");

	gettimeofday(&t1, NULL);
	if (ioctl(fd, psipe_IOCTL_ASEND, &data) < 0) {
		perror("psipe_IOCTL_ASEND failed!");
		return -1;
	}
	gettimeofday(&t2, NULL);

	printf("psipe_IOCTL_ASEND: %ld us elapsed\n", calc_time(&t1, &t2));

	gettimeofday(&t2, NULL);
	if (ioctl(fd, psipe_IOCTL_WAIT) < 0) {
		perror("psipe_IOCTL_WAIT failed!");
		return -1;
	}
	gettimeofday(&t3, NULL);

	printf("psipe_IOCTL_WAIT: %ld us elapsed\n", calc_time(&t2, &t3));
	printf("TOTAL: %ld us elapsed\n", calc_time(&t1, &t3));

	puts("Checking results...");

	int errcnt = 0;
	for (int i = 0; i < data.len/sizeof(int); ++i) {
		if (((int *)data.addr)[i] != i)
			errcnt++;
	}

	printf("%d errors counted.\n", errcnt);

	return 0;
}

int main(int argc, char **argv)
{
	struct context ctx = parse_args(argc, argv);
	int *data;

	if (!ctx.vec_len)
		return -1;

	if (open_psipe_dev(&ctx) < 0)
		return -2;

	data = calloc(ctx.vec_len, sizeof(int));
	if (!data)
		return -3;

	if (offload_work(ctx.fd, data, ctx.vec_len * sizeof(int))) {
		close(ctx.fd);
		return -4;
	}

	close(ctx.fd);
	return 0;
}
