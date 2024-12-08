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

#define PNVL_IOCTL_MAGIC 0xe1

#define PNVL_IOCTL_WORK _IOW(PNVL_IOCTL_MAGIC, 1, struct pnvl_data *)
#define PNVL_IOCTL_WAIT _IOR(PNVL_IOCTL_MAGIC, 2, struct pnvl_data *)
