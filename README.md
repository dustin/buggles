# OSS RC RX

I've used a few clones of [FrSky][frsky] RXes, but the software
support was never to my liking.  There's the [Airwolf][airwolf] RXes
from Banggood which by default output either 8 channels of PWM or
CPPM.

Other builds of firmware exist for these, but you have to choose from
a variety of builds from a closed source project discussed in a forum
somewhere.  I wanted to do my own things.

I'd built an [OpenSky][opensky] RX for a project in the past, but the
range was pretty bad.  I attributed a lot of this to the chip antenna
that had to be embedded in my aircraft.  I found
[a new board][newboard] that didn't have a chip antenna and played
with that a bit on a breadboard.  It seems good enough, so I decided
to have fun with it.

# Goals

I had a few goals with this project.  Mostly, I just wanted to learn
some stuff.   I did.  That was nice.  But practically, I wanted to:

* See if I could make the [radio I found][newboard] work.
* Have a [good protocol][sumd] for the RX -> FC communication.
* Get basic useful things like RSSI for my OSDs.
* Easier BINDing (if you're not bound, just BIND).

One of the builds of the "Airwolf" radios has s.bus output, but this
is not a particularly desirable protocol for numerous reasons
(e.g. having an actual CRC on data is nice even when you're using a
hardware UART, which isn't wired up on the Airwolf).  Even if it were,
I still want my RSSI injection an stuff.

# Current State

The code is the minimal to reliably do what I want.  I've tested it
with the hardware SPI from an ATMega 328P connected to a CC2500 and
the hardware UART transmitting SUMD.  The CC2500 I'm using doesn't
have a GDO pin, so I'm using GD1 connected to the SCK pin of the AVR.
I'm assuming it'd work fine if you also had a GDO (with a different
build config), but I've not tried.

I'll probably try to get it running on an Airwolf just because, but I
just fried the vreg on mine, so maybe not tonight.  In order to do
that, I need a software implementation of SPI (which is silly on
hardware that does SPI) and SoftSerial.  Neither should be a huge
challenge.

I could probably also do an ATTiny85 variant.  It is currently using
two timers, but that's not strictly necessary.

On the topic of timers, I did vary (optionally) from the SUMD spec in
such a way as to transmit packets approximately every 9ms instead of
every 10.  This allows me to deliver values to your flight controller
immediately upon receiving them over the radio for the lowest latency
possible.

Example:

![SUMD Oneshot](https://usercontent.irccloud-cdn.com/file/s1Hdcx0L/SUMD)

The teal line in the above (ch2) was getting a pulse every time the
radio received a valid packet.  The yellow line (ch1) is SUMD packets
being spat out.

Note the packets are pretty wide -- this was 19 channels -- 8 channels
of the radio, one channel of RSSI, and then 10 debug channels
reporting a histogram of packet loss during a test.  Non-debug builds
will produce smaller packets, but in any case, the latency is the
same.  About every 9ms, you'll have fresh data.

[frsky]: http://www.frsky-rc.com/
[airwolf]: http://www.banggood.com/DIY-FRSKY-8CH-Receiver-RX-PPM-Output-For-X9DPLUS-XJT-DJT-DFT-DHT-p-987247.html
[opensky]: https://github.com/readerror67/OpenSky
[newboard]: http://www.aliexpress.com/item/CC2500-SPI-2-4GHz-Wireless-Transceiver-Module/32610339676.html
[sumd]: http://www.deviationtx.com/media/kunena/attachments/98/HoTT-SUMD-Spec-REV01-12062012-pdf.pdf
