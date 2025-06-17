#include "sw/module/psipe_ioctl.h"

#define WAIT_ALL_OPS 1

// d = dev num, n = total elems, t = total devs
#define PART_FOR_DEV(d, n, t) ((n / t) + (d < (n % t)))

struct psipe_devices {
	int num;
	int *fds;
};

int psipe_fd(int id);
int psipe_num_devs(void);
int psipe_open_devs(void);
int psipe_close_devs(void);
int psipe_wait(int fd, psipe_handle_t id);
int psipe_flush(int fd);

// these return a handle if return value is non-negative
int psipe_send(int fd, void *addr, size_t len);
int psipe_recv(int fd, void *addr, size_t len);
int psipe_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs);
int psipe_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs);
