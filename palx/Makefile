#++
# Makefile - Makefile for the PALX IM6100/HD6120 cross assembler ...
#
#DESCRIPTION:
#   This is a fairly simple Makefile for building the Spare Time Gizmos PALX
# IM6100/HD6120/PDP-8 cross assembler on Debian Linux and using GNU Make and
# gcc.
#
#
#                                   Bob Armstrong [15-MAR-23]
#
#TARGETS:
#  make palx	- rebuild PALX
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 15-MAR-23	RLA	New file.
#--

# Compiler preprocessor DEFINEs for the entire project ...
DEFINES = _DEBUG


# Define the target (library) and source files required ...
TARGET    = palx
CSRCS	  = palx.c
INCLUDES  = 
OBJECTS   = $(CSRCS:.c=.o)
LIBRARIES = 


# Define the standard tool paths and options.
CC       = /usr/bin/gcc
LD       = $(CC)
CCFLAGS  = -std=c11 -ggdb3 -O3 -pthread -Wall -Wno-deprecated-declarations \
           -funsigned-char -funsigned-bitfields -fshort-enums \
	    $(foreach inc,$(INCLUDES),-I$(inc)) \
	    $(foreach def,$(DEFINES),-D$(def))
LDFLAGS  = 


# Rule to rebuild the executable ...
all:		$(TARGET)


$(TARGET):	$(OBJECTS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBRARIES)


# Rules to compile C files ...
%.o: %.c
	$(CC) -c $(CCFLAGS) -o $@ $<

# A rule to clean up ...
clean:
	rm -f $(TARGET) $(OBJECTS) *~ *.core core Makefile.dep


# And a rule to rebuild the dependencies ...
Makefile.dep: $(CSRCS)
	@echo Building dependencies
	@$(CC)  -M $(CCFLAGS) $(CSRCS) >Makefile.dep

include Makefile.dep
