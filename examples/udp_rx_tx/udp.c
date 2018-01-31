/*
 * Copyright (C) 2016 Autonomous Networks Research Group
 *                     University of Southern California
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Simple program to test channel statistics using UDP packets
 *
 * @author      Jason A. Tran <jasontra@usc.edu>
 *              Pedro Henrique Gomes <pdasilva@usc.edu>
 *
 * @}
 */

#include <stdio.h>
#include <inttypes.h>

#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/pktdump.h"
#include "timex.h"
#include "utlist.h"
#include "xtimer.h"

#include "thread.h"
#include "msg.h"

#define MSG_QUEUE_SIZE 8
#define CC2538_RSSI_OFFSET 73

void print_rss(msg_t *msg);
void print_prr(uint32_t pkt_rcv, uint32_t num_pkts);

static gnrc_netreg_entry_t receiver = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                               KERNEL_PID_UNDEF);

/* this is used to unregister the thread from receiving UDP packets sent to the 
   port in the "receiver" struct. thread registry is done using a linked list. */
static void _unregister_thread(void)
{
    gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &receiver);
    receiver.target.pid = KERNEL_PID_UNDEF;
}

int udp_tx(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: %s <num_pkts> (0 = infinite) <interval_in_us> [<ipv6_addr> <port>]\n", 
            argv[0]);
        return 1;
    }

    if (atoi(argv[1]) < 0 || atoi(argv[2]) <= 0)
    {
        printf("error: please input valid values");
        return 1;
    }

    int iface;
    unsigned int num_pkts = atoi(argv[1]);
    uint32_t interval = (uint32_t)atoi(argv[2]);
    ipv6_addr_t addr;
    uint16_t port;

    char *addr_str = argv[3];
    char *port_str = argv[4];

    /* get interface, if available */
    iface = ipv6_addr_split_iface(addr_str);
    if ((iface < 0) && (gnrc_netif_numof() == 1)) {
        iface = gnrc_netif_iter(NULL)->pid;
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, addr_str) == NULL) {
        puts("Error: unable to parse destination address");
        return 1;
    }
    /* parse port */
    port = (uint16_t)atoi(port_str);
    if (port == 0) {
        puts("Error: unable to parse destination port");
        return 1;
    }

    char data_to_snd[15] = "Go Trojans!!!!";

    uint8_t pkt_num = 1;
    for (unsigned int i = 0; i < num_pkts || num_pkts == 0; i++) {
        gnrc_pktsnip_t *payload, *udp, *ip;
        unsigned payload_size;

        /* allocate payload */
        payload = gnrc_pktbuf_add(NULL, data_to_snd, strlen(data_to_snd), GNRC_NETTYPE_UNDEF);
        if (payload == NULL) {
            puts("Error: unable to copy data to packet buffer");
            return 1;
        }

        /* store size for output */
        payload_size = (unsigned)payload->size;

        /* allocate UDP header, set source port := destination port */
        udp = gnrc_udp_hdr_build(payload, port, port);
        if (udp == NULL) {
            puts("Error: unable to allocate UDP header");
            gnrc_pktbuf_release(payload);
            return 1;
        }

        /* allocate IPv6 header */
        ip = gnrc_ipv6_hdr_build(udp, NULL, &addr);
        if (ip == NULL) {
            puts("Error: unable to allocate IPv6 header");
            gnrc_pktbuf_release(udp);
            return 1;
        }

        /* add netif header, if interface was given */
        if (iface > 0) {
            gnrc_pktsnip_t *netif = gnrc_netif_hdr_build(NULL, 0, NULL, 0);

            ((gnrc_netif_hdr_t *)netif->data)->if_pid = (kernel_pid_t)iface;
            LL_PREPEND(ip, netif);
        }

        /* send packet */
        if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, ip)) {
            puts("Error: unable to locate UDP thread");
            gnrc_pktbuf_release(ip);
            return 1;
        }

        printf("Success: sent %u bytes to [%s]:%u\n", payload_size, addr_str, port);

        pkt_num++;

        xtimer_usleep(interval);
    }

    printf("Done!\n");

    return 0; 
}

int udp_rx(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: %s <num_pkts> <interval_in_us> <port>\n", argv[0]);
        return 1;
    }

    if (atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0 || atoi(argv[3]) <= 0)
    {
        printf("error: please input values greater than 0");
        return 1;
    }

    uint32_t num_pkts = atoi(argv[1]);
    uint32_t interval = atoi(argv[2]);
    uint32_t port = atoi(argv[3]);

    msg_t msg; 
    msg_t msg_queue[MSG_QUEUE_SIZE];

    /* setup the message queue */
    msg_init_queue(msg_queue, MSG_QUEUE_SIZE);

    /* register this thread to the chosen UDP port */
    receiver.demux_ctx = port; 
    receiver.target.pid = thread_getpid();
    gnrc_netreg_register(GNRC_NETTYPE_UDP, &receiver);
    
    printf("Waiting for first packet...\n");

    /* this function blocks. use msg_try_receive(&msg) for non-blocking. */
    msg_receive(&msg);

    /* Print RSSI */
    print_rss(&msg);

    /* We initialize pkt_rcv with 1 */
    uint32_t pkt_rcv = 1;

    printf("interval is %d\n", interval + interval/10);
    for (int i = 0; i < num_pkts - 1; i++) {

        /* We wait for an interval plus 10%. If no packet arrived, we have missed it */
        if (xtimer_msg_receive_timeout(&msg, interval + interval/10) < 0) {
            continue;
        }

        /* We received one packet within the interval, lets print the RSSI */
        print_rss(&msg);

        /* And count it */
        pkt_rcv++;
    }

    /* Stop this thread from receiving anymore packets */
    _unregister_thread();

    printf("UDP experiment complete\n");

    /* Print the PER */
    print_prr(pkt_rcv, num_pkts);    

    return 0;
}

void print_rss(msg_t *msg)
{
    if (msg->type == GNRC_NETAPI_MSG_TYPE_RCV) {

        gnrc_pktsnip_t *pkt = msg->content.ptr;
        gnrc_pktsnip_t *snip = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_NETIF);
        gnrc_netif_hdr_t *hdr = snip->data;

        /**** TO-DO ****/

        /** 
         * gnrc_netif_hdr_t has information about the signal quality. The RSS 
         * value is equal to the RSSI from 'hdr' minus CC2538_RSSI_OFFSET You 
         * have to print out the RSS of each packet received. Feel free make any
         * changes to help you finish the lab faster 
        */
        gnrc_netif_hdr_t = hdr->rssi - CC2538_RSSI_OFFSET;
        printf("hdr: %d%", gnrc_netif_hdr_t * 100);

        /* Tell GNRC you are done with this packet so it can release the memory */
        gnrc_pktbuf_release(pkt);
    }
}

void print_prr(uint32_t pkt_rcv, uint32_t num_pkts)
{
    /**** TO-DO ****/

    /** 
     *You know the number of packets you were supposed to receive and the number
     * of packets you actually received. Calculate the Packet Reception Ratio 
     * and print it out 
     */
    

}