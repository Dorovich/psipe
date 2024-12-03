/* proxy.h - External access to Proto-NVLink memory
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#pragma once

#include "qapi/qmp/bool.h"
#include "qemu/typedefs.h"
#include <sys/socket.h>

#define PNVL_PROXY_HOST "localhost"
#define PNVL_PROXY_PORT 8987
#define PNVL_PROXY_BUFF PAGE_SIZE

#define PNVL_REQ_NIL 0x0
#define PNVL_REQ_ACK 0x1
#define PNVL_REQ_SYN 0x2
#define PNVL_REQ_RST 0x3

/* Forward declaration */
typedef struct PNVLDevice PNVLDevice;

typedef unsigned int ProxyRequest;

typedef struct PNVLProxy {
	int sockd;
	bool server_mode;
	uint16_t port;
	struct sockaddr_in addr;
} PNVLProxy;

/* ============================================================================
 * Public
 * ============================================================================
 */

ProxyRequest pnvl_proxy_wait_req(PNVLDevice *dev);

int pnvl_proxy_issue_req(PNVLDevice *dev, ProxyRequest req);
int pnvl_proxy_handle_req(PNVLDevice *dev, ProxyRequest req);

int pnvl_proxy_rx_page(PNVLDevice *dev, uint8_t *buffer, size_t *len);
int pnvl_proxy_tx_page(PNVLDevice *dev, uint8_t *buffer, size_t len);

void pnvl_proxy_reset(PNVLDevice *dev);
void pnvl_proxy_init(PNVLDevice *dev, Error **errp);
void pnvl_proxy_fini(PNVLDevice *dev);
