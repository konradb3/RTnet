                     RTnet Configuration Service (RTcfg)
                     ===================================

                                Revision: 1.7


RTcfg is a configuration service for setting up a RTnet network and
distributing additional user-defined configuration data. This document
describes the protocol and the user interface of RTcfg.



Sequence Diagram
================

Normal Startup
--------------

Configuration                                                   Existing
   Server                              New Client                Client
      |                                     |                      |
      |       Client Config, Stage 1        |                      |
      |  (unicast/broadcast, single frame)  |   (if broadcasted)   |
      |-----------------------------------> | -------------------->|
      |                                    +-+                     |
      |                                    | |                     |
      |                                    | |   Set               |
      |                                    | | Config 1            |
      |                                    | |                     |
      |                                    +-+                     |
      .                                     .                      .
      .                                     .                      .
      |         Announce (broadcast)        |                      |
      | <-----------------------------------|--------------------> |
     +-+                                    |                     +-+
     | |                                    |                     | |
     | |                                    |                     | | Update
     | |                                    |                     | | Tables
     | |                                    |                     | |
     | |                                    |  Announce (unicast) +-+
     | | Update                             | <------------------- |
     | | Tables                            +-+                     |
     | |                                   | |                     |
     | |                                   | | Update              |
     | |                                   | | Tables              |
     | |                                   | |                     |
     | |      Client Config, Stage 2       +-+                     |
     +-+    (unicast, multiple frames)      |                      |
      | ----------------------------------> |                      |
      |                                    +-+                     |
      |                                    | |                     |
      |                                    | | Receive             |
      |                                    | | Config 2            |
      |                                    | |                     |
      |     Acknowledge Config (unicast)   +-+                     |
      |<----------------------------------- |                      |
      |                                     |                      |
      |                                    +-+                     |
      |                                    | |                     |
      |                                    | | Process             |
      |                                    | | Config 2            |
      |                                    | |                     |
      |                                    +-+                     |
      .                                     .                      .
      .                                     .                      .
      |          Ready (broadcast)          |                      |
      |<------------------------------------|--------------------->|
      .                                     .                      .
      .                                     .                      .
      |          Ready (broadcast)          |                      |
      |------------------------------------>|--------------------->|
      |                                     |                      |



Normal Operation
----------------

Configuration
   Server                                Client A               Client B
      |         Heartbeat (unicast)         |                      |
      |<------------------------------------|                      |
      |                                     |                      |
      .                                     .                      .
      .                                     .                      .
      |         Heartbeat (unicast)         |                      |
      |<-----------------------------------------------------------|
      |                                     |                      |



Failing Client
--------------

Configuration
   Server                                Client A               Client B
      |                                     |                      |
     +-+                                    |                      |
     | |                                    |                      |
     | | Missing                            |                      |
     | | Heartbeat                          |                      |
     | | Detection                          |                      |
     | |                                    |                      |
     +-+      Dead Station (broadcast)      |                      |
      | ----------------------------------> | -------------------> |
     +-+                                   +-+                    +-+
     | |                                   | |                    | |
     | | Update                            | | Update             | | Update
     | | Tables                            | | Tables             | | Tables
     | |                                   | |                    | |
     +-+                                   +-+                    +-+
      |                                     |                      |



Server Restart
--------------

Configuration                            Running                Running
   Server                                Client A               Client B
      |                                     |                      |
      |       Client Config, Stage 1        |                      |
      |  (unicast/broadcast, single frame)  |   (if broadcasted)   |
      |-----------------------------------> | -------------------->|
      |                                    +-+                     |
      |                                    | |                     |
      |                                    | | Receive             |
      |                                    | | Config 1            |
      |                                    | |                     |
      |         Announce (unicast)         +-+                     |
      |<----------------------------------- |                      |
     +-+                                   +-+                     |
     | |                                   | |                     |
     | | Update                            | | Update              |
     | | Client Status                     | | Server Address      |
     | | and Tables                        | | and Tables          |
     | |                                   | |                     |
     +-+                                   +-+                     |
      |                                     |                      |

Note: The configuration of a restarted or replace server must not differ from
      the configuration the currently running clients originally received. The
      only exception are the servers physical and logical addresses.



Frame Formats
=============

RTcfg frames are identified by the hexadecimal Ethernet type 9022. All frame
fields are encoded in network order (big endian). The first field consists of
an identification byte as illustrated below. Currently, the version bits are
zero in all frames, but they must be ignored in order to remain compatible
with possible future extensions.

 +---------------+------------------------+
 |  Bits 7 - 5   |       Bits 4 - 0       |
 | Frame Version |    Frame Identifier    |
 +---------------+------------------------+

