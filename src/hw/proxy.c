/* proxy.c - External access to Proto-SIPE memory
 *
 * Copyright (c) 2025 David Cañadas López <david.canadas@estudiantat.upc.edu>
 *
 * SPDX-Liscense-Identifier: GPL-2.0
 *
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "proxy.h"
#include "psipe.h"
#include "qapi/qapi-commands-machine.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

static void psipe_proxy_init_server(PSIPEDevice *dev)
{
	PSIPEProxy *proxy = &dev->proxy;
	socklen_t len = sizeof(proxy->client.addr);

	if (bind(proxy->server.sockd, (struct sockaddr *)&proxy->server.addr,
				sizeof(proxy->server.addr)) < 0) {
		perror("bind");
		return;
	}

	if (listen(proxy->server.sockd, PSIPE_PROXY_MAXQ) < 0) {
		perror("listen");
		return;
	}

	puts("Server started, waiting for client...");

	proxy->client.sockd = accept(proxy->server.sockd,
			(struct sockaddr *)&proxy->client.addr, &len);
	if (proxy->client.sockd < 0) {
		perror("accept");
		return;
	}

	/* Begin connection test */
	if (psipe_proxy_issue_req(dev, PSIPE_REQ_ACK) != PSIPE_SUCCESS) {
		perror("psipe_proxy_issue_req");
		return;
	}
	if (psipe_proxy_await_req(dev, PSIPE_REQ_ACK) != PSIPE_SUCCESS) {
		perror("psipe_proxy_await_req");
		return;
	}
	puts("Client connection established.");
	/* End connection test */
}

static void psipe_proxy_init_client(PSIPEDevice *dev)
{
	PSIPEProxy *proxy = &dev->proxy;

	if (connect(proxy->server.sockd, (struct sockaddr *)&proxy->server.addr,
				sizeof(proxy->server.addr)) < 0) {
		perror("connect");
		return;
	}

	/* Begin connection test */
	if (psipe_proxy_await_req(dev, PSIPE_REQ_ACK) != PSIPE_SUCCESS) {
		perror("psipe_proxy_await_req");
		return;
	}
	if (psipe_proxy_issue_req(dev, PSIPE_REQ_ACK) != PSIPE_SUCCESS) {
		perror("psipe_proxy_issue_req");
		return;
	}
	puts("Server connection established.");
	/* End connection test */
}

static inline int psipe_proxy_endpoint(PSIPEDevice *dev)
{
	return (dev->proxy.server_mode ?
			dev->proxy.client.sockd : dev->proxy.server.sockd);
}

static ProxyRequest psipe_proxy_wait_req(PSIPEDevice *dev)
{
	int con = psipe_proxy_endpoint(dev);
	ProxyRequest req = PSIPE_REQ_NIL;
	fd_set cons;

	FD_ZERO(&cons);
	FD_SET(con, &cons);
	if (select(con+1, &cons, NULL, NULL, NULL) && FD_ISSET(con, &cons))
		recv(con, &req, sizeof(req), 0);

	return req;
}

static int psipe_proxy_handle_req(PSIPEDevice *dev, ProxyRequest req)
{
	int con = psipe_proxy_endpoint(dev);

	switch(req) {
	case PSIPE_REQ_SYN:
		psipe_execute(dev);
		break;
	case PSIPE_REQ_RST:
		qmp_system_reset(NULL); /* see qemu/ui/gtk.c L1313 */
		break;
	case PSIPE_REQ_SLN:
		psipe_proxy_issue_req(dev, PSIPE_REQ_RLN);
		send(con, &dev->dma.config.len_avail,
				sizeof(dev->dma.config.len_avail), 0);
		break;
	case PSIPE_REQ_RLN:
		recv(con, &dev->dma.config.len_avail,
				sizeof(dev->dma.config.len_avail), 0);
		break;
	case PSIPE_REQ_ACK:
		break;
	default:
		return PSIPE_FAILURE;
	}

	return PSIPE_SUCCESS;
}

/* ============================================================================
 * Public
 * ============================================================================
 */

int psipe_proxy_issue_req(PSIPEDevice *dev, ProxyRequest req)
{
	int ret, con = psipe_proxy_endpoint(dev);

	ret = send(con, &req, sizeof(req), 0);
	if (ret < 0)
		return PSIPE_FAILURE;

	return PSIPE_SUCCESS;
}

int psipe_proxy_wait_and_handle_req(PSIPEDevice *dev)
{
	return psipe_proxy_handle_req(dev, psipe_proxy_wait_req(dev));
}

int psipe_proxy_await_req(PSIPEDevice *dev, ProxyRequest req)
{
	ProxyRequest new_req;

	do {
		new_req = psipe_proxy_wait_req(dev);
	} while(new_req != PSIPE_FAILURE && new_req != req);

	return psipe_proxy_handle_req(dev, new_req);
}

/*
 * Receive page: buffer <-- socket
 */
int psipe_proxy_rx_page(PSIPEDevice *dev, uint8_t *buff)
{
	int src = psipe_proxy_endpoint(dev);
	int len = 0;

	if (recv(src, &len, sizeof(len), 0) < 0 || !len)
		return PSIPE_FAILURE;

	if (recv(src, buff, len, 0) < 0)
		return PSIPE_FAILURE;

	return len;
}

/*
 * Transmit page: buffer --> socket
 */
int psipe_proxy_tx_page(PSIPEDevice *dev, uint8_t *buff, int len)
{
	int dst = psipe_proxy_endpoint(dev);

	if (len <= 0)
		return PSIPE_FAILURE;

	if (send(dst, &len, sizeof(len), 0) < 0)
		return PSIPE_FAILURE;

	if (send(dst, buff, len, 0) < 0)
		return PSIPE_FAILURE;

	return PSIPE_SUCCESS;
}

bool psipe_proxy_get_mode(Object *obj, Error **errp)
{
	PSIPEDevice *dev = PSIPE(obj);
	return dev->proxy.server_mode;
}

void psipe_proxy_set_mode(Object *obj, bool mode, Error **errp)
{
	PSIPEDevice *dev = PSIPE(obj);
	dev->proxy.server_mode = mode;
}

void psipe_proxy_reset(PSIPEDevice *dev)
{
	return;
}

void psipe_proxy_init(PSIPEDevice *dev, Error **errp)
{
	PSIPEProxy *proxy = &dev->proxy;
	struct hostent *h;

	h = gethostbyname(PSIPE_PROXY_HOST);
	if (!h) {
		herror("gethostbyname");
		return;
	}

	proxy->server.sockd = socket(AF_INET, SOCK_STREAM, 0);
	if (proxy->server.sockd < 0) {
		perror("socket");
		return;
	}

	bzero(&proxy->server.addr, sizeof(proxy->server.addr));
	proxy->server.addr.sin_family = AF_INET;
	proxy->server.addr.sin_port = htons(proxy->port);
	proxy->server.addr.sin_addr.s_addr = *(in_addr_t *)h->h_addr_list[0];

	if (proxy->server_mode)
		psipe_proxy_init_server(dev);
	else
		psipe_proxy_init_client(dev);
}

void psipe_proxy_fini(PSIPEDevice *dev)
{
	if (dev->proxy.server_mode)
		close(dev->proxy.client.sockd);
	close(dev->proxy.server.sockd);
}
