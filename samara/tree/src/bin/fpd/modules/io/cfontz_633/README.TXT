Crystalfontz CFA-631, CFA-633 & CFA-635 packet based
communications test/example program for Linux.


==================
Basic Installation
==================

   These are generic installation instructions:
   (Note that gcc v3.0 or later, make and their supporting libraries
    are required for this utility to work.)

     1. Copy the 633_linux.tar into the directory that you want it
        installed to (i.e. /usr/local/crystalfontz/) and 
        untar the file (i.e. tar -vf 633_linux.tar). 

     2. 'cd' into the 633_linux folder and 'make' to compile and
        create the 'test633' binary.
     
     3. Run 'test633' as root, using the following format:
        ./test633 PORT BAUD
        PORT is something like "/dev/ttyS0" or "/dev/usb/ttyUSB0"
        BAUD is 19200 or 115200
        To clear the display, enter "clear" as an optional third parameter
    
    The screen output should show something similar to the following:

                ----------------------
                -  >This is line 1<  -
                -  >This is line 2<  -
                ----------------------


==================   
Notes on Baud Rate
==================

   This utility may be used to test the 631, 633 and 635 displays.

   BAUD rates for the modules are as follows:
      631 USB: 115200
      633 USB/Serial: 19200 (default) or 115200 (FW v1.4 and beyond)
      635 USB: 115200


==================
Make options
==================

   If you modify the existing code and wish to do a fresh make, run
   'make clean' and then 'make' again. 


==================
Crystalfontz America, Incorporated
15611 East Washington Road
Valleyford, WA 99036-8523
sales@crystalfontz.com http://www.crystalfontz.com
voice (509) 291-3514 fax (509) 291-3345 US toll-free (888) 206-9720
==================