When using RTmac, the lowest real-time priority is applied to RTcfg frames.



Stage 1 Configuration Frame
---------------------------

 +----------+----------------+----------------+----------------+ - -
 |  ID: 0   | Client Address | Client Address | Server Address |
 | (1 byte) |  Type (1 byte) |   (variable)   |   (variable)   |
 +----------+----------------+----------------+----------------+ - -
  - - +---------------+-----------------+-----------------+
      | Stage 2 Burst |  Configuration  |  Configuration  |
      | Rate (1 Byte) | Length (2 bytes)| Data (variable) |
  - - +---------------+-----------------+-----------------+

The overall frame length must not be greater than the MTU of the network
interface (typical: 1500 bytes). It might be limited by the installed RTmac
discipline.

Valid address types are:

  Symbolic Name    | Value | Address Length [Bytes per Field]
 ------------------+-------+----------------------------------
   RTCFG_ADDR_MAC  |   0   |                0
   RTCFG_ADDR_IP   |   1   |                4
   <extensible>    |  ...  |               ...

Stage 1 Configuration frames are sent as unicast when either only physical
client addresses are used (RTCFG_MAC), or when the linkage of physical and
logical (e.g. RTCFG_ADDR_IP) address is known. In any other case the frames
are broadcasted to all stations.

The Stage 2 Burst Rate field specifies the number of stage 2 configuration
frames the server is able to send without receiving an Acknowledge
Configuration frame. See below for the handshake mechanism to determine the
actual burst rate.

The configuration data of the first stage typically consists of parameters (or
even shell commands) which are required for the new client to become part of
an RTmac-managed network. If no data is available for this stage (e.g. when
RTmac is not used), the server sets the Configuration Length field to zero.



Announcement Frames
-------------------

New Announcement Frame:
 +----------+----------------+----------------+----------+---------------+
 |  ID: 1   | Client Address | Client Address |  Flags   | Stage 2 Burst |
 | (1 byte) |  Type (1 byte) |   (variable)   | (1 byte) | Rate (1 byte) |
 +----------+----------------+----------------+----------+---------------+

Reply Announcement Frame:
 +----------+----------------+----------------+----------+---------------+
 |  ID: 2   | Client Address | Client Address |  Flags   | Padding Field |
 | (1 byte) |  Type (1 byte) |   (variable)   | (1 byte) |   (1 byte)    |
 +----------+----------------+----------------+----------+---------------+

See "Stage 1 Configuration Frame" for valid address types and lengths.

New Announcement frames are sent as broadcast so that every other client can
update its ARP and routing table appropriately. In contrast, the Reply
Announcement frame is sent directly to the new client. A Reply Announcement
frame is also sent to the server if a client received a Stage 1 Configuration
frame while already being in operation mode. This occurs when the server is
restarted or replaced after a failure.

Flags are encoded as follows:

  Bit Number | Interpretation if set
 ------------+---------------------------------------------------------------
       0     | requests available stage 2 configuration data from the server
       1     | client is ready (i.e. will not send an explicit Ready frame)
      2-7    | <reserved>

Furthermore, the client reports its own Stage 2 Burst Rate back to the server.
The minimum of the server and the client value is selected as the actual burst
rate. After the server has send the according number of Stage 2 Configuration
frames, it will wait for an Acknowledge Configuration frame from the client.



Stage 2 Configuration Frames
----------------------------

Initial Frame:
 +----------+----------+-----------------+------------------+ - -
 |  ID: 3   |  Flags   | Active Stations | Heartbeat Period |
 | (1 byte) | (1 byte) |    (4 bytes)    |    (2 bytes)     |
 +----------+----------+-----------------+------------------+ - -
  - - +----------------------+--------------------+
      | Configuration Length | Configuration Data |
      |      (4 bytes)       |     (variable)     |
  - - +----------------------+--------------------+

Subsequent Fragments:
 +----------+-----------------+--------------------+
 |  ID: 4   | Fragment Offset | Configuration Data |
 | (1 byte) |    (4 bytes)    |     (variable)     |
 +----------+-----------------+--------------------+

The maximum length of a fragment is determined by the available MTU.

Stage 2 Configuration frames are always sent as unicast.

The Active Stations field contains the number of currently running stations,
including the server, but excluding the new client. This number is used be the
client to detect when all other clients have sent their Reply Announcement
frames, and when all stations have reported to be ready.

