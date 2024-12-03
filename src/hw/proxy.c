/* proxy.c - External access to Proto-NVLink memory
 *
 * Author: David Cañadas López <dcanadas@bsc.es>
 *
 */

#include "proxy.h"

/* ============================================================================
 * Public
 * ============================================================================
 */

ProxyRequest pnvl_proxy_wait_req(PNVLDevice *dev)
{
	int ret, con;
	ProxyRequest req;
	fd_set cons;

	con = dev->proxy.sockd;
	req = PNVL_REQ_NONE;
	FD_ZERO(&cons);
	FD_SET(con, &cons);
	ret = select(con+1, &cons, NULL, NULL, NULL);
	if (ret && FD_ISSET(con, &cons))
		recv(con, &req, sizeof(req), 0);

	return req;
}

int pnvl_proxy_issue_req(PNVLDevice *dev, ProxyRequest req)
{
	int ret;

	ret = send(dev->proxy.sockd, &req, sizeof(req), 0);
	if (ret < 0)
		return PNVL_FAILURE;

	return PNVL_SUCCESS
}

int pnvl_proxy_handle_req(PNVLDevice *dev, ProxyRequest req)
{
	switch(req) {
	case PNVL_REQ_SYN:
		return pnvl_transfer_pages(dev);
	case PNVL_REQ_RST:
		qmp_system_reset(NULL); /* see qemu/ui/gtk.c L1313 */
		break;
	case PNVL_REQ_NIL:
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
int pnvl_proxy_rx_page(PNVLDevice *dev, uint8_t *buffer, size_t *len)
{
	int ret, con;

	con = dev->proxy.sockd;

	ret = recv(con, len, sizeof(*len), 0);
	if (ret < 0)
		return PNVL_FAILURE;

	ret = recv(con, buffer, *len, 0);
	if (ret < 0)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
}

/*
 * Transmit page: buffer --> socket
 */
int pnvl_proxy_tx_page(PNVLDevice *dev, uint8_t *buffer, size_t len)
{
	int ret, con;

	con = dev->proxy.sockd;

	ret = send(con, &len, sizeof(len), 0);
	if (ret < 0)
		return PNVL_FAILURE;

	ret = send(con, buffer, len, 0);
	if (ret < 0)
		return PNVL_FAILURE;

	return PNVL_SUCCESS;
}
