# GRBL headless bridge
Use this software on your RapberryPi or Notebook, which is conencted to your laser and your network via LAN or WIFI
Personally I use Lightburn to connect to the brdige via GRBL (LAN).

Basically this is just a bridge from a TCP-Socket to UART.

# Build
```bash
make
```

# Usage
```bash
./grblbridge [-hv] [-p port] [-m port] <serial-port>
```

## Required arguments

serial-port The device-node of your laser (e.g. /dev/ttyUSB0).
The application tries to connect to the laser dynamically.
In case the laser isn't turned on during startup, it's gonna be opened later.

## Optional arguments
-h Show available arguments


-v Be verbose


-p GRBL port. Default port is 23 (telnet). Since port 23 is < port 1024 root-privileges are required.


-m Monitoring port. This port is disabled, when no port is given.

# Monitor

Connect to monitoring interface (currently known issues in WSL2)
```bash
nc <grblbridge-address> <monitoring-port>
```
