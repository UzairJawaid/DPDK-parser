#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>

static volatile bool force_quit;

/* Ports set in promiscuous mode off by default. */
// static int promiscuous_on;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;
#define MAX_PORTS 2
#define MAX_TX_QUEUE_PER_PORT 16

/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[MAX_PORTS];

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[MAX_PORTS];

// static unsigned int l2fwd_rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

static struct rte_ether_addr override_dst_mac;
static struct rte_ether_addr override_src_mac;

static int dst_mac_override_enabled = 0;
static int src_mac_override_enabled = 0;

struct __rte_cache_aligned lcore_queue_conf
{
	unsigned n_rx_queue;
	struct
	{
		uint16_t port_id;
		uint16_t queue_id;
		uint16_t tx_port_id;
		uint16_t tx_queue_id;
	} rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
};

struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
/* >8 End of list of queues to be polled for a given lcore. */

static struct rte_eth_dev_tx_buffer *tx_buffers[MAX_PORTS][MAX_TX_QUEUE_PER_PORT];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *l2fwd_pktmbuf_pool = NULL;

/* Per-port statistics struct */
struct __rte_cache_aligned l2fwd_port_statistics
{
	uint64_t tx;
	uint64_t rx;
	uint64_t tx_bytes;
	uint64_t rx_bytes;
	uint64_t tx_Bps;
	uint64_t rx_Bps;
	uint64_t dropped;
	uint64_t nip;
	uint64_t ip_dropped;
};
struct l2fwd_port_statistics port_statistics[MAX_PORTS];

/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 10; /* default period is 10 seconds */

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx, total_ip, total_ip_drop, total_bytes_tx, total_bytes_rx, total_bit_tx, total_bit_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;
	total_ip = 0;
	total_ip_drop = 0;
	total_bytes_tx = 0;
	total_bit_rx = 0;
	total_bit_tx = 0;
	total_bytes_rx = 0;
	const char clr[] = {27, '[', '2', 'J', '\0'};
	const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < MAX_PORTS; portid++)
	{
		/* skip disabled ports */
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24" PRIu64
			   "\nPackets sentBytes: %19" PRIu64
			   "\nPackets sentBits: %20" PRIu64
			   "\nPackets received: %20" PRIu64
			   "\nPackets recBytes: %20" PRIu64
			   "\nPackets recBits: %21" PRIu64
			   "\nPackets sentBps: %21" PRIu64
			   "\nPackets sentbps: %21" PRIu64
			   "\nPackets recvBps: %21" PRIu64
			   "\nPackets recvbps: %21" PRIu64
			   "\nIP Packets: %26" PRIu64
			   "\nPackets dropped: %21" PRIu64
			   "\nPackets non ip: %21" PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].tx_bytes,
			   port_statistics[portid].tx_bytes * 8,
			   port_statistics[portid].rx,
			   port_statistics[portid].rx_bytes,
			   port_statistics[portid].rx_bytes * 8,
			   port_statistics[portid].tx_Bps,
			   port_statistics[portid].tx_Bps * 8,
			   port_statistics[portid].rx_Bps,
			   port_statistics[portid].rx_Bps * 8,
			   port_statistics[portid].nip,
			   port_statistics[portid].dropped,
			   port_statistics[portid].ip_dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
		total_ip += port_statistics[portid].nip;
		total_ip_drop += port_statistics[portid].ip_dropped;
		total_bytes_tx += port_statistics[portid].tx_bytes;
		total_bit_tx += port_statistics[portid].tx_bytes * 8;
		total_bytes_rx += port_statistics[portid].rx_bytes;
		total_bit_rx += port_statistics[portid].rx_bytes * 8;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18" PRIu64
		   "\nTotal packets received: %14" PRIu64
		   "\nTotal ip packets: %20" PRIu64
		   "\nTotal packets dropped: %15" PRIu64
		   "\nTotal non ip packets: %15" PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_ip,
		   total_packets_dropped,
		   total_ip_drop);
	printf("\n====================================================\n");

	fflush(stdout);
}

