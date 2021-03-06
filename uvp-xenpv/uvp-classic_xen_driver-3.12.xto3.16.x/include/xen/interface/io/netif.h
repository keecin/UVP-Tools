/******************************************************************************
 * netif.h
 *
 * Unified network-device I/O interface for Xen guest OSes.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2003-2004, Keir Fraser
 */

#ifndef __XEN_PUBLIC_IO_NETIF_H__
#define __XEN_PUBLIC_IO_NETIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 * Older implementation of Xen network frontend / backend has an
 * implicit dependency on the MAX_SKB_FRAGS as the maximum number of
 * ring slots a skb can use. Netfront / netback may not work as
 * expected when frontend and backend have different MAX_SKB_FRAGS.
 *
 * A better approach is to add mechanism for netfront / netback to
 * negotiate this value. However we cannot fix all possible
 * frontends, so we need to define a value which states the minimum
 * slots backend must support.
 *
 * The minimum value derives from older Linux kernel's MAX_SKB_FRAGS
 * (18), which is proved to work with most frontends. Any new backend
 * which doesn't negotiate with frontend should expect frontend to
 * send a valid packet using slots up to this value.
 */
#define XEN_NETIF_NR_SLOTS_MIN 18

/*
 * Notifications after enqueuing any type of message should be conditional on
 * the appropriate req_event or rsp_event field in the shared ring.
 * If the client sends notification for rx requests then it should specify
 * feature 'feature-rx-notify' via xenbus. Otherwise the backend will assume
 * that it cannot safely queue packets (as it may not be kicked to send them).
 */

/*
 * "feature-split-event-channels" is introduced to separate guest TX
 * and RX notification. Backend either doesn't support this feature or
 * advertises it via xenstore as 0 (disabled) or 1 (enabled).
 *
 * To make use of this feature, frontend should allocate two event
 * channels for TX and RX, advertise them to backend as
 * "event-channel-tx" and "event-channel-rx" respectively. If frontend
 * doesn't want to use this feature, it just writes "event-channel"
 * node as before.
 */

/*
 * "feature-no-csum-offload" should be used to turn IPv4 TCP/UDP checksum
 * offload off or on. If it is missing then the feature is assumed to be on.
 * "feature-ipv6-csum-offload" should be used to turn IPv6 TCP/UDP checksum
 * offload on or off. If it is missing then the feature is assumed to be off.
 */

/*
 * "feature-gso-tcpv4" and "feature-gso-tcpv6" advertise the capability to
 * handle large TCP packets (in IPv4 or IPv6 form respectively). Neither
 * frontends nor backends are assumed to be capable unless the flags are
 * present.
 */

/*
 * This is the 'wire' format for packets:
 *  Request 1: xen_netif_tx_request  -- XEN_NETTXF_* (any flags)
 * [Request 2: xen_netif_extra_info]    (only if request 1 has XEN_NETTXF_extra_info)
 * [Request 3: xen_netif_extra_info]    (only if request 2 has XEN_NETIF_EXTRA_MORE)
 *  Request 4: xen_netif_tx_request  -- XEN_NETTXF_more_data
 *  Request 5: xen_netif_tx_request  -- XEN_NETTXF_more_data
 *  ...
 *  Request N: xen_netif_tx_request  -- 0
 */

/* Protocol checksum field is blank in the packet (hardware offload)? */
#define _XEN_NETTXF_csum_blank		(0)
#define  XEN_NETTXF_csum_blank		(1U<<_XEN_NETTXF_csum_blank)

/* Packet data has been validated against protocol checksum. */
#define _XEN_NETTXF_data_validated	(1)
#define  XEN_NETTXF_data_validated	(1U<<_XEN_NETTXF_data_validated)

/* Packet continues in the next request descriptor. */
#define _XEN_NETTXF_more_data		(2)
#define  XEN_NETTXF_more_data		(1U<<_XEN_NETTXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _XEN_NETTXF_extra_info		(3)
#define  XEN_NETTXF_extra_info		(1U<<_XEN_NETTXF_extra_info)

#define XEN_NETIF_MAX_TX_SIZE 0xFFFF
struct netif_tx_request {
    grant_ref_t gref;      /* Reference to buffer page */
    uint16_t offset;       /* Offset within buffer page */
    uint16_t flags;        /* XEN_NETTXF_* */
    uint16_t id;           /* Echoed in response message. */
    uint16_t size;         /* Packet size in bytes.       */
};
typedef struct netif_tx_request netif_tx_request_t;

/* Types of netif_extra_info descriptors. */
#define XEN_NETIF_EXTRA_TYPE_NONE	(0)  /* Never used - invalid */
#define XEN_NETIF_EXTRA_TYPE_GSO	(1)  /* u.gso */
#define XEN_NETIF_EXTRA_TYPE_MCAST_ADD	(2)  /* u.mcast */
#define XEN_NETIF_EXTRA_TYPE_MCAST_DEL	(3)  /* u.mcast */
#define XEN_NETIF_EXTRA_TYPE_MAX	(4)

