# override with `make BUILD=release`
# default to release build
BUILD	  := release
CXX	  := g++
#CXX	  := clang
OUT_DIR	  := $(PWD)/$(BUILD)
OBJ_DIR	  := $(OUT_DIR)/obj
SAMPLES_OUT_DIR := $(OUT_DIR)/samples

CXX_FLAGS.debug     := -g3 -std=c++0x -Wall -pthread

CXX_FLAGS.release   := -std=c++0x -Wall -pthread

CXX_INC	  := -I./include/
CXX_INC	  += -I./csdk/stack/include
CXX_INC	  += -I./csdk/ocsocket/include
CXX_INC	  += -I./csdk/ocrandom/include
CXX_INC	  += -I./csdk/logger/include
CXX_INC	  += -I./csdk/libcoap

# Force metatargets to build:
.PHONY: prep_dirs c_sdk simpleserver simpleclient roomserver roomclient

all:	.PHONY

prep_dirs:
	-mkdir $(OUT_DIR)
	-mkdir $(OBJ_DIR)
	-mkdir $(SAMPLES_OUT_DIR)

c_sdk: 
	cd csdk && $(MAKE) "BUILD=$(BUILD)"

simpleserver: OCLib.a examples/simpleserver.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(SAMPLES_OUT_DIR)/$@ examples/simpleserver.cpp $(CXX_INC) $(OBJ_DIR)/OCLib.a csdk/$(BUILD)/liboctbstack.a

simpleclient: OCLib.a examples/simpleclient.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(SAMPLES_OUT_DIR)/$@ examples/simpleclient.cpp $(CXX_INC) $(OBJ_DIR)/OCLib.a csdk/$(BUILD)/liboctbstack.a

simpleclientserver: OCLib.a examples/simpleclientserver.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(SAMPLES_OUT_DIR)/$@ examples/simpleclientserver.cpp $(CXX_INC) $(OBJ_DIR)/OCLib.a csdk/$(BUILD)/liboctbstack.a

roomserver: OCLib.a examples/roomserver.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(SAMPLES_OUT_DIR)/$@ examples/roomserver.cpp $(CXX_INC) $(OBJ_DIR)/OCLib.a csdk/$(BUILD)/liboctbstack.a

roomclient: OCLib.a examples/roomclient.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(SAMPLES_OUT_DIR)/$@ examples/roomclient.cpp $(CXX_INC) $(OBJ_DIR)/OCLib.a csdk/$(BUILD)/liboctbstack.a

OCLib.a: OCPlatform.o OCResource.o OCReflect.o OCUtilities.o InProcServerWrapper.o InProcClientWrapper.o
	ar -cvq $(OBJ_DIR)/OCLib.a $(OBJ_DIR)/OCPlatform.o $(OBJ_DIR)/OCResource.o $(OBJ_DIR)/OCReflect.o $(OBJ_DIR)/OCUtilities.o $(OBJ_DIR)/InProcServerWrapper.o $(OBJ_DIR)/InProcClientWrapper.o

OCReflect.o: OCLib/OCReflect.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(OBJ_DIR)/$@ -c OCLib/OCReflect.cpp $(CXX_INC)

OCPlatform.o: OCLib/OCPlatform.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(OBJ_DIR)/$@ -c OCLib/OCPlatform.cpp $(CXX_INC)

OCResource.o: OCLib/OCResource.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(OBJ_DIR)/$@ -c OCLib/OCResource.cpp $(CXX_INC)

OCUtilities.o: OCLib/OCUtilities.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(OBJ_DIR)/$@ -c OCLib/OCUtilities.cpp $(CXX_INC)

InProcServerWrapper.o: OCLib/InProcServerWrapper.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(OBJ_DIR)/$@ -c OCLib/InProcServerWrapper.cpp $(CXX_INC)

InProcClientWrapper.o: OCLib/InProcClientWrapper.cpp
	$(CXX) $(CXX_FLAGS.$(BUILD)) -o $(OBJ_DIR)/$@ -c OCLib/InProcClientWrapper.cpp $(CXX_INC)

clean: clean_legacy
	-rm -rf release
	-rm -rf debug
	cd csdk && $(MAKE) clean
	cd csdk && $(MAKE) deepclean
clean_legacy:
	-rm -f -v $(OBJ_DIR)/OCLib.a $(OBJ_DIR)/*.o $(SAMPLES_OUT_DIR)/*
