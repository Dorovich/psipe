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

#define PNVL_IOCTL_SEND _IOW(PNVL_IOCTL_MAGIC, 1, struct pnvl_data *)
#define PNVL_IOCTL_RECV _IOW(PNVL_IOCTL_MAGIC, 2, struct pnvl_data *)
#define PNVL_IOCTL_ASEND _IOW(PNVL_IOCTL_MAGIC, 3, struct pnvl_data *)
#define PNVL_IOCTL_ARECV _IOW(PNVL_IOCTL_MAGIC, 4, struct pnvl_data *)
#define PNVL_IOCTL_WAIT _IO(PNVL_IOCTL_MAGIC, 5)
#define PNVL_IOCTL_RETURN _IO(PNVL_IOCTL_MAGIC, 6)