static bool
l2fwd_parsing(struct rte_mbuf *m, unsigned dest_portid)
{
	struct rte_ether_hdr *eth;
	struct rte_ipv4_hdr *ip;
	struct rte_udp_hdr *udp;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
	{
		return false;
	}

	// Count this as an IP packet
	port_statistics[dest_portid].nip += 1;
	ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(*eth));
	if (ip->next_proto_id == IPPROTO_UDP)
	{
		udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
									  sizeof(*eth) + sizeof(*ip));
	}

	if (dst_mac_override_enabled)
	{
		rte_ether_addr_copy(&override_dst_mac, &eth->dst_addr);
	}

	if (src_mac_override_enabled)
	{
		rte_ether_addr_copy(&override_src_mac, &eth->src_addr);
	}

	return true;
}

/* Simple forward. 8< */
static void
l2fwd_simple_forward(struct rte_mbuf *m, struct lcore_queue_conf *qconf, int qidx)
{
	unsigned dst_port;
	int sent;

	dst_port = qconf->rx_queue_list[qidx].tx_port_id;
	uint16_t tx_queue = qconf->rx_queue_list[qidx].tx_queue_id;
	// l2fwd_parsing(m, dst_port);
	if (!l2fwd_parsing(m, dst_port))
	{
		rte_pktmbuf_free(m);
		port_statistics[dst_port].ip_dropped += 1;
		return;
	}
	port_statistics[dst_port].tx_bytes += rte_pktmbuf_pkt_len(m);
	sent = rte_eth_tx_buffer(dst_port, tx_queue, tx_buffers[dst_port][tx_queue], m);
	if (sent)
	{
		port_statistics[dst_port].tx += sent;
	}
}

/* >8 End of simple forward. */

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	int sent;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	unsigned i, j, portid, nb_rx, queueid;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
							   BURST_TX_DRAIN_US;

	prev_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_queue == 0)
	{
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++)
	{

		portid = qconf->rx_queue_list[i].port_id;
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u RX port %u queue %u TX port %u queue %u\n",
				lcore_id,
				qconf->rx_queue_list[i].port_id,
				qconf->rx_queue_list[i].queue_id,
				qconf->rx_queue_list[i].tx_port_id,
				qconf->rx_queue_list[i].tx_queue_id);
	}

	while (!force_quit)
	{

		/* Drains TX queue in its main loop. 8< */
		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc))
		{
			for (i = 0; i < qconf->n_rx_queue; i++)
			{
				uint16_t portid = qconf->rx_queue_list[i].tx_port_id;
				uint16_t queueid = qconf->rx_queue_list[i].tx_queue_id;

				sent = rte_eth_tx_buffer_flush(portid, queueid,
											   tx_buffers[portid][queueid]);
				if (sent)
					port_statistics[portid].tx += sent;
			}

			prev_tsc = cur_tsc;
		}
		/* >8 End of draining TX queue. */

		/* Read packet from RX queues. 8< */
		for (i = 0; i < qconf->n_rx_queue; i++)
		{
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;

			nb_rx = rte_eth_rx_burst(portid, queueid,
									 pkts_burst, MAX_PKT_BURST);

			if (unlikely(nb_rx == 0))
				continue;

			port_statistics[portid].rx += nb_rx;

			for (j = 0; j < nb_rx; j++)
			{
				m = pkts_burst[j];
				uint16_t pkt_len = rte_pktmbuf_pkt_len(m);
				port_statistics[portid].rx_bytes += pkt_len;
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				l2fwd_simple_forward(m, qconf, i);
			}
		}
		/* >8 End of read packet from RX queues. */
	}
}

static int
l2fwd_launch_one_lcore(__rte_unused void *dummy)
{
	l2fwd_main_loop();
	return 0;
}

