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
#include "hw/pnvl_hw.h"
#include "sw/module/pnvl_ioctl.h"
#include "pnvl_util.h"

static int offload_work(struct context_multi *ctx)
{
	struct timeval t1, t2, t3;
	struct pnvl_data data = {
		.addr = (unsigned long)ctx->data,
		.len = (unsigned long)(ctx->data_len * sizeof(int)),
	};

	for (int i = 0; i < ctx->fds_len; ++i) {
		puts("Sending data...");

		gettimeofday(&t1, NULL);
		if (ioctl(ctx->fds[i], PNVL_IOCTL_ASEND, &data) < 0) {
			perror("PNVL_IOCTL_ASEND failed!");
			return -1;
		}
		gettimeofday(&t2, NULL);

		printf("PNVL_IOCTL_ASEND: %ld us elapsed\n",
				calc_time(&t1, &t2));

		gettimeofday(&t2, NULL);
		if (ioctl(ctx->fds[i], PNVL_IOCTL_WAIT) < 0) {
			perror("PNVL_IOCTL_WAIT failed!");
			return -1;
		}
		gettimeofday(&t3, NULL);

		printf("PNVL_IOCTL_WAIT: %ld us elapsed\n",
				calc_time(&t2, &t3));
		printf("TOTAL: %ld us elapsed\n",
				calc_time(&t1, &t3));
	}

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
	struct context_multi ctx = parse_args_multi(argc, argv);

	if (offload_work(&ctx)) {
		free_context_multi(&ctx);
		return -4;
	}

	free_context_multi(&ctx);
	return 0;
}
