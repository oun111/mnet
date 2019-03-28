
#include ./Makefile.inc


REL_FILE        := RELEASE

PAYSVR_DIST_PATH:= ../dist

.PHONY: all
all:  
	$(MAKE) -C src all


.PHONY: clean
clean:  
	$(MAKE) -C src clean

.PHONY: distclean
distclean:
	$(MAKE) -C src distclean
	rm -rf cscope.* 
	rm -rf tags $(PAYSVR_DIST_PATH)

dist:
	mkdir -p  $(PAYSVR_DIST_PATH)
	cp src/mnet_test $(PAYSVR_DIST_PATH)
	cp ./conf/paysvr_conf.json $(PAYSVR_DIST_PATH)
	cp ./modules/*.dlo $(PAYSVR_DIST_PATH)
