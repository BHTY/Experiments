HELLDPMI
This is a very basic DPMI program built using Microsoft tools, showcasing the
most minimal design for a protected-mode application. There's no logic, it's
just a skeleton. It's built using the Microsoft Macro Assembler Version 6.11
(older versions potentially as far back as 5 should work, and possibly newer
too, but they need to be capable of outputting OMF objects) and Visual C++
1.1 32-Bit Edition for Windows NT (early CL386 versions can output OMF
objects too, but here we just have the compiler output assembly and then
assemble that). To build, have the environment variables for MASM and VC set
in your path, and then type "nmake". Ensure that the Microsoft Segmented
Executable Linker (comes with MASM) takes precedence over the PE/COFF linker
(comes with VC), as both are named LINK.EXE.

The application's memory model is small, with CS!=DS=SS, each with a nonzero 
base address. A totally flat memory model would be possible, but would require
an executable format (like PE/COFF) with relocations. If one wished to build
a DOS extender that loaded an executable in such a format, the loader stub
could be built out of HELLDPMI.

The upside of using entirely Microsoft tools and storing the executable in a
standard DOS MZ EXE is that you can take the MAP file output by LINK, feed it
into MAPSYM(32?), and get a SYM file that can be used in WDEB386 to debug your
application on Windows enhanced mode.

As built, HELLDPMI requires a resident DPMI server with a built-in DOS extender
(i.e. protected-mode INT 21H services), such as the Windows virtual machine
manager. If you were building a DOS extender yourself, you'd obviously remove
the INT 21H calls made in protected-mode, but keep the rest of the skeleton.
The only INT 21H use in protected-mode (apart from the termination call, which
is mandated by the DPMI spec) is for the hello world message, so its purpose
is purely illustrative. 

The purpose of HELLDPMI is twofold. First of all, it serves as a base for any
potential DPMI projects that you or I may wish to do (I'm planning to write
the C4 DOS Extender). Moreover, however, it serves as a proof of concept that 
Microsoft was shipping the tools to write 32-bit DPMI applications all along. 
It wasn't just with Phar Lap TNT (or even their 386 DOS extender which could 
link COFF objects from the MSC32 compiler into its EXP executables); you could
build them even then, with a bit of jury-rigging. Obviously, a "real" 32-bit 
executable format with relocations (to allow it to be loaded at an arbitrary 
linear base address but still work fine in a flat model) is preferable, so the 
C4 extender will include a PE/COFF dynamic loader in addition to the exception 
handlers and DOS extension APIs that it installs.

An older version of HELLDPMI uses a tiny memory model where CS=DS!=SS, which
causes problems as C code wanted to place data into the _DATA segment which
was distinct from the _TEXT segment. 


Write monitor components of DDT32 first; no symbols or expression parsing at first
Table-based assembler & disassembler
