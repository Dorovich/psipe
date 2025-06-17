/* proxy.h - External access to Proto-SIPE memory
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#ifndef PSIPE_PROXY_H
#define PSIPE_PROXY_H

#include "qemu/osdep.h"
#include "qemu/typedefs.h"
#include <sys/socket.h>

#define PSIPE_PROXY_HOST "localhost"
#define PSIPE_PROXY_PORT 8987
#define PSIPE_PROXY_BUFF PAGE_SIZE
#define PSIPE_PROXY_MAXQ 1

#define PSIPE_REQ_NIL 0x0
#define PSIPE_REQ_ACK 0x1 /* general acknowledge */
#define PSIPE_REQ_SYN 0x2 /* start syncing page data */
#define PSIPE_REQ_RST 0x3 /* reset machine */
#define PSIPE_REQ_SLN 0x4 /* send your available length */
#define PSIPE_REQ_RLN 0x5 /* receive my available length */

/* Forward declaration */
typedef struct PSIPEDevice PSIPEDevice;

typedef unsigned int ProxyRequest;

typedef struct PSIPEProxyConn {
	int sockd;
	struct sockaddr_in addr;
} PSIPEProxyConn;

typedef struct PSIPEProxy {
	PSIPEProxyConn server;
	PSIPEProxyConn client;
	bool server_mode;
	uint16_t port;
} PSIPEProxy;

/* ============================================================================
 * Public
 * ============================================================================
 */

int psipe_proxy_rx_page(PSIPEDevice *dev, uint8_t *buff);
int psipe_proxy_tx_page(PSIPEDevice *dev, uint8_t *buff, int len);

bool psipe_proxy_get_mode(Object *obj, Error **errp);
void psipe_proxy_set_mode(Object *obj, bool mode, Error **errp);

int psipe_proxy_issue_req(PSIPEDevice *dev, ProxyRequest req);
int psipe_proxy_wait_and_handle_req(PSIPEDevice *dev);
int psipe_proxy_await_req(PSIPEDevice *dev, ProxyRequest req);

void psipe_proxy_reset(PSIPEDevice *dev);
void psipe_proxy_init(PSIPEDevice *dev, Error **errp);
void psipe_proxy_fini(PSIPEDevice *dev);

#endif /* PSIPE_PROXY_H */
