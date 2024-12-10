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
#define PNVL_PROXY_MAXQ 1

#define PNVL_REQ_NIL 0x0
#define PNVL_REQ_ACK 0x1 /* any acknowledge */
#define PNVL_REQ_SYN 0x2 /* sync page data */
#define PNVL_REQ_RST 0x3 /* reset machine */
#define PNVL_REQ_ALN 0x4 /* sync available length */

/* Forward declaration */
typedef struct PNVLDevice PNVLDevice;

typedef unsigned int ProxyRequest;

typedef struct PNVLProxyConn {
	int sockd;
	struct sockaddr_in addr;
} PNVLProxyConn;

typedef struct PNVLProxy {
	PNVLProxyConn server;
	PNVLProxyConn client;
	bool server_mode;
	uint16_t port;
} PNVLProxy;

/* ============================================================================
 * Public
 * ============================================================================
 */

size_t pnvl_proxy_rx_page(PNVLDevice *dev, uint8_t *buff);
int pnvl_proxy_tx_page(PNVLDevice *dev, uint8_t *buff, size_t len);

int pnvl_proxy_issue_req(PNVLDevice *dev, ProxyRequest req);
int pnvl_proxy_handle_req(PNVLDevice *dev, ProxyRequest req);
ProxyRequest pnvl_proxy_wait_req(PNVLDevice *dev);

void pnvl_proxy_reset(PNVLDevice *dev);
void pnvl_proxy_init(PNVLDevice *dev, Error **errp);
void pnvl_proxy_fini(PNVLDevice *dev);
