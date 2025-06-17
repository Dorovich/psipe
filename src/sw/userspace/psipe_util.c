/* psipe_util.c - Utility functions for userspace programs
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "psipe_util.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* LOGs & co*/
#define KERR "\e[1;31m"
#define KNORM "\e[0m"
#define LOGF(fd, ...) fprintf(fd, __VA_ARGS__)
#define LOG_ERR(...)				\
	LOGF(stderr, KERR __VA_ARGS__);		\
	LOGF(stderr, KNORM);
#define LOG(...) LOGF(stdout, __VA_ARGS__)

/* First invalid number (by definition PCI domain numbers are 16 bits long) */
#define PCI_DOMAIN_NUMBER_INVALID (1 << 16)
/* First invalid number (by definition PCI bus numbers are 8 bits long) */
#define PCI_BUS_NUMBER_INVALID (1 << 8)
/* First invalid (by definition PCI device numbers are 5 bits long) */
#define PCI_DEVICE_NUMBER_INVALID (1 << 5)
/* First invalid number (by definition PCI function numbers are 3 bits long) */
#define PCI_FUNCTION_NUMBER_INVALID (1 << 3)

void usage(FILE *fd, char **argv)
{
	LOGF(fd, "Usage : %s [-b bus] [-d dom] [-h] [-r regs] [-s slot] [-v]\n",
		argv[0]);
	LOGF(fd, " \t -b bus \n\t\t pci bus number of device\n");
	LOGF(fd, " \t -d dom \n\t\t pci domain number of device\n");
	LOGF(fd, " \t -h \n\t\t display this help message\n");
	LOGF(fd, " \t -r regs \n\t\t number of 64bit device registers\n");
	LOGF(fd, " \t -s slot \n\t\t pci device number of device\n");
	LOGF(fd, " \t -v \n\t\t run on verbose mode\n");
}

void usage_multi(FILE *fd, char **argv)
{
	LOGF(fd, "Usage : %s [-b bus] [-d dom] [-h] [-r regs] [-s slot] [-v]\n",
		argv[0]);
	LOGF(fd, " \t -n len \n\t\t number of chiplet connections\n");
	LOGF(fd, " \t -d filename \n\t\t pci domain number of device\n");
	LOGF(fd, " \t -l len \n\t\t length of integers to transmit\n");
	LOGF(fd, " \t -h \n\t\t display this help message\n");
}

long int calc_time(struct timeval *t1, struct timeval *t2)
{
	long int us1, us2;

	us1 = t1->tv_sec * 1000000 + t1->tv_usec;
	us2 = t2->tv_sec * 1000000 + t2->tv_usec;

	return us2 - us1;
}

int open_psipe_dev(struct context *ctx)
{
	char filename[64];

	snprintf(filename, 64, "/dev/psipe/d%ub%ud%uf%u_bar0",
		ctx->pci_domain_nb, ctx->pci_bus_nb, ctx->pci_device_nb,
		ctx->pci_func_nb);

	ctx->fd = open(filename, O_RDWR | O_SYNC);
	if (ctx->fd == -1) {
		LOG_ERR("fd failed - file %s\n", filename);
		close(ctx->fd);
	}

	return ctx->fd;
}

struct context parse_args(int argc, char **argv)
{
	int op;
	char *endptr;
	struct context ctx;

	/* change domain_nb if necessary */
	ctx.pci_domain_nb = 0;
	ctx.pci_device_nb = PCI_DEVICE_NUMBER_INVALID;
	ctx.pci_bus_nb = PCI_BUS_NUMBER_INVALID;
	/* PCI funciton number is always 0 for psipe */
	ctx.pci_func_nb = 0;

	ctx.vec_len = 0;

