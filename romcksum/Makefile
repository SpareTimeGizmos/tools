#++
# Makefile - Makefile for romcksum
#
#DESCRIPTION:
#   This is a fairly simple Makefile for building the romcksum utility.
# This program is used by several different Spare Time Gizmos projects to add
# a checksum to an EPROM image.
#
#                                   Bob Armstrong [25-JAN-24]
#
#TARGETS:
#  make romcksum - rebuild romcksum
#  make clean	 - delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 25-JAN-25	RLA	New file.
#--

# Define the target (library) and source files required ...
TARGET    = romcksum
CSRCS	  = romcksum.c
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
