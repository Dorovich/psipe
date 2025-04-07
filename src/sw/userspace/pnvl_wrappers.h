// d = dev id, n = total, s = num devs
#define PART_FOR_DEV(d, n, s) ((n / s) + (d < (n % s)))

struct _pnvl_devices {
	int num;
	int *fds;
};

int _pnvl_fd(int id);
int _pnvl_num_devs(void);
int _pnvl_open_devs(void);
int _pnvl_close_devs(void);
int _pnvl_send(int fd, void *addr, size_t len);
int _pnvl_recv(int fd, void *addr, size_t len);
int _pnvl_barrier(int fd);
int _pnvl_send_args(int fd, int sz_n, int sz_t, int sz_m, int len, int ofs);
int _pnvl_recv_args(int fd, int *sz_n, int *sz_t, int *sz_m, int *len, int *ofs);
