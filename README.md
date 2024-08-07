# rpcemu-morphos
MorphOS port of RPCemu

This is my hack. I needed to run RISC OS on MorphOS so I did this quick port.
I don't load any config file, and the executable takes no arguments. If you need
IDE disc support there is a line with hd4.hdf in src/ide.c . This only work with
RISC OS 3 or 4. The easiest way to get the emulator running is to download a RiscPC 
ROM from https://www.riscosopen.org/.
The emulator expects roms to be in a directory called "roms" in the same directory
as the executable. The rom needs to be named "riscos.rom". The emulator defaults 
hostfs to "hostfs" directory. Due to something filesystem related writes fail with hostfs.
Read is OK. You can try downloading the HardDisc4 zip from above address.

The biggest problem besides all that just is missing, is the RiscPC Timer0.
It need to fire each centisecond. Right now I am using pthreads, for that and
for video. I have tried several ways without any luck.


The main site for RPCEmu where you can find the official sources and many guides.

https://www.marutan.net/rpcemu/index.php

Feel free to improve!