	while ((op = getopt(argc, argv, "b:d:hr:s:l:v")) != -1) {
		switch (op) {
		case 'b':
			ctx.pci_bus_nb = strtol(optarg, &endptr, 10);
			if (errno != 0 || optarg == endptr) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			if (ctx.pci_bus_nb >= PCI_BUS_NUMBER_INVALID) {
				LOG_ERR("PCI bus number (%u) out of range"
					"([0, %u])\n", ctx.pci_bus_nb,
					PCI_BUS_NUMBER_INVALID - 1);
				exit(-1);
			}
			break;
		case 'd':
			ctx.pci_domain_nb = strtol(optarg, &endptr, 10);
			if (errno != 0 || optarg == endptr) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			if (ctx.pci_domain_nb >= PCI_DOMAIN_NUMBER_INVALID) {
				LOG_ERR("PCI domain number (%u) out of"
					"range ([0, %u])\n", ctx.pci_domain_nb,
					PCI_DOMAIN_NUMBER_INVALID - 1);
				exit(-1);
			}
			break;
		case 'h':
			usage(stdout, argv);
			exit(0);
		case 's':
			ctx.pci_device_nb = strtol(optarg, &endptr, 10);
			if (errno != 0 || optarg == endptr) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			if (ctx.pci_device_nb >= PCI_DEVICE_NUMBER_INVALID) {
				LOG_ERR("PCI device number (%u) out of"
					"range ([0, %u])\n", ctx.pci_device_nb,
					PCI_DEVICE_NUMBER_INVALID - 1);
				exit(-1);
			}
			break;
		case 'l':
			ctx.vec_len = strtol(optarg, &endptr, 10);
			if (errno != 0 || optarg == endptr) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			break;
		default:
			usage(stderr, argv);
			exit(-1);
		}
	}

	if (ctx.pci_device_nb >= PCI_DEVICE_NUMBER_INVALID) {
		LOG_ERR("PCI device (slot) number must be provided."
			"Consider using lspci.\n");
		exit(-1);
	}

	if (ctx.pci_bus_nb >= PCI_BUS_NUMBER_INVALID) {
		LOG_ERR("PCI bus number must be provided."
			"Consider using lspci.\n");
		exit(-1);
	}

	if (ctx.pci_domain_nb >= PCI_DOMAIN_NUMBER_INVALID) {
		LOG_ERR("PCI domain number must be provided."
			"Consider using lspci.\n");
		exit(-1);
	}

	if (ctx.pci_func_nb >= PCI_FUNCTION_NUMBER_INVALID) {
		LOG_ERR("PCI function number must be provided."
			"Consider using lspci.\n");
		exit(-1);
	}


	return ctx;
}

struct context_multi parse_args_multi(int argc, char **argv)
{
	int op;
	char *endptr;
	struct context_multi ctx;
	int fds_pos = 0;
	
	ctx.fds_len = 0;
	ctx.fds = NULL;
	ctx.data_len = 0;
	ctx.data = NULL;

	while ((op = getopt(argc, argv, "n:d:l:h")) != -1) {
		switch (op) {
		case 'n':
			ctx.fds_len = strtol(optarg, &endptr, 10);
			if (errno != 0 || optarg == endptr) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			if (ctx.fds)
				free(ctx.fds);
			ctx.fds = calloc(ctx.fds_len, sizeof(int));
			break;
		case 'l':
			ctx.data_len = strtol(optarg, &endptr, 10);
			if (errno != 0 || optarg == endptr) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			if (ctx.data)
				free(ctx.data);
			ctx.data = calloc(ctx.data_len, sizeof(int));
			break;
		case 'd':
			if (!ctx.fds || !ctx.fds_len || (ctx.fds_len > 0
						&& fds_pos == ctx.fds_len))
				break;
			ctx.fds[fds_pos] = open(optarg, O_RDWR | O_SYNC);
			if (errno != 0 || ctx.fds[fds_pos] < 0) {
				LOG_ERR("strtol: invalid value (%s) for"
					"argument %c\n", optarg, op);
				exit(-1);
			}
			fds_pos++;
			break;
		case 'h':
			usage_multi(stdout, argv);
			exit(0);
		default:
			usage_multi(stderr, argv);
			exit(-1);
		}
	}

	if (!ctx.fds_len) {
		LOG_ERR("A number of devices must be provided.\n");
		exit(-1);
	}

	if (fds_pos != ctx.fds_len) {
		LOG_ERR("Missing devices. Consult /dev/psipe/ to "
				"find available ones.\n");
		exit(-1);
	}

	if (!ctx.data_len) {
		LOG_ERR("A positive number of integers must be provided.\n");
		exit(-1);
	}

	return ctx;
}

void free_context_multi(struct context_multi *ctx)
{
	for (int i = 0; i < ctx->fds_len; ++i)
		close(ctx->fds[i]);
}
