/* pnvl_ioctl.h - Definitions for IOCTL
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

struct pnvl_data {
	unsigned long addr;
	unsigned long len;
};

typedef unsigned long pnvl_handle_t;

#define PNVL_IOCTL_MAGIC 0xe1

#define PNVL_IOCTL_SEND _IOW(PNVL_IOCTL_MAGIC, 1, struct pnvl_data *)
#define PNVL_IOCTL_RECV _IOW(PNVL_IOCTL_MAGIC, 2, struct pnvl_data *)
#define PNVL_IOCTL_WAIT _IOW(PNVL_IOCTL_MAGIC, 3, pnvl_handle_t)
