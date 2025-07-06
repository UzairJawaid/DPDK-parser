Place this folder in DPDK/examples

Commands to compile
cd dpdk
meson configure -Dexamples=DPDK=parser build
ninja -C build


My testing setup,

I have tested in orcale cloud using tap interfaces, you can use any interface that support multiple queues.
My setup was restricted to 4 cores and tap interfaces so couldnot perform performance testing, though i have tested using scapy.


Sample Commands
sudo ./build/examples/dpdk-DPDK-parser -l 0-3 -n 4 -b 0000:00:06.0 --vdev=net_tap0,iface=tap0,queues=3 --vdev=net_tap2,iface=tap2,queues=3 -- -S 00:00:00:00:AB:02 -D 00:00:00:00:cd:03 -T 1

// -S will change source mac for all packets
// -D will change dst mac for all packets
// -T will set the time to print stats.

this command will make two tap interfaces i started tcpdum on one and send packets on other using scapy. Testpmd showed  the logs correctly.


//************************Sample logs ***********************************************************??


ubuntu@instance-20250701-2338:~/dpdk$ sudo ./build/examples/dpdk-DPDK-parser -l 0-3 -n 4 -b 0000:00:06.0 --vdev=net_tap0,iface=tap0,queues=3 --vdev=net_tap2,iface=tap2,queues=3 -- -S 00:00:00:00:AB:02 -D 00:00:00:00:cd:03 -T 1
EAL: Detected CPU lcores: 4
EAL: Detected NUMA nodes: 1
EAL: Detected static linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'PA'
Lcore 1: RX port 0, queue 0 TX port 1, queue 0
Lcore 1: RX port 1, queue 0 TX port 0, queue 0
Lcore 2: RX port 0, queue 1 TX port 1, queue 1
Lcore 2: RX port 1, queue 1 TX port 0, queue 1
Lcore 3: RX port 0, queue 2 TX port 1, queue 2
Lcore 3: RX port 1, queue 2 TX port 0, queue 2
Initializing port 0... Port 0: device reports max_rx_queues=64, max_tx_queues=64
done:
Port 0, MAC address: F6:9B:B6:41:F6:B3

Initializing port 1... Port 1: device reports max_rx_queues=64, max_tx_queues=64
done:
Port 1, MAC address: 9A:1C:F4:4A:9A:AD

L2FWD: entering main loop on lcore 1
L2FWD:  -- lcoreid=1 RX port 0 queue 0 TX port 1 queue 0
L2FWD:  -- lcoreid=1 RX port 1 queue 0 TX port 0 queue 0
L2FWD: entering main loop on lcore 2
L2FWD:  -- lcoreid=2 RX port 0 queue 1 TX port 1 queue 1
L2FWD:  -- lcoreid=2 RX port 1 queue 1 TX port 0 queue 1
L2FWD: lcore 0 has nothing to do
L2FWD: entering main loop on lcore 3
L2FWD:  -- lcoreid=3 RX port 0 queue 2 TX port 1 queue 2
L2FWD:  -- lcoreid=3 RX port 1 queue 2 TX port 0 queue 2

Port statistics ====================================
Statistics for port 0 ------------------------------
Packets sent:                        0
Packets sentBytes:                   0
Packets sentBits:                    0
Packets received:                    2
Packets recBytes:                  180
Packets recBits:                  1440
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                   180
Packets recvbps:                  1440
IP Packets:                          0
Packets dropped:                     0
Packets non ip:                     3
Statistics for port 1 ------------------------------
Packets sent:                        0
Packets sentBytes:                   0
Packets sentBits:                    0
Packets received:                    3
Packets recBytes:                  266
Packets recBits:                  2128
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                   266
Packets recvbps:                  2128
IP Packets:                          0
Packets dropped:                     0
Packets non ip:                     2
Aggregate statistics ===============================
Total packets sent:                  0
Total packets received:              5
Total ip packets:                    0
Total packets dropped:               0
Total non ip packets:               5
====================================================

Port statistics ====================================
Statistics for port 0 ------------------------------
Packets sent:                        0
Packets sentBytes:                   0
Packets sentBits:                    0
Packets received:                    3
Packets recBytes:                  266
Packets recBits:                  2128
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                    86
Packets recvbps:                   688
IP Packets:                          0
Packets dropped:                     0
Packets non ip:                     6
Statistics for port 1 ------------------------------
Packets sent:                        0
Packets sentBytes:                   0
Packets sentBits:                    0
Packets received:                    6
Packets recBytes:                  516
Packets recBits:                  4128
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                   250
Packets recvbps:                  2000
IP Packets:                          0
Packets dropped:                     0
Packets non ip:                     3
Aggregate statistics ===============================
Total packets sent:                  0
Total packets received:              9
Total ip packets:                    0
Total packets dropped:               0
Total non ip packets:               9
====================================================
Port statistics ====================================
Statistics for port 0 ------------------------------
Packets sent:                    41424
Packets sentBytes:             1408416
Packets sentBits:             11267328
Packets received:                   10
Packets recBytes:                  796
Packets recBits:                  6368
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                     0
Packets recvbps:                     0
IP Packets:                      41424
Packets dropped:                     0
Packets non ip:                    10
Statistics for port 1 ------------------------------
Packets sent:                        0
Packets sentBytes:                   0
Packets sentBits:                    0
Packets received:                41434
Packets recBytes:              1409212
Packets recBits:              11273696
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                     0
Packets recvbps:                     0
IP Packets:                          0
Packets dropped:                     0
Packets non ip:                    10
Aggregate statistics ===============================
Total packets sent:              41424
Total packets received:          41444
Total ip packets:                41424
Total packets dropped:               0
Total non ip packets:              20
====================================================
Port statistics ====================================
Statistics for port 0 ------------------------------
Packets sent:                    46628
Packets sentBytes:             1585352
Packets sentBits:             12682816
Packets received:                   10
Packets recBytes:                  796
Packets recBits:                  6368
Packets sentBps:                 59024
Packets sentbps:                472192
Packets recvBps:                     0
Packets recvbps:                     0
IP Packets:                      46628
Packets dropped:                     0
Packets non ip:                    11
Statistics for port 1 ------------------------------
Packets sent:                        0
Packets sentBytes:                   0
Packets sentBits:                    0
Packets received:                46639
Packets recBytes:              1586218
Packets recBits:              12689744
Packets sentBps:                     0
Packets sentbps:                     0
Packets recvBps:                 59024
Packets recvbps:                472192
IP Packets:                          0
Packets dropped:                     0
Packets non ip:                    10
Aggregate statistics ===============================
Total packets sent:              46628
Total packets received:          46650
Total ip packets:                46628
Total packets dropped:               0
Total non ip packets:              21
====================================================
