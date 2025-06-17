/* psipe_ioctl.h - Definitions for IOCTL
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

struct psipe_data {
	unsigned long addr;
	unsigned long len;
};

typedef unsigned long psipe_handle_t;

#define PSIPE_IOCTL_MAGIC 0xe1

#define PSIPE_IOCTL_SEND _IOW(PSIPE_IOCTL_MAGIC, 1, struct psipe_data *)
#define PSIPE_IOCTL_RECV _IOW(PSIPE_IOCTL_MAGIC, 2, struct psipe_data *)
#define PSIPE_IOCTL_WAIT _IOW(PSIPE_IOCTL_MAGIC, 3, psipe_handle_t)
#define PSIPE_IOCTL_FLUSH _IO(PSIPE_IOCTL_MAGIC, 4)
