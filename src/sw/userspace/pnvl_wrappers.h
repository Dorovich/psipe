// d = dev id, n = total, s = num devs
#define PART_FOR_DEV(d, n, s) ((n / s) + (d < (n % s)))

struct _pnvl_devices {
	int num;
	int *fds;
};

int _pnvl_count_devs(void);
void _pnvl_arecv(int fd, void *addr, size_t len);
void _pnvl_asend(int fd, void *addr, size_t len);
void _pnvl_close_devs(void);
void _pnvl_open_devs(void);
void _pnvl_recv(int fd, void *addr, size_t len);
void _pnvl_recv_matmul_params(int fd, int *sz_n, int *sz_t, int *sz_m, int *g_len, int *g_ofs);
void _pnvl_return(int fd);
void _pnvl_send_all(void *addr, size_t len);
void _pnvl_send(int fd, void *addr, size_t len);
void _pnvl_send_matmul_params_all(int sz_n, int sz_t, int sz_m);
void _pnvl_send_matmul_params(int fd, int sz_n, int sz_t, int sz_m);
void _pnvl_wait(int fd);