If the heartbeat mechanism shall be enabled on the new client, the Heartbeat
Period field contains the client's period in milliseconds for sending Heartbeat
frames. Otherwise it is set to zero.

Flags are encoded as follows:

  Bit Number | Interpretation if set
 ------------+---------------------------------------------------------------
       0     | <reserved>
       1     | server is ready (i.e. will not send an explicit Ready frame)
      2-7    | <reserved>

The second configuration stage can be used to distribute user-defined
configurations, applications, etc. (e.g. by sending a tar archive). If no
data is available for this stage, the server sets the Configuration Length
field to zero.



Acknowledge Configuration Frames
--------------------------------

 +----------+--------------------+
 |  ID: 5   | Acknowledge Length |
 | (1 byte) |     (4 bytes)      |
 +----------+--------------------+

An Acknowledge Configuration frame is sent by a new client after it has either
received the number of Stage 2 Configuration frames specified by the negotiated
burst rate (see above), or the last expected Stage 2 Configuration frame has
arrived.

The Acknowledge Length field is set to the number of yet successfully received
bytes. If the client has detected an inconsistent fragment, this number only
reflects the amount of data which was correctly received. The server will then
continue the Stage 2 Configuration frame transmission according to the
specified offset.



Ready Frame
-----------

 +----------+
 |  ID: 6   |
 | (1 byte) |
 +----------+

After a station has finished its setup procedures, it signals this state to all
other stations by sending a Ready frame as broadcast. This allows the server
and the clients to synchronise the completion of their configuration phase. The
frame is not sent if the client has already set the Ready Bit in its New
Announcement frame.



Heartbeat Frame
---------------

 +----------+
 |  ID: 7   |
 | (1 byte) |
 +----------+

Every client has to send Heartbeat frames within the period specified in the
Stage 2 Configuration frame as unicast to the server.



Dead Station Frame
------------------

 +----------+----------------+--------------------+--------------------+
 |  ID: 8   | Client Address |   Logical Client   |  Physical Client   |
 | (1 byte) |  Type (1 byte) | Address (variable) | Address (32 bytes) |
 +----------+----------------+--------------------+--------------------+

See "Stage 1 Configuration Frame" for valid address types and lengths.

When the server detects that a client failed to send a heartbeat frame within
the specified maximum period, it broadcasts a Dead Station frame to all other
clients. Every station will then remove the corresponding entries from its ARP
and routing tables.



Management Tool
===============

The RTcfg server and client functionality is controlled by the command line
tool rtcfg.



Server Commands
---------------

rtcfg <dev> server [-p period] [-b burstrate] [-h <heartbeat>]
      [-t <threshold>] [-r]

Starts a RTcfg server for the specified device <dev>. The server then sends
every 1000 ms stage 1 configuration frames to new clients. <period> (in
milliseconds) can be used to override the interval value. The number of
clients invited within one period is controlled by <burstrate> (default: 4).
This value also defines the number of stage 2 configuration fragments the
server should send as far as the client supports it (see also "announce").
<heartbeat> specifies the Heartbeat period of the clients in milliseconds
(default: 1000 ms), the value 0 turns the heartbeat mechanism off. <threshold>
sets the number of missing heartbeats after which a client shall be considered
dead (default: 2). If -r is given, the server automatically reports to be
ready within its stage 1 configuration frame, thus disengading it from issuing
an explicite "ready" command.

rtcfg <dev> add <address> [-hw <hw_address>] [-stage1 <stage1_file>]
      [-stage2 <stage2_file>] [-t <timeout>]

Adds a client to the server's list of potential participants of the network
connected to the specified device <dev>. <address> can be either an IP address
(A.B.C.D) or a physical address (AA:BB:CC:DD:EE:FF). If a physical address is
explicitely assigned using <hw_address>, the <address> parameter must define
the client's IP address. Optionally, files can specified which will be passed
during the different configuration stages. <timeout> (in milliseconds) defines
the internal timeout after which a half-finished client configuration is reset
to its initial state again. By default this reset is never performed.

rtcfg <dev> del <address>

Removes a client from the list of network participants. See above for details
about the address format.

rtcfg <dev> wait [-t <timeout>]

Waits until both configuration stages for all clients in the server's list are
completed. If <timeout> (in milliseconds) is given, rtcfg will return an error
code when the configuration cannot be completed within the specified time. The
default timeout is infinite.

rtcfg <dev> ready [-t <timeout>]

