# ssdp-discovery
is a simple library for SSDP discovery. I made it to connect application instances in a local network.
`Tested on Windows`

## Building
You can just add `ssdp.c` to your project or makefile. If you want a static library then use cmake.
On Windows you must link your project to WinSock library: `Ws2_32.lib`

## Usage
```c
ssdp_ctx ctx;
ssdp_ctx_init(&ctx, 0, "service_type_uri", "service_name_uri", 1024, 1024);
ssdp_begin_receiving(&ctx);

ssdp_packet p;
int result = 0;
do {
	memset(&p, 0, sizeof(p));
	result = ssdp_receive(&ctx, &p);
	if (result > 0 && p.type != SSDP_PT_NONE) {
		char ip[16] = {0};
		inet_ntop(p.addr.sin_family, &p.addr.sin_addr, ip, sizeof(ip));
		printf("Received a %s packet from %s:%hu:\n\tService type: %s\n",
			ssdp_packet_type_string(p.type), ip, ntohs(p.addr.sin_port), p.service_type);
	}
} while (result >= 0);

ssdp_ctx_release(&ctx);
```