static const char short_options[] =
	"D:" /* destination MAC override */
	"S:" /* source MAC override */
	"T:" /*set the print stats time*/
	"h";

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret; // timer_secs;
	char **argvopt;
	char *prgname = argv[0];

	argvopt = argv;

	while ((opt = getopt(argc, argvopt, short_options)) != EOF)
	{

		switch (opt)
		{
		case 'D':
			if (rte_ether_unformat_addr(optarg, &override_dst_mac) != 0)
			{
				printf("Invalid destination MAC format: %s\n", optarg);
				return -1;
			}
			dst_mac_override_enabled = 1;
			break;

		case 'S':
			if (rte_ether_unformat_addr(optarg, &override_src_mac) != 0)
			{
				printf("Invalid source MAC format: %s\n", optarg);
				return -1;
			}
			src_mac_override_enabled = 1;
			break;
		case 'T':
			timer_period = atoi(optarg);
			if (timer_period < 1 || timer_period > 3600)
			{
				printf("Invalid timer period %s\n", optarg);
				return -1;
			}
			break;
		case 'h':
			printf("Usage: %s [-D dst_mac] [-S src_mac] [-T period_seconds]\n", prgname);
			exit(0);
		default:
			// l2fwd_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind - 1] = prgname;

	ret = optind - 1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM)
	{
		printf("\n\nSignal %d received, preparing to exit...\n",
			   signum);
		force_quit = true;
	}
}