/* xen_netif_extra_info flags. */
#define _XEN_NETIF_EXTRA_FLAG_MORE	(0)
#define  XEN_NETIF_EXTRA_FLAG_MORE	(1U<<_XEN_NETIF_EXTRA_FLAG_MORE)

/* GSO types */
#define XEN_NETIF_GSO_TYPE_NONE		(0)
#define XEN_NETIF_GSO_TYPE_TCPV4	(1)
#define XEN_NETIF_GSO_TYPE_TCPV6	(2)

/*
 * This structure needs to fit within both netif_tx_request and
 * netif_rx_response for compatibility.
 */
struct netif_extra_info {
	uint8_t type;  /* XEN_NETIF_EXTRA_TYPE_* */
	uint8_t flags; /* XEN_NETIF_EXTRA_FLAG_* */

	union {
		/*
		 * XEN_NETIF_EXTRA_TYPE_GSO:
		 */
		struct {
			/*
			 * Maximum payload size of each segment. For
			 * example, for TCP this is just the path MSS.
			 */
			uint16_t size;

			/*
			 * GSO type. This determines the protocol of
			 * the packet and any extra features required
			 * to segment the packet properly.
			 */
			uint8_t type; /* XEN_NETIF_GSO_TYPE_* */

			/* Future expansion. */
			uint8_t pad;

			/*
			 * GSO features. This specifies any extra GSO
			 * features required to process this packet,
			 * such as ECN support for TCPv4.
			 */
			uint16_t features; /* XEN_NETIF_GSO_FEAT_* */
		} gso;

		/*
		 * XEN_NETIF_EXTRA_TYPE_MCAST_{ADD,DEL}:
		 * Backend advertises availability via
		 * 'feature-multicast-control' xenbus node containing value
		 * '1'.
		 * Frontend requests this feature by advertising
		 * 'request-multicast-control' xenbus node containing value
		 * '1'. If multicast control is requested then multicast
		 * flooding is disabled and the frontend must explicitly
		 * register its interest in multicast groups using dummy
		 * transmit requests containing MCAST_{ADD,DEL} extra-info
		 * fragments.
		 */
		struct {
			uint8_t addr[6]; /* Address to add/remove. */
		} mcast;

		uint16_t pad[3];
	} u;
};
typedef struct netif_extra_info netif_extra_info_t;

struct netif_tx_response {
	uint16_t id;
	int16_t  status;       /* XEN_NETIF_RSP_* */
};
typedef struct netif_tx_response netif_tx_response_t;

struct netif_rx_request {
	uint16_t    id;        /* Echoed in response message.        */
	grant_ref_t gref;      /* Reference to incoming granted frame */
};
typedef struct netif_rx_request netif_rx_request_t;

/* Packet data has been validated against protocol checksum. */
#define _XEN_NETRXF_data_validated	(0)
#define  XEN_NETRXF_data_validated	(1U<<_XEN_NETRXF_data_validated)

/* Protocol checksum field is blank in the packet (hardware offload)? */
#define _XEN_NETRXF_csum_blank		(1)
#define  XEN_NETRXF_csum_blank		(1U<<_XEN_NETRXF_csum_blank)

/* Packet continues in the next request descriptor. */
#define _XEN_NETRXF_more_data		(2)
#define  XEN_NETRXF_more_data		(1U<<_XEN_NETRXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _XEN_NETRXF_extra_info		(3)
#define  XEN_NETRXF_extra_info		(1U<<_XEN_NETRXF_extra_info)

/* GSO Prefix descriptor. */
#define _XEN_NETRXF_gso_prefix		(4)
#define  XEN_NETRXF_gso_prefix		(1U<<_XEN_NETRXF_gso_prefix)

struct netif_rx_response {
    uint16_t id;
    uint16_t offset;       /* Offset in page of start of received packet  */
    uint16_t flags;        /* XEN_NETRXF_* */
    int16_t  status;       /* -ve: NETIF_RSP_* ; +ve: Rx'ed pkt size. */
};
typedef struct netif_rx_response netif_rx_response_t;

/*
 * Generate netif ring structures and types.
 */

#if defined(CONFIG_XEN) || defined(HAVE_XEN_PLATFORM_COMPAT_H)
DEFINE_RING_TYPES(netif_tx, struct netif_tx_request, struct netif_tx_response);
DEFINE_RING_TYPES(netif_rx, struct netif_rx_request, struct netif_rx_response);
#else
#define xen_netif_tx_request netif_tx_request
#define xen_netif_rx_request netif_rx_request
#define xen_netif_tx_response netif_tx_response
#define xen_netif_rx_response netif_rx_response
DEFINE_RING_TYPES(xen_netif_tx,
		  struct xen_netif_tx_request,
		  struct xen_netif_tx_response);
DEFINE_RING_TYPES(xen_netif_rx,
		  struct xen_netif_rx_request,
		  struct xen_netif_rx_response);
#define xen_netif_extra_info netif_extra_info
#endif

#define XEN_NETIF_RSP_DROPPED	-2
#define XEN_NETIF_RSP_ERROR	-1
#define XEN_NETIF_RSP_OKAY	 0
/* No response: used for auxiliary requests (e.g., netif_tx_extra). */
#define XEN_NETIF_RSP_NULL	 1

#endif
