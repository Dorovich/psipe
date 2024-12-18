/* proxy.c - External access to Proto-NVLink memory
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "proxy.h"

/* ============================================================================
 * Private
 * ============================================================================
 */

void pnvl_proxy_init_server(PNVLDevice *dev)
{
	int ret;
	PNVLProxy *proxy = &dev->proxy;
	socklen_t len; /* not used again */

	ret = bind(proxy->server.sockd, (struct sockaddr *)&proxy->server.addr,
		sizeof(proxy->server.addr));
	if (ret < 0) {
		perror("bind");
		return;
	}

	ret = listen(proxy->server.sockd, PNVL_PROXY_MAXQ);
	if (ret < 0) {
		perror("listen");
		return;
	}

	len = sizeof(proxy->client.addr);
	con = accept(proxy->client.sockd,
		(struct sockaddr *)&proxy->client.addr, &len);
	if (ret < 0) {
		perror("accept");
		return;
	}

	pnvl_proxy_issue_req(dev, PNVL_REQ_ACK);

	close(proxy->client.sockd);
	close(proxy->server.sockd);
}

void pnvl_proxy_init_client(PNVLDevice *dev)
{
	int ret;
	PNVLProxy *proxy = &dev->proxy;

	ret = connect(proxy->server.sockd,
		(struct sockaddr *)&proxy->server.addr, sizeof(proxy->addr));
	if (ret < 0) {
		perror("connect");
		return;
	}

	ProxyRequest req = pnvl_proxy_wait_req(dev);
	pnvl_proxy_handle_req(dev, req);

	close(proxy->server.sockd);
}

/* ============================================================================
 * Public
 * ============================================================================
 */

ProxyRequest pnvl_proxy_wait_req(PNVLDevice *dev)
{
	int ret, con;
	ProxyRequest req = PNVL_REQ_NONE;
	fd_set cons;

	if (dev->proxy.server_mode)
		con = dev->proxy.client.sockd;
	else
		con = dev->proxy.server.sockd;

	FD_ZERO(&cons);
	FD_SET(con, &cons);
	ret = select(con+1, &cons, NULL, NULL, NULL);
	if (ret && FD_ISSET(con, &cons))
		recv(con, &req, sizeof(req), 0);

	return req;
}

int pnvl_proxy_issue_req(PNVLDevice *dev, ProxyRequest req)
{
	int ret, con;

	if (dev->proxy.server_mode)
		con = dev->proxy.client.sockd;
	else
		con = dev->proxy.server.sockd;

	ret = send(con, &req, sizeof(req), 0);
	if (ret < 0)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
}

int pnvl_proxy_handle_req(PNVLDevice *dev, ProxyRequest req)
{
	switch(req) {
	case PNVL_REQ_SYN:
		return pnvl_transfer_pages(dev);
	case PNVL_REQ_RST:
		qmp_system_reset(NULL); /* see qemu/ui/gtk.c L1313 */
		break;
	case PNVL_REQ_ALN:
		return recv(dev->proxy.sockd, &dev->dma.config.len_avail,
			sizeof(dev->dma.config.len_avail), 0);
	case PNVL_REQ_ACK:
		break;
	default:
		return PNVL_FAILURE;
	}

	return PNVL_SUCCESS;
}

/*
 * Receive page: buffer <-- socket
 */
size_t pnvl_proxy_rx_page(PNVLDevice *dev, uint8_t *buff)
{
	int ret, src;
	size_t len = 0;
	PNVLProxy *proxy = &dev->proxy;

	src = proxy->server_mode ? proxy->client.sockd : proxy->server.sockd;

	ret = recv(src, &len, sizeof(len), 0);
	if (ret < 0 || !len)
		return PNVL_FAILURE;

	ret = recv(src, buff, len, 0);
	if (ret < 0)
		return PNVL_FAILURE;

	return len;
}

/*
 * Transmit page: buffer --> socket
 */
int pnvl_proxy_tx_page(PNVLDevice *dev, uint8_t *buff, size_t len)
{
	int ret, dst;
	PNVLProxy *proxy = &dev->proxy;

	if (len <= 0)
		return PNVL_FAILURE;

	dst = proxy->server_mode ? proxy->client.sockd : proxy->server.sockd;

	ret = send(dst, &len, sizeof(len), 0);
	if (ret < 0)
		return PNVL_FAILURE;

	ret = send(dst, buff, len, 0);
	if (ret < 0)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
}

void pnvl_proxy_reset(PNVLDevice *dev)
{
	return;
}

void pnvl_proxy_init(PNVLDevice *dev, Error **errp)
{
	PNVLProxy *proxy = &dev->proxy;
	struct hostent *h; /* not used again */

	h = gethostbyname(PNVL_PROXY_HOST);
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
	proxy->server.addr.sin_addr = *(in_addr_t *)h->h_addr_list[0];

	if (proxy->server_mode)
		pnvl_proxy_init_server(dev);
	else
		pnvl_proxy_init_client(dev);
}

void pnvl_proxy_fini(PNVLDevice *dev)
{
	return;
}
