                     RTnet Configuration Service (RTcfg)
                     ===================================

                                Revision: 1.3


RTcfg is a configuration service for setting up an RTnet network and
distributing additional user-defined configurations. It is an embedded part of
the RTnet stack. This document describes the protocol and the user interface
of RTcfg.



Sequence Diagram
================

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
      |         Announce (broadcast)       +-+                     |
      | <---------------------------------- | -------------------> |
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
      |                                    | |   Set               |
      |                                    | | Config 2            |
      |                                    | |                     |
      |     Acknowledge Config (unicast)   +-+                     |
      |<----------------------------------- |                      |
      |                                     |                      |
      .                                     .                      .
      .                                     .                      .
      |         Heartbeat (unicast)         |                      |
      |<------------------------------------|                      |
      |                                     |                      |
      .                                     .                      .
      .                                     .                      .
      |         Heartbeat (unicast)         |                      |
      |<-----------------------------------------------------------|
      |                                     |                      |



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

Stage 1 Configuration frames are sent as unicast when using physical client
addresses (RTCFG_MAC). Otherwise, they are broadcasted to any station.

The configuration data of the first stage typically consists of parameters (or
even shell commands) which are required for the new client to become part of
an RTmac-managed network. If no data is available for this stage (e.g. when
RTmac is not used), the server sets the Configuration Length field to zero.

The Stage 2 Burst Rate field specifies the number of stage 2 configuration
frames the server is able to send without receiving an Acknowledge
Configuration frame. See below for the handshake mechanism to determine the
actual burst rate.



Announcement Frames
-------------------

New Announcement Frame:
 +----------+----------------+----------------+-------------+---------------+
 |  ID: 1   | Client Address | Client Address | Get Config. | Stage 2 Burst |
 | (1 byte) |  Type (1 byte) |   (variable)   |  (1 byte)   | Rate (1 byte) |
 +----------+----------------+----------------+-------------+---------------+

Reply Announcement Frame:
 +----------+----------------+----------------+
 |  ID: 2   | Client Address | Client Address |
 | (1 byte) |  Type (1 byte) |   (variable)   |
 +----------+----------------+----------------+

See "Stage 1 Configuration Frame" for valid address types and lengths.

New Announcement frames are sent as broadcast so that every other client can
update its ARP and routing table appropriately. In contrast, the Reply
Announcement frame is sent directly to the new client.

If the "Get Configuration" field is set to a non-zero value, the server starts
sending the Stage 2 Configuration frames.

Furthermore, the client reports its own Stage 2 Burst Rate back to the server.
The minimum of the server and the client value is selected as the actual burst
rate. After the server has send the according number of Stage 2 Configuration
frames, it will wait for an Acknowledge Configuration frame from the client.



Stage 2 Configuration Frames
----------------------------

Initial Frame:
 +----------+-----------------+------------------+----------------------+ - -
 |  ID: 3   | Active Clients  | Heartbeat Period | Configuration Length |
 | (1 byte) |    (4 bytes)    |    (2 bytes)     |      (4 bytes)       |
 +----------+-----------------+------------------+----------------------+ - -
  - - +--------------------+
      | Configuration Data |
      |     (variable)     |
  - - +--------------------+

Subsequent Fragments:
 +----------+-----------------+--------------------+
 |  ID: 4   | Fragment Offset | Configuration Data |
 | (1 byte) |    (4 bytes)    |     (variable)     |
 +----------+-----------------+--------------------+

The maximum length of a fragment is determined by the available MTU.

Stage 2 Configuration frames are always sent as unicast.

The Active Clients field contains the number of currently running stations
excluding the server and the new client. This number is used be the client
to detect when all other clients have sent their Reply Announcement frames.

If the heartbeat mechanism shall be enabled on the new client, the Heartbeat
Period field contains the client's period in milliseconds for sending Heartbeat
frames. Otherwise it is set to zero.

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



Heartbeat Frame
---------------

 +----------+
 |  ID: 6   |
 | (1 byte) |
 +----------+

Every client has to send Heartbeat frames with the period specified in the
Stage 2 Configuration frame as unicast to the server.



Tool Extensions
===============

The RTcfg server and client functionality is controlled by the new command
line tool rtcfg.

Server Commands
---------------

rtcfg <dev> server [-p period] [-b burstrate] [-h <heartbeat>] [-t <threshold>]

Starts a RTcfg server for the specified device <dev>. The server then sends
every 1000 ms stage 1 configuration frames to new clients. <period> (in
milliseconds) can be used to override the interval value. The number of
clients invited within one period is controlled by <burstrate> (default: 4).
This value also defines the number of stage 2 configuration fragments the
server should send as far as the client supports it (see also "announce").
<heartbeat> specifies the Heartbeat period of the clients in milliseconds
(default: 1000 ms), the value 0 turns the heartbeat mechanism off. <threshold>
sets the number of missing heartbeats after which a client shall be considered
dead (default: 2).

