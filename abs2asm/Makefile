#++
# Makefile - Makefile for abs2asm
#
#DESCRIPTION:
#   This is a fairly simple Makefile for building the abs2asm utility.  This
# program converts a PDP-11 absolute loader file into an assembly source file
# (no, it doesn't disassemble - sorry!) for the Spare Time Gizmos SBCT11 single
# board computer.
#
#                                   Bob Armstrong [25-JAN-24]
#
#TARGETS:
#  make abs2asm - rebuild abs2asm
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 25-JAN-25	RLA	New file.
#--

# Define the target (library) and source files required ...
TARGET    = abs2asm
CSRCS	  = abs2asm.c
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
