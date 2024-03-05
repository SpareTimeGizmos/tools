# PromICE
   This program communicates with a Grammar Engine PromICE EPROM emulator.
It can download files to the emulator, test the PromICE memory, reset the
attached target device, and more.  

   Yes, Grammer Engine does have a LOADICE program which already does all
this and more.  I couldn't find a Linux port though, and that inspired me
to write this one.

## USAGE:
       promice [global options] [command] [local options] [file0] [file1]
     
       Global options applicable to all commands
         -p port     -> set COM port
         -b baud     -> set serial baud rate
     
       Commands (only one may appear!)
         v[erify]    -> verify communication with PromICE
         r[eset]     -> reset target without downloading
         t[est]      -> execute self test
         d[ownload]  -> download file(s)
     
       Local options for downloading only
         -f dd       -> fill unused locations with dd
         -v          -> verify after downloading
         -s size     -> set size of emulated device

## ENVIRONMENT VARIABLES
	PROMICE_PORT  -> set the default serial port to use
	PROMICE_BAUD  -> set the default serial baud rate

## NOTES
  In the case of a dual, master/slave, PromICE then two file names can be
specified for download.  The first file is downloaded to unit 0 (that's the
master unit, or the bottom connector on the back panel) and the second file
is downloaded to unit 1 (the slave, or upper connector on the back).  This
program assumes that both emulated EPROMs are the same size, although I don't
think the actual PromICE hardware requires that.