rtcfg <dev> add [ip|hw] <address> [-stage1 <stage1_file>]
      [-stage2 <stage2_file>]

Adds a client to the server's list of potential participants of the network
connected to the specified device <dev>. <address> can be either an IP address
(ip, A.B.C.D) or a physical address (hw, AA:BB:CC:DD:EE:FF). If no address
type is given, ip is assumed. Optionally, files can specified which will be
passed during the different configuration stages.

rtcfg <dev> del [ip|hw] <address>

Removes a client from the list of network participants. See above for details
about the parameter format.

rtcfg <dev> wait [-t <timeout>]

Waits until both configuration stages are completed for all clients in the
server's list. If <timeout> (in milliseconds) is given, rtifconfig will return
an error code when the configuration cannot be completed within the specified
time. The default timeout is infinite.



Client Commands
---------------

rtcfg <dev> client [-t <timeout>] [-f <stage1_file>]

Waits until the first configuration stage is completed for the device <dev>.
If <timeout> (in milliseconds) is given, rtifconfig will return an error code
when the configuration cannot be completed within the specified time. The
default timeout is infinite. The incoming configuration data is either send to
the standard output or to <stage1_file> if specified.

rtcfg <dev> announce [-t <timeout>] [-c|-f <stage2_file>] [-b burstrate]

Sends an New Announcement frame over the device <dev> and waits until this
second configuration stage is completed. If <timeout> (in milliseconds) is
given, rtifconfig will return an error code when the configuration cannot be
completed within the specified time. The default timeout is infinite. If -c or
-f is given, stage 2 configuration data is requested and either send to the
standard output or to <stage2_file>. <burstrate> controls the number of stage 2
configuration fragments the client should accept. The actual amount is
negotiated according to both the client's and the server's capability (see also
"server").



Example
-------

This examples demonstrates how RTcfg can be used to start a RTnet/RTmac
network. With the current version 0.5.0, only a common startup is possible.
Future discipline implementations will also support adding new stations (with
known addresses!) to the network during runtime. These implementation will
then benefit from the stage 1 configuration mechanism.


rtnetserver.sh:

echo "Starting RTnet (RTmac master/RTcfg server)..."
# basic setup
rtifconfig rteth0 up $MYIP 255.255.255.0
# ***how it may look like in the future...***
#rtifconfig rteth0 mac tdma2

# RTmac-specific stuff, depends on the discipline
# ***how it may look like in the future...***
#rtmacconfig_tdma2 rteth0 master 2000
#rtmacconfig_tdma2 rteth0 slot 0 -s 1500

# setup the RTcfg server
rtcfg rteth0 server
rtcfg rteth0 add 192.168.0.2 -stage1 client2-1.sh client2-2.sh
rtcfg rteth0 add 192.168.0.3 -stage1 client3-1.sh client3-2.sh
[...]

# wait for the configuration to complete
rtcfg rteth0 wait

# setup RTmac according to version 0.5.0 and earlier
# (without the delays which are currently required, will be fixed soon)
rtifconfig rteth0 mac master 2000
rtifconfig rteth0 mac add 192.168.0.2
rtifconfig rteth0 mac add 192.168.0.3
rtifconfig rteth0 mac up
rtifconfig rteth0 mac offset 192.168.0.2 200
rtifconfig rteth0 mac offset 192.168.0.2 300

echo "RTnet is running now."
EOF


client2-1.sh:

# RTmac-specific stuff, depends on the discipline
# ***how it may look like in the future...***
#rtmacconfig_tdma2 rteth0 slot 200 -s 1000
EOF


client2-2.sh:

echo "Hello, I'm client 192.168.0.2!"
EOF


client3-1.sh:

# RTmac-specific stuff, depends on the discipline
# ***how it may look like in the future...***
#rtmacconfig_tdma2 rteth0 slot 300 -s 1500
EOF


client3-2.sh:

echo "Hello, I'm client 192.168.0.3!"
EOF


rtnetclient.sh:

echo "Starting RTnet (client)..."
# basic setup
rtifconfig rteth0 up $MY_IP 255.255.255.0
# ***how it may look like in the future...***
#rtifconfig rteth0 mac tdma2

# wait for the first configuration stage
rtcfg rteth0 client -f /tmp/my_stage1_config

# run the stage 1 script
chmod u+x /tmp/my_stage1_config
/tmp/my_stage1_config

# announce myself and wait for the second stage
rtcfg rteth0 announce -f /tmp/my_stage2_config

# setup RTmac according to version 0.5.0 and earlier
rtifconfig rteth0 mac client

# run the stage 2 script
chmod u+x /tmp/my_stage2_config
/tmp/my_stage2_config

echo "RTnet is running now."
EOF


November 2003, Jan Kiszka <jan.kiszka-at-web.de>
