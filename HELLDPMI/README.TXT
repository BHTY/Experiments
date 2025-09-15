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

The application's memory model is tiny, with CS=DS, but with a nonzero base
address. A totally flat memory model would be possible, but would require
an executable format (like PE/COFF) with relocations. If one wished to build
a DOS extender that loaded an executable in such a format, the loader stub
could be built out of HELLDPMI.

Note that some convincing may be needed to get CL386 to place variables into
the _TEXT segment. Remember that CL386 is oriented towards a flat memory
model where CS=DS. That is the model we're dealing with, so any C code can
access data from an assembly module, but the compiler will output assembly
placing any data into the data segment. There are three potential remedies
for this:
1.) Just accept this as the state of affairs, meaning that we'd need to 
allocate a selector for _DATA and fiddle with segment registers to swap
between the assembly code's data and the C code's data
2.) Similar to #1, but put all 32-bit data in _DATA. This has the side
effect of CS no longer equalling DS, as DS now needs to point to _DATA,
which could cause further problems down the line
3.) Some kind of preprocessor run over the assembly output from CL386 to
put any data into the _TEXT segment

A lot of things (C-style varargs) would probably be easier if SS=DS too, 
but right now HELLDPMI doesn't do that. That is probably easily fixed by
just reserving a region at the end of the data segment for the stack.

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


Write monitor components of DDT32 first; no symbols or expression parsing at first
Table-based assembler & disassembler
