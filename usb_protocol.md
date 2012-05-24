Communicating with iQue Player
==============================

This document describes the USB communication protocol with the iQue
Player.

Author: Mike Ryan

Email: mikeryan (ta) lacklustre.net

Version: 2012-05-23_1

The latest version can always be found at: http://lacklustre.net/n64/ique/

Communication Protocol
======================

All communication with the iQue Player is encapsulated in Transfer
Units (TUs). A TU received from the device will always be 4 bytes. A TU
sent to the device will be 4 bytes in most cases, though the final bytes
may be omitted in some cases.

The first byte of a TU encodes its type. The types that are sent to the
device and received from the device are disjoint.

commands
--------

    00000000: 43 00 00 00 43 11 00 00 42 0f e5

 - 43: three bytes of data follow
 - 42: two bytes follow
 - 41: one byte follows (unseen but presumed)

Commands are 4 bytes, followed by a 4 byte argument.

Known commands:

    command                        args
    00 00 00 11     read block      block number
    00 00 00 17     get status      none?
    00 00 00 1d     set LED         2 == on, anything else == off

The following commands are issued by the diagnostic utility when the
connect command is issued. Their function is unknown.

    00 00 00 12
    00 00 00 15
    00 00 00 16

In all cases the reply is 8 bytes. Sample replies:

    12: 00 00 00 ed 00 00 00 00
    15: 00 00 00 ea 00 00 00 00
    16: 00 00 00 e9 00 00 00 00

replies
-------

Replies follow commands. The first transfer unit is type 1b and has a
three byte body that specifies length. Example:

    1b 00 10 00

This specifies that a data chunk of 0x1000 bytes follows. This is the
length of the raw data, excluding the TU type indicator.

Data is encapsulated in TUs whose type field is 1f, 1e, or 1d.

 - 1f: 3 bytes of data
 - 1d: 2 bytes of data
 - 1e: 1 byte of data

After all data has been returned, a final transfer unit of type 15
follows:

    15 00 00 00

The transfer should be ACKd at this point by sending 44.





scratch starts below


file xfer reply
===============

Files are transferred in 4096 byte blocks. A block is made up of many
transfer units.

    00000000: 1b 00 10 00 1f fd 21 00 1f 00 17 10 1f b1 4f 49
             size ------^    ^----- content
    00000010: ...

Transfer units are 4 bytes long. Each unit has a 1 byte type followed
by 3 bytes of data:

    tt dd dd dd

Types:

 - 1b: block size (always 0x1000 / 4096)
 - 1f: data bytes
 - 1e: two valid bytes in this unit
 - 1d: one valid byte in this unit

Transfer requests
=================

private

 - depot.sys
 - inode 4
 - block 0x0fe5 (4079)

Details:

    >  00000000: 43 00 00 00 43 17 00 00 42 00 00
     < 00000000: 1b 00 00 08
     < 00000000: 1f 00 00 00 1f e8 00 00 1e 00 01 00 15 00 00 00
    >  00000000: 44

    >  00000000: 43 00 00 00 43 17 00 00 42 00 00
     < 00000000: 1b 00 00 08
     < 00000000: 1f 00 00 00 1f e8 00 00 1e 00 01 00 15 00 00 00
    >  00000000: 44

    >  00000000: 43 00 00 00 43 11 00 00 42 0f ec
     < 00000000: 1b 00 00 08
     < 00000000: 1f 00 00 00 1f ee 00 00 1e 00 00 00
    >  00000000: 44

    followed by data..

identity

 - id.sys
 - inode 5
 - block 0x0fe4 (4078)

Details:

    00000000: 43 00 00 00 43 17 00 00 42 00 00
    00000000: 43 00 00 00 43 11 00 00 42 0f ee

ticket
 - ticket.sys
 - inode 14
 - blocks 0x0f3e, 0x075d, 0x075c, 0x075b
   these aren't contiguous, maybe because we added zelda after the fact?

Details:

    not saved, derp

cert
 - cert.sys
 - inode 7
 - block 0x0fe3 (4077)

Details:

    00000000: 43 00 00 00 43 17 00 00 42 00 00
    00000000: 43 00 00 00 43 17 00 00 42 00 00
    00000000: 43 00 00 00 43 11 00 00 42 0f e5
             11, not 17  -----^          ^-- block number

LED
---

Get LED status:


    >  00000000: 43 00 00 00 43 17 00 00 42 00 00
     < 00000000: 1b 00 00 08
     < 00000000: 1f 00 00 00 1f e8 00 00 1e 00 01 00 15 00 00 00
    >  00000000: 44

Turn on LED:

    >  00000000: 43 00 00 00 43 17 00 00 42 00 00
     < 00000000: 1b 00 00 08
     < 00000000: 1f 00 00 00 1f e8 00 00 1e 00 01 00 15 00 00 00
    >  00000000: 44

    >  00000000: 43 00 00 00 43 1d 00 00 42 00 02
     < 00000000: 1b 00 00 08
     < 00000000: 1f 00 00 00 1f e2 00 00 1e 00 00 00 15 00 00 00
    >  00000000: 44

command is 5th byte in sent packets:

 - 17 get LED status
 - 1d set LED

final byte (byte 0xa) in command is on/off. 02 is on, 00 is off

byte 5 in second reply packet differs: e8 vs e2


File System Structure
=====================

inode table is at block 0x0fff

list of all files
-----------------

from Marsh's iQue

    inode 0    recrypt.sys        16384    16.0 KB
    inode 1    003e93f1.app    11829248    11.3 MB
    inode 2    004dd630.app     8159232     7.8 MB
    inode 3    0010cd30.app     8060928     7.7 MB
    inode 4    depot.sys          16384    16.0 KB
    inode 5    id.sys             16384    16.0 KB
    inode 6    00200f70.rec    29868032    28.5 MB
    inode 7    cert.sys           16384    16.0 KB
    inode 8    00200f70.sta       32768    32.0 KB
    inode 9    005d1870.sta       16384    16.0 KB
    inode 10   crl.sys            16384    16.0 KB
    inode 12   005d1870.rec     3358720     3.2 MB
    inode 13   sig.db             16384    16.0 KB
    inode 14   ticket.sys         65536    64.0 KB

TODO
====

 - document status reply
 - writing files
 - deleting files
