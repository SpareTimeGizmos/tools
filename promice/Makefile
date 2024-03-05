#++
# Makefile - Makefile for the Grammar Engine PromICE utility ...
#
#   COPYRIGHT (C) 2023 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
#
# LICENSE:
#    This file is part of the PromICE project.  PromICE is free software; you
# may redistribute it and/or modify it under the terms of the GNU Affero
# General Public License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.
#
#    PromICE is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
# details.  You should have received a copy of the GNU Affero General Public
# License along with PromICE.  If not, see http://www.gnu.org/licenses/.
#
# DESCRIPTION:
#   This is a fairly simple Makefile for building the Spare Time Gizmos PromICE
# utility on Debian Linux and using GNU Make and gcc.  PromICE talks to the
# Grammar Engine PromICE emulators and can download images.
#
#
#                                   Bob Armstrong [6-APR-23]
#
#TARGETS:
#  make PromICE	- rebuild PromICE
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
#  6-APR-23	RLA	New file.
#--

# Compiler preprocessor DEFINEs for the entire project ...
DEFINES = _DEBUG


# Define the target (library) and source files required ...
TARGET    = PromICE
CSRCS	  = PromICE.c hexfile.c protocol.c serial.c
INCLUDES  = 
OBJECTS   = $(CSRCS:.c=.o)
LIBRARIES = 


# Define the standard tool paths and options.
CC       = /usr/bin/gcc
LD       = $(CC)
CCFLAGS  = -std=gnu11 -ggdb3 -O3 -pthread -Wall -Wno-deprecated-declarations \
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
