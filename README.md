# ssdp-connect
is a simple library to establish connection between devices in local network using SSDP discovery.

`IMPORTANT:` this is not a full-featured SSDP library. You can use it only for interconnecting 
instances of your application in the local network or in other words to find your app in the local network 
and make a connection with it (doesn't matter TCP or UDP), but not for analyzing and responding to foreign SSDP packets. 
There is no guarantee that information that this library will retrieve from foreign SSDP requests will be valid.

`Tested on Windows (10, 8.1), Linux (Manjaro)`

## How it works
- Server creates two non-blocking UDP sockets. One for receiving SSDP multicasts, other for making connection with client. Client creates non-blocking UDP socket.
- Client sends ssdp:discover request. Server receives and responds to this request.
- Client sends a message to server (may be some kind of handshake).
- Now Client and Server are connected. Server can close SSDP socket.

This algorithm can be changed to establish TCP connection instead of UDP.

## Building
You can just add source files to your project or makefile. If you want a static library then use cmake. Examples also can be built with cmake. 
On Windows you must link your project to WinSock library: `Ws2_32.lib`
