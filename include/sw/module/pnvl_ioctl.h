/* pnvl_ioctl.h - Definitions for IOCTL
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

struct pnvl_data {
	unsigned long in_addr;
	unsigned long in_len;
	unsigned long out_addr;
	unsigned long out_len;
};

#define PNVL_IOCTL_MAGIC 0xe1

#define PNVL_IOCTL_WORK _IOW(PNVL_IOCTL_MAGIC, 1, struct pnvl_data *)
#define PNVL_IOCTL_WAIT _IOR(PNVL_IOCTL_MAGIC, 2, size_t)
#define PNVL_IOCTL_WATCH _IOR(PNVL_IOCTL_MAGIC, 3, struct pnvl_data *)
