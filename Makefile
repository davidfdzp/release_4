# Makefile for MAC-MFTDMA simulator for ns-2.33

INSTALL = /usr/bin/install -c
STATIC_BUILD = yes

ifndef NSDIR
$(error "NSDIR unset! Please initialise it using: export NSDIR=<path_to_ns>")
endif


export CCOPT = -g -Wall -Wno-write-strings
#export CCOPT = -Wall -Wno-write-strings -Werror

#export LDFLAGS = -Wl,-export-dynamic 
export LDFLAGS = -Wl,-export-dynamic

export SRC_DIR = $(PWD)/src
export NS_TCL_LIB_STL = tcl/lib/ns-diffusion.tcl \
    tcl/delaybox/delaybox.tcl \
    tcl/packmime/packmime.tcl $(SRC_DIR)/defaults.tcl \
    $(SRC_DIR)/helpfun.tcl

export GEN_DIR = gen/
export OBJ_GEN = $(SRC_DIR)/version.o $(GEN_DIR)ns_tcl.o $(GEN_DIR)ptypes.o \
               $(SRC_DIR)/mac-allocator.o $(SRC_DIR)/mac-tdmadama.o \
               $(SRC_DIR)/mac-requester.o $(SRC_DIR)/atm-encap.o \
               $(SRC_DIR)/mpeg-encap.o $(SRC_DIR)/voicemodel.o \
               $(SRC_DIR)/rle-encap.o $(SRC_DIR)/mac-rle.o \
               $(SRC_DIR)/prio_fid.o $(SRC_DIR)/phy-tdmadama.o 

ifeq ($(STATIC_BUILD),yes)

all:
	$(MAKE) ns -e -C $(NSDIR)
	$(CC) utils/analy.c -o utils/analy
	$(CC) utils/fullrun.c -lpthread -lm -lconfig -Wall -o utils/fullrun

install: install-utils force
	$(MAKE) install -C $(NSDIR)

else

#
# This code allows the TDMA-DAMA code to be loaded dynamically.
# In order to use this code, ns2 must know which shared object to load.
# Then, the first time the tdmadama is compiled, prepare ns2 using 'make prepare'.
# Also, the tcl script for the simulation must load src/defaults.tcl explicitly 
# at the beginning 

export OBJ = $(OBJ_C) $(OBJ_GEN)
export LDFLAGS += -shared -fpic
export NS = libtdmadama.so

all: 
	$(MAKE) $(NS) -e -C $(NSDIR)	
	$(CC) utils/analy.c -o utils/analy
	$(CC) utils/fullrun.c -lpthread -lm -lconfig -Wall -o utils/fullrun

prepare:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) install
	$(MAKE) clean -C $(NSDIR)
	$(MAKE) ns -C $(NSDIR)
	$(RM) $(NSDIR)/ns
	$(MAKE) ns LDFLAGS='-Wl,-export-dynamic ~/lib/libtdmadama.so' -C $(NSDIR)
	$(MAKE) install -C $(NSDIR)


install: install-utils force
	$(INSTALL) -m 755 $(NSDIR)/$(NS) ~/lib


endif

install-utils: force
	$(INSTALL) -m 755 utils/analy ~/bin
	$(INSTALL) -m 755 utils/fullrun ~/bin	

clean:
	rm -rf src/*.o 
	touch src/defaults.tcl
	rm -rf utils/analy utils/fullrun

force:

%:;	$(MAKE) $@ -e -C $(NSDIR)