Reports that the server has completed its setup, generally including the RTmac
startup phase, and waits until all other stations are reporting to be ready as
well. If <timeout> (in milliseconds) is given, rtcfg will return an error code
when the synchronisation cannot be completed within the specified time. The
default timeout is infinite.



Client Commands
---------------

rtcfg <dev> client [-t <timeout>] [-c|-f <stage1_file>] [-m maxstations]

Waits until the first configuration stage is completed for the device <dev>.
If <timeout> (in milliseconds) is given, rtcfg will return an error code when
the configuration cannot be completed within the specified time. The default
timeout is infinite. The incoming configuration data is either send to the
standard output if -c is given or to <stage1_file> if specified. By default
clients can synchronise with up to 32 other stations (including the server).
This limit can be modified using the <maxstations> parameter.

rtcfg <dev> announce [-t <timeout>] [-c|-f <stage2_file>] [-b burstrate] [-r]

Sends an New Announcement frame over the device <dev> and waits until this
second configuration stage is completed. If <timeout> (in milliseconds) is
given, rtcfg will return an error code when the configuration cannot be
completed within the specified time. The default timeout is infinite. If -c or
-f is given, stage 2 configuration data is requested and either send to the
standard output or to <stage2_file>. <burstrate> controls the number of stage 2
configuration fragments the client should accept (default: 4). The actual
amount is negotiated according to both the client's and the server's capability
(see also "server"). If -r is given, the client automatically reports to be
ready within its announcement frame, thus disengading it from issuing an
explicite "ready" command.

rtcfg <dev> ready [-t <timeout>]

Reports that the client has completed its setup and waits until all other
stations are reporting to be ready as well. If <timeout> (in milliseconds) is
given, rtcfg will return an error code when the synchronisation cannot be
completed within the specified time. The default timeout is infinite.



Example
-------

This examples demonstrates how RTcfg can be used to start a RTnet/RTmac
network. With the current version 0.6.1, only a common startup is possible.
Future discipline implementations will also support adding new stations (with
known addresses!) to the network during runtime. These implementations will
then benefit from the stage 1 configuration mechanism.


rtnetserver.sh:

echo "Starting RTnet (RTmac master/RTcfg server)..."
# basic setup
rtifconfig rteth0 up $MYIP 255.255.255.0

# RTmac-specific stuff, depends on the discipline
# ***how it may look like in the future...***
#rtmacconfig_tdma rteth0 master 2000
#rtmacconfig_tdma rteth0 slot 0 -s 1500

# setup the RTcfg server
rtcfg rteth0 server
rtcfg rteth0 add 192.168.0.2 -stage1 client2-1.sh -stage2 client2-2.sh
rtcfg rteth0 add 192.168.0.3 -stage1 client3-1.sh -stage2 client3-2.sh
[...]

# wait for the configuration to be completed
rtcfg rteth0 wait

# setup RTmac according to version 0.6.1 and earlier
# (with precautious delays)
rtifconfig rteth0 mac master 2000
rtifconfig rteth0 mac add 192.168.0.2 200
rtifconfig rteth0 mac add 192.168.0.3 400
sleep 5
rtifconfig rteth0 mac up
sleep 10

rtcfg rteth0 ready
echo "RTnet is running now."
EOF


client2-1.sh:

# RTmac-specific stuff, depends on the discipline
# ***how it may look like in the future...***
#rtmacconfig_tdma rteth0 slot 200 -s 1000
EOF


client2-2.sh:

echo "Hello, I'm client 192.168.0.2!"
EOF


client3-1.sh:

# RTmac-specific stuff, depends on the discipline
# ***how it may look like in the future...***
#rtmacconfig_tdma rteth0 slot 300 -s 1500
EOF


client3-2.sh:

echo "Hello, I'm client 192.168.0.3!"
EOF


rtnetclient.sh:

echo "Starting RTnet (client)..."
# basic setup
rtifconfig rteth0 up $MY_IP 255.255.255.0

# wait for the first configuration stage
rtcfg rteth0 client -f /tmp/my_stage1_config

# run the stage 1 script
chmod u+x /tmp/my_stage1_config
/tmp/my_stage1_config

# announce myself and wait for the second stage
rtcfg rteth0 announce -f /tmp/my_stage2_config

# setup RTmac according to version 0.6.1 and earlier
rtifconfig rteth0 mac client

# run the stage 2 script
chmod u+x /tmp/my_stage2_config
/tmp/my_stage2_config

rtcfg rteth0 ready
echo "RTnet is running now."
EOF


2003, 2004, Jan Kiszka <jan.kiszka-at-web.de>
