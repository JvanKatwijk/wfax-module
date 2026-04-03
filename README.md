
--------------------------------------------------------------------------
		SDRconnect weatherFAX module
-------------------------------------------------------------------------

The "fax" module for sdrConnect is a module for decoding weatherfax signals
transmitted on shortwave.

![overview](/wfax-example-1.png?raw=true)

The *weatherfax* (wfax) module is, like other SDRconnect modules, 
a separate program that uses a network connection to
communicate with sdrConnect.
The program uses a samplerate of 2000000 Samples/second,
it filters the band where the transmission takes place and decimates
the input stream to 12000 Samples/second.

**Note that the communication between the program and sdrConnect uses a 2M
connection, so working remote using WiFi (or so) most likely does not work.**

----------------------------------------------------------------------------
---------------------------------------------------------------------------
![main program](/fax-control.png?raw=true)

The "main window" of the program contains the controls.
The **top line** is devoted to the connection with sdrControl.
 * the second element is for specifying the host for the connection, here "127.0.0.1", indicating that the the fax program and sdrControl run on the same PC;
 * the third element specifies the port to be used for the connection, the default value for the connection is port 5454;
 * if reasonable values are put in for host and port, touching the button
labeled **connect** starts the conenction sequence.
 * if a connection could be made, the *label* at the right end of the top line
indicates (as shown here) **connected**.

Once a connection is made, one might select a frequency. While sdrConnect
provides lots of means to set and alter the frequency, lots of fax transmissions
are on predefined frequencies.
Therefore, a small list of **presets** is available, touching the presets button
shows the list

![main program](/presets.png?raw=true)

Selecting a frequency is simply by clicking with the mouse cursor
set on the selected frequency.

Now, I live in the Netherlands and the frequencies are one that - with my
simple means - I can receive. In other parts of the world, there are obviously
other frequencies for fax transmissions.
Therefore, the list can be modified
 * **double** clicking on an element in the list with the mouse removed the elements from the list;
 * **touching** the button labeled **save freq** will add the currently 
selected frequency to the list.

Basic operation is then - after selecting the frequency - waiting until a
transmssion starts. The indicator on the window - SYNCED in the picture above -
tells APTSTART when waiting for the detection of a transmission.
A transmission (in the default Wefax576 mode) starts with sending a tone of
300Hz during 5 seconds. The implemented tone decoder detects the tone, next
for a period of about 10 seconds **phasing lines** are transmitted, 
lines with at the start and the end the encoding for **white** and in the
middle the encoding for **black**.
During the detection of these phase lines the indicator on the window
will state "PHASING".
In the picture above the phasing lines are visible.

After being convinced that the phasing signal was indeed phasing, the 
software switched to state SYNCED, and the decoding of the actual picture
starts.

Now be aware that for mode Wefax576, the picture with is 576 * M_PI, which
amounts to 1809 pixels.
The transmission rate is 2 lines per second, and including the overhead
at the start (i.e.phasing lines and some white lines), while the picture
itself is 1200 lines, the receiver counts over 1300 lines.
So transmission of a single picture takes over 10 minutes.

Anyway. at the end of the picture data, a tone (450 hz) is transmitted,
again for a few seconds. The tine decoder in the fax software detects this
and causes a switch to state FAX_DONE.

In state FAX_DONE mode, the software will wait until the user takes action.
 * touching the **saveSingle** button instructs the software to save the image in a picture file;
 * touching the ""continue** button instructs the software to switch over to
the waiting state APTSTART.

The **cheat** button:
 * if in state APTSTART the software switches to state SYNCED and decodes
data. Of course, lines most likely do not start at the beginning of the picture; * if in state SYNCED the software switches to state FAX_DONE.

The **reset** button will do what the name suggests

![example](/example-fax.png?raw=true)

While usually annotated maps are transmitted, sometimes schedules etc are
in the map.

-----------------------------------------------------------------------
weatherfax
-----------------------------------------------------------------------

The frequencies on which weathercharts are transmitted are to be found in
e.g.

	https://www.weather.gov/media/marine/rfax.pdf


