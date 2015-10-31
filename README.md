#A Little Atari 810 Disk Drive

**A tiny working model of a retro computing icon offers a blend of nostalgia and sillyness.**

My first computer was an Atari 400. My first disk drive was the magnificent Atari 810. Overwhelmed by a recent wave of nostalgia from playing Zork for the first time in 30 years I have built a working model of an Atari 810 that uses 8Gig microSD cards instead of 5 1/4 inch floppies to emulate up to 8 drives. Maintaining the relative dimensions of drive to media, the model is somewhat smaller than the original.

![](http://rossumblog.files.wordpress.com/2011/05/booting-scaled1000.jpg)
![](http://rossumblog.files.wordpress.com/2011/05/ontop-scaled1000.jpg)
![](http://rossumblog.files.wordpress.com/2011/05/relativesizes-scaled1000.jpg)

The original 810 managed 90k per disk and had a volume of about 30,000 cm3. Assuming a 8Gig card the new version can store about 90,000 disks and at 5 cm3 only takes up 0.000167 times as much space. So it is a lot bigger and a lot smaller. Progress eh?

###How it works
The Atari 810 (and subsequent 1050) drives are “smart” serial peripherals. The Atari OS communicates drives, printers, modems and devices using the SIO (“Serial Input/Output”) protocol. SIO devices connected to the Atari with a chunky 13 pin connector.

![](http://rossumblog.files.wordpress.com/2011/05/sio-scaled1000.png)

###Sio
The Atari addresses 5 bytes commands to peripherals by lowering pin 7 (Command) and transmitting 19200 baud asynch serial on pin 5 (Data Out). If the peripheral recognises the command (‘1′,’R’,1,0 for a read from disk 1, for example) it acknowleges the command and responds with data on pin 3 (Data In) at a pokey 19200 baud by default. Pin 10 can supply 50ma of 5v.
![](http://rossumblog.files.wordpress.com/2011/05/sioread-scaled1000.png)

###Hardware
The hardware is pretty simple: a LPC1114 microcontroller, a microSD slot, a 3v3 regulator, a led and some caps. I used the 1114 because they are cheap and I had them lying around after building the Wikipedia reader: just about any 3v3 micro with SPI and a UART would also work fine. 

![](https://rossumblog.files.wordpress.com/2011/05/schematic-scaled1000.png)
![](http://rossumblog.files.wordpress.com/2011/05/boardandcase-scaled1000.jpg)
![](http://rossumblog.files.wordpress.com/2011/05/beforepainting-scaled1000.jpg)

The enclosure is a 3D print from Shapeways. This is the first time I have used them and I have to say I was delighted bt the experience. My inexperience in 3D modeling is evident but Shapeways sent me a lovely collection of little enclosures in various materials. I tried to make it as small as possible and still accomodate the microSD card. Testors enamel completed the look (make sure you mix in some olive with the light tan and cream).
![](http://rossumblog.files.wordpress.com/2011/05/model-scaled1000.jpg)

##Software

The microcontroller code emulates up to 8 Atari drives. At power on it checks for a microSD card, mounts a Fat16 or Fat32 file system and scans the card for .ATR and .XFD disk image files commonly used with Atari emulators. It also looks for XEX files which are Atari executables, another emulator mainstay. The code then “inserts”  the BOOT.RUR image into drive 1 and waits for the Atari to start sending commands during bootup.

![](http://rossumblog.files.wordpress.com/2011/05/incontext-scaled1000.jpg)

BOOT.RUR is a UI app written in C and compiled with cc65. Because the drive has no input or display we use this app running on the Atari to select disk images or applications. Cursor keys select the image or xex application, moving off the left or the right edge of the screen will page the list. Keys ‘1’ …’8′ will insert the selected item into drives ‘1’..’8′. The space bar will eject the disk, the return key will boot the selected image or xex. If an xex is selected, the firmware will synthesize a kboot disk image to load and execute it.

![](http://rossumblog.files.wordpress.com/2011/05/screen-scaled1000.jpg)

It is electrically possible to write an Atari app that can reflash the firmware but so far this is left as an exercise to the reader. The board pinout happens to be the same as a FTDI serial cable so inital firmware downloads can happen through FlashMagic.

As noted earlier, 19200 baud is pretty pokey by modern standards. Other drive and dos vendors came up with various tricks to make the serial run at 38400 or 57600 or beyond. This board supports a common 57600 baud mode so if you tire of the nostalgic thweep-thweep-thweep you can speed things up a bit by using MyPicodos or similar.

There are lots of good commercial and open source emulators and serial cables that are much more complete (and a little more practical) than this one. I am amazed at the continuing innovation on this platform: check out Yoomp and Crownland and compare them to titles released in the 80s.

