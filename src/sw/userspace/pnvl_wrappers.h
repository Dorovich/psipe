#include "sw/module/pnvl_ioctl.h"

#define WAIT_ALL_OPS 1

// d = dev num, n = total elems, t = total devs
#define PART_FOR_DEV(d, n, t) ((n / t) + (d < (n % t)))

struct _pnvl_devices {
	int num;
	int *fds;
};

int _pnvl_fd(int id);
int _pnvl_num_devs(void);
int _pnvl_open_devs(void);
int _pnvl_close_devs(void);
int _pnvl_wait(int fd, pnvl_handle_t id);

// these return a handle if return value is non-negative
int _pnvl_send(int fd, void *addr, size_t len);
int _pnvl_recv(int fd, void *addr, size_t len);
int _pnvl_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs);
int _pnvl_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs);
