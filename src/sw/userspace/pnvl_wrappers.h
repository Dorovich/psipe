#include "sw/module/pnvl_ioctl.h"

#define WAIT_ALL_OPS 0

// d = dev num, n = total elems, t = total devs
#define PART_FOR_DEV(d, n, t) ((n / t) + (d < (n % t)))

struct pnvl_devices {
	int num;
	int *fds;
};

int pnvl_fd(int id);
int pnvl_num_devs(void);
int pnvl_open_devs(void);
int pnvl_close_devs(void);
int pnvl_wait(int fd, pnvl_handle_t id);
int pnvl_flush(int fd);

// these return a handle if return value is non-negative
int pnvl_send(int fd, void *addr, size_t len);
int pnvl_recv(int fd, void *addr, size_t len);
int pnvl_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs);
int pnvl_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs);