int main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	int ret;
	uint16_t nb_ports;
	// uint16_t nb_ports_available = 0;
	uint16_t portid; // last_port;
	unsigned lcore_id, rx_lcore_id;
	unsigned int nb_mbufs;
	uint64_t prev_tsc, cur_tsc, timer_tsc, current0_rBps, current0_tBps, pre0_rBps, pre0_tBps, current1_rBps, current1_tBps, pre1_rBps, pre1_tBps;
	/* Init EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	uint16_t num_lcores = rte_lcore_count();

	if (num_lcores <= 1)
		rte_exit(EXIT_FAILURE, "At least 2 lcores required (1 main + >=1 worker)\n");
	uint16_t num_workers = num_lcores - 1;
	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");
	/* >8 End of init EAL. */

	/* convert to number of cycles */
	if (timer_period > 0)
		timer_period *= rte_get_timer_hz();

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* >8 End of initialization of the driver. */
	l2fwd_dst_ports[0] = 1;
	l2fwd_dst_ports[1] = 0;
	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	uint16_t queue_id = 0;
	RTE_LCORE_FOREACH_WORKER(rx_lcore_id)
	{
		qconf = &lcore_queue_conf[rx_lcore_id];
		qconf->n_rx_queue = 0;

		// For each port
		RTE_ETH_FOREACH_DEV(portid)
		{
			qconf->rx_queue_list[qconf->n_rx_queue].port_id = portid;
			qconf->rx_queue_list[qconf->n_rx_queue].queue_id = queue_id;
			qconf->rx_queue_list[qconf->n_rx_queue].tx_port_id = l2fwd_dst_ports[portid];
			qconf->rx_queue_list[qconf->n_rx_queue].tx_queue_id = queue_id;
			qconf->n_rx_queue++;

			printf("Lcore %u: RX port %u, queue %u TX port %u, queue %u\n",
				   rx_lcore_id,
				   portid, queue_id,
				   l2fwd_dst_ports[portid], queue_id);
		}
		queue_id++;
	}

	// nb_mbufs = RTE_MAX(
	// 	nb_ports * num_workers * (nb_rxd + nb_txd + MAX_PKT_BURST + MEMPOOL_CACHE_SIZE),
	// 	8192U);
	nb_mbufs = 8192U;
	/* Create the mbuf pool. 8< */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
												 MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
												 rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	/* >8 End of create the mbuf pool. */

	/* Initialise each port */
	RTE_ETH_FOREACH_DEV(portid)
	{
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		struct rte_eth_conf local_port_conf = port_conf;
		struct rte_eth_dev_info dev_info;

		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);

		ret = rte_eth_dev_info_get(portid, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
					 "Error during getting device (port %u) info: %s\n",
					 portid, strerror(-ret));
		printf("Port %u: device reports max_rx_queues=%u, max_tx_queues=%u\n",
			   portid, dev_info.max_rx_queues, dev_info.max_tx_queues);
		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
		/* Configure the number of queues for a port. */
		ret = rte_eth_dev_configure(portid, num_workers, num_workers, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
					 ret, portid);
		/* >8 End of configuration of the number of queues for a port. */

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
											   &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
					 "Cannot adjust number of descriptors: err=%d, port=%u\n",
					 ret, portid);

		ret = rte_eth_macaddr_get(portid,
								  &l2fwd_ports_eth_addr[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
					 "Cannot get MAC address: err=%d, port=%u\n",
					 ret, portid);

		fflush(stdout);
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		/* RX queue setup. 8< */
		for (uint16_t q = 0; q < num_workers; q++)
		{
			ret = rte_eth_rx_queue_setup(portid, q, nb_rxd,
										 rte_eth_dev_socket_id(portid),
										 &rxq_conf,
										 l2fwd_pktmbuf_pool);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u, queue=%u\n",
						 ret, portid, q);
		}

		fflush(stdout);
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		for (uint16_t q = 0; q < num_workers; q++)
		{

			ret = rte_eth_tx_queue_setup(portid, q, nb_txd,
										 rte_eth_dev_socket_id(portid),
										 &txq_conf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
						 ret, portid);
			// Allocate tx_buffer
			tx_buffers[portid][q] = rte_zmalloc_socket("tx_buffer",
													   RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
													   rte_eth_dev_socket_id(portid));
			if (tx_buffers[portid][q] == NULL)
				rte_exit(EXIT_FAILURE, "Cannot allocate tx buffer for port %u queue %u\n",
						 portid, q);

			rte_eth_tx_buffer_init(tx_buffers[portid][q], MAX_PKT_BURST);

			ret = rte_eth_tx_buffer_set_err_callback(tx_buffers[portid][q],
													 rte_eth_tx_buffer_drop_callback,
													 &port_statistics[portid].dropped);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "Cannot set err callback for port %u queue %u\n",
						 portid, q);
		}

		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
					 ret, portid);

		printf("done: \n");

		printf("Port %u, MAC address: " RTE_ETHER_ADDR_PRT_FMT "\n\n",
			   portid,
			   RTE_ETHER_ADDR_BYTES(&l2fwd_ports_eth_addr[portid]));

		/* initialize port stats */
		memset(&port_statistics[portid], 0, sizeof(port_statistics[portid]));
	}

	ret = 0;
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MAIN);
	prev_tsc = rte_rdtsc();
	timer_tsc = 0;
	current0_tBps = 0;
	current1_tBps = 0;
	current0_rBps = 0;
	current1_rBps = 0;
	pre0_tBps = 0;
	pre1_tBps = 0;
	pre0_rBps = 0;
	pre1_rBps = 0;
	while (!force_quit)
	{

		if (timer_period > 0)
		{

			cur_tsc = rte_rdtsc();

			timer_tsc += (cur_tsc - prev_tsc);
			prev_tsc = cur_tsc;
			current0_tBps = port_statistics[0].tx_bytes;
			current1_tBps = port_statistics[1].tx_bytes;
			current0_rBps = port_statistics[0].rx_bytes;
			current1_rBps = port_statistics[1].rx_bytes;
			/* if timer has reached its timeout */
			if (unlikely(timer_tsc >= timer_period))
			{

				port_statistics[0].tx_Bps = (current0_tBps - pre0_tBps) / (timer_tsc / rte_get_tsc_hz());
				port_statistics[1].tx_Bps = (current1_tBps - pre1_tBps) / (timer_tsc / rte_get_tsc_hz());
				port_statistics[0].rx_Bps = (current0_rBps - pre0_rBps) / (timer_tsc / rte_get_tsc_hz());
				port_statistics[1].rx_Bps = (current1_rBps - pre1_rBps) / (timer_tsc / rte_get_tsc_hz());
				pre0_tBps = current0_tBps;
				pre1_tBps = current1_tBps;
				pre0_rBps = current0_rBps;
				pre1_rBps = current1_rBps;

				print_stats();
				timer_tsc = 0;
			}
		}
	}
	RTE_LCORE_FOREACH_WORKER(lcore_id)
	{
		if (rte_eal_wait_lcore(lcore_id) < 0)
		{
			ret = -1;
			break;
		}
	}

	RTE_ETH_FOREACH_DEV(portid)
	{
		printf("Closing port %d...", portid);
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%d, port=%d\n",
				   ret, portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}

	/* clean up the EAL */
	rte_eal_cleanup();
	printf("Bye...\n");

	return ret;
}
