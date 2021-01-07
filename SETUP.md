# Test Setup to Generate Simulated Traffic

## Overview

The setup consists of 2 workstations (`david` and `benjamin`), both installed with network interface card Intel Corporation Ethernet Server Adapter X520-2 \[8086:000c\] (the SFP+ module is from Finisar), and connected directly with optical fiber. The subnet used is 192.168.2.0/24, where `benjamin` is 192.168.2.1 and `david` is 192.168.2.2.

`benjamin` is the traffic generating machine, on which we run TREX, the traffic generator. `david` is the machine under test, on which we run Nginx to serve HTTP traffic, simulating a real-work server.

## Setting up `david`

Install Nginx with `sudo zypper install -y nginx`

Follow TREX's guide on [Nginx Tuning](https://trex-tgn.cisco.com/trex/doc/trex_astf_vs_nginx.html#_nginx_tuning) to increase the maximum number of file descriptors allow, then restart Nginx with `sudo systemctl restart nginx`.

Since TREX cannot handle ARP request to resolve IPs used by `benjamin`, we need to manually setup static mapping to IP to MAC address so `david` won't send out ARP requests.

```shell
$ for i in $(seq 3 254); do sudo ip neigh add 192.168.2.$i lladdr 00:1b:21:3c:9e:2c dev ixgbe1; done
```

## Setting up `benjamin`

Install the latest TREX from archive at https://trex-tgn.cisco.com/trex/release/latest

Write the following TREX configuration into `/etc/trex_cfg.yaml`.

```yaml
- port_limit      : 2
  version         : 2
  low_end: true
#List of interfaces. Change to suit your setup. Use ./dpdk_setup_ports.py -s to see available options
  #interfaces    : ["ixgbe1","dummy"]
  interfaces    : ["0000:04:00.1","dummy"]
  port_info       :  # Port IPs. Change to suit your needs. In case of loopback, you can leave as is.
          - src_mac: 00:1b:21:3c:9e:2c
            dest_mac: 00:1b:21:3c:9e:31
          - src_mac: 00:1b:21:3c:9e:31
            dest_mac: 00:1b:21:3c:9e:2c
```

Modify nginx_wget.py so the 192.168.2.0/24 subnet is used.

```diff
--- astf/nginx_wget.py
+++ astf/nginx_wget.py
@@ -4,8 +4,8 @@
 class Prof1():
     def get_profile(self, **kwargs):
         # ip generator
-        ip_gen_c = ASTFIPGenDist(ip_range = ["16.0.0.1", "16.0.0.200"], distribution="seq")
-        ip_gen_s = ASTFIPGenDist(ip_range = ["48.0.0.1", "48.0.0.200"], distribution="seq")
+        ip_gen_c = ASTFIPGenDist(ip_range = ["192.168.2.3", "192.168.2.253"], distribution="seq",)
+        ip_gen_s = ASTFIPGenDist(ip_range = ["192.168.2.2", "192.168.2.2"], distribution="seq")
         ip_gen = ASTFIPGen(glob = ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client = ip_gen_c,
                            dist_server = ip_gen_s)
```

Then start the traffic generator with the following command. `$multiplier` is the flow multiplier, this corresponds to the number of HTTP request that will be sent per second, and `$duration` is how long the test should last, which in turn affect the total number of requests that will be sent (total requests = `$multiplier` * `$duration`).

```shell
$ sudo ./t-rex-64 -f astf/nginx_wget_local.py --astf --astf-client-mask 0x1 -m $multiplier -d $duration --nc
```
