# GRBL headless bridge

## Important notification about security

When the application was started with root-privileges, those are dropped as related ports are set up.
Anyways, there're no credentials in regards of GRBL therefore be careful who can access those network ports I guess.


## My intention

I use this software on a Notebook, which is connencted to a small laser engraver.
So I can forward for example Lightburns GRBL to my laser engraver via WLAN.
It should also work on RasPi or any other Linux based system since it should be fully POSIX compatible.

Basically this is just a bridge from a TCP-Socket to UART.

Besides a TCP-UART bridge I just wanted to code something using a serial-port and network-socket.


# Build
```bash
make
```

# Usage
```bash
./grblbridge [-hv] [-p GRBL-port] [-m Monitor-port] -d <serial-port>
```

Once running, there's an interactive console.
You can see its command by typing 'h', there you could also inject further command to any device.
I have used that for debugging and some simple emulation.


## Required arguments

-d <serial-port> The device-node of your device (e.g. /dev/ttyUSB0).
The application tries to connect to the serial-port (device) dynamically.
This means, if the device isn't turned on, therefore the device-node isn't available yet, the application won't complain.
As the serial-port appeared the application will open and configure it during runtime.

## Optional arguments
-h Show available arguments


-v Be verbose


-p GRBL port. Default port is 23 (telnet). Since port 23 is < 1024 root-privileges are required.


-m Monitoring port. This port is disabled, when no port is given.
Set a monitoring port to see in a separate terminal (e.g. netcat) what messages are sent between the GRBL application and the device.

# Monitor

Connect to monitoring interface

```bash
nc <grblbridge-ipv4> <monitoring-port>
```
