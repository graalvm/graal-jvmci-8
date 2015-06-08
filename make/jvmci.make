# This Makefile is generated automatically, do not edit
# This file was built with the command: mx.sh makefile -o make/jvmci.make

TARGET=.
# Bootstrap JDK to be used (for javac and jar)
ABS_BOOTDIR=

JAVAC=$(ABS_BOOTDIR)/bin/javac -g -target 1.8
JAR=$(ABS_BOOTDIR)/bin/jar

EXPORTED_FILES_ADDITIONAL=$(TARGET)/options $(TARGET)/services
HS_COMMON_SRC=.

# Directories, where the generated property-files reside within the JAR files
PROVIDERS_INF=/META-INF/providers/
SERVICES_INF=/META-INF/services/
OPTIONS_INF=/META-INF/options/

ifeq ($(ABS_BOOTDIR),)
    $(error Variable ABS_BOOTDIR must be set to a JDK installation.)
endif
ifneq ($(MAKE_VERBOSE),)
    SHELL=sh -x
endif

define process_options
    $(eval providers=$(1)/$(PROVIDERS_INF))
    $(eval services=$(1)/$(SERVICES_INF))
    $(eval options=$(1)/$(OPTIONS_INF))
    test -d $(services) || mkdir -p $(services)
    test ! -d $(providers) || (cd $(providers) && for i in $$(ls); do c=$$(cat $$i); echo $$i >> $(abspath $(services))/$$c; rm $$i; done)

    # Since all projects are built together with one javac call we cannot determine
    # which project contains HotSpotVMConfig.inline.hpp so we hardcode it.
    $(eval vmconfig=$(1)/hotspot/HotSpotVMConfig.inline.hpp)
    $(eval vmconfigDest=$(HS_COMMON_SRC)/../graal/com.oracle.jvmci.hotspot/src_gen/hotspot)
    test ! -f $(vmconfig) || (mkdir -p $(vmconfigDest) && cp $(vmconfig) $(vmconfigDest))
endef

define extract
    $(eval TMP := $(shell mktemp -d $(1)_XXXXX))
    mkdir -p $(2);
    cd $(TMP) && $(JAR) xf $(abspath $(1)) &&         ((test ! -d .$(SERVICES_INF) || cp -r .$(SERVICES_INF) $(abspath $(2))) &&  (test ! -d .$(OPTIONS_INF) || cp -r .$(OPTIONS_INF) $(abspath $(2))));
    rm -r $(TMP);
    cp $(1) $(2);
endef


all: default

export: all
	mkdir -p $(EXPORT_DIR)
	$(foreach export,$(EXPORTED_FILES),$(call extract,$(export),$(EXPORT_DIR)))
.PHONY: export



JDK_BOOTCLASSPATH = $(ABS_BOOTDIR)/jre/lib/resources.jar:$(ABS_BOOTDIR)/jre/lib/rt.jar:$(ABS_BOOTDIR)/jre/lib/jsse.jar:$(ABS_BOOTDIR)/jre/lib/jce.jar:$(ABS_BOOTDIR)/jre/lib/charsets.jar:$(ABS_BOOTDIR)/jre/lib/jfr.jar

JVMCI_OPTIONS_PROCESSOR_SRC = $(shell find graal/com.oracle.jvmci.options/src -type f -name *.java 2> /dev/null)
JVMCI_OPTIONS_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.options.processor/src -type f -name *.java 2> /dev/null)

JVMCI_OPTIONS_PROCESSOR_JAR = $(TARGET)/graal/com.oracle.jvmci.options.processor/ap/com.oracle.jvmci.options.processor.jar

JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC = $(shell find graal/com.oracle.jvmci.hotspotvmconfig/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.common/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.hotspotvmconfig.processor/src -type f -name *.java 2> /dev/null)

JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR = $(TARGET)/graal/com.oracle.jvmci.hotspotvmconfig.processor/ap/com.oracle.jvmci.hotspotvmconfig.processor.jar

JVMCI_SERVICE_PROCESSOR_SRC = $(shell find graal/com.oracle.jvmci.service/src -type f -name *.java 2> /dev/null)
JVMCI_SERVICE_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.service.processor/src -type f -name *.java 2> /dev/null)

JVMCI_SERVICE_PROCESSOR_JAR = $(TARGET)/graal/com.oracle.jvmci.service.processor/ap/com.oracle.jvmci.service.processor.jar

JVMCI_API_SRC = $(shell find graal/com.oracle.jvmci.meta/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.code/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.runtime/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.options/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.common/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.debug/src -type f -name *.java 2> /dev/null)

JVMCI_API_JAR = $(TARGET)/build/jvmci-api.jar

JVMCI_API_DEP_JARS = $(TARGET)/build/jvmci-service.jar graal/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_API_JAR)

JVMCI_SERVICE_SRC = $(shell find graal/com.oracle.jvmci.service/src -type f -name *.java 2> /dev/null)

JVMCI_SERVICE_JAR = $(TARGET)/build/jvmci-service.jar

JVMCI_SERVICE_DEP_JARS = graal/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_SERVICE_JAR)

JVMCI_HOTSPOT_SRC = $(shell find graal/com.oracle.jvmci.hotspotvmconfig/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspotvmconfig/graal/com.oracle.jvmci.hotspotvmconfig/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.amd64/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.amd64/graal/com.oracle.jvmci.amd64/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.compiler/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.compiler/graal/com.oracle.jvmci.compiler/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot/graal/com.oracle.jvmci.hotspot/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot.amd64/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot.amd64/graal/com.oracle.jvmci.hotspot.amd64/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.sparc/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.sparc/graal/com.oracle.jvmci.sparc/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot.sparc/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot.sparc/graal/com.oracle.jvmci.hotspot.sparc/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot.jfr/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot.jfr/graal/com.oracle.jvmci.hotspot.jfr/src_gen -type f -name *.java 2> /dev/null)

JVMCI_HOTSPOT_JAR = $(TARGET)/build/jvmci-hotspot.jar

JVMCI_HOTSPOT_DEP_JARS = $(TARGET)/build/jvmci-api.jar $(TARGET)/build/jvmci-service.jar graal/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_HOTSPOT_JAR)

$(JVMCI_OPTIONS_PROCESSOR_JAR): $(JVMCI_OPTIONS_PROCESSOR_SRC)  
	$(eval TMP := $(shell mktemp -d JVMCI_OPTIONS_PROCESSOR_XXXXX))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH)  $(JVMCI_OPTIONS_PROCESSOR_SRC)
	cp -r graal/com.oracle.jvmci.options.processor/src/META-INF $(TMP)
	$(call process_options,$(TMP),False)
	mkdir -p $$(dirname $(JVMCI_OPTIONS_PROCESSOR_JAR))
	$(JAR) cf $(JVMCI_OPTIONS_PROCESSOR_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR): $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC)  
	$(eval TMP := $(shell mktemp -d JVMCI_HOTSPOTVMCONFIG_PROCESSOR_XXXXX))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH)  $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC)
	cp -r graal/com.oracle.jvmci.hotspotvmconfig.processor/src/META-INF $(TMP)
	$(call process_options,$(TMP),False)
	mkdir -p $$(dirname $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR))
	$(JAR) cf $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_SERVICE_PROCESSOR_JAR): $(JVMCI_SERVICE_PROCESSOR_SRC)  
	$(eval TMP := $(shell mktemp -d JVMCI_SERVICE_PROCESSOR_XXXXX))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH)  $(JVMCI_SERVICE_PROCESSOR_SRC)
	cp -r graal/com.oracle.jvmci.service.processor/src/META-INF $(TMP)
	$(call process_options,$(TMP),False)
	mkdir -p $$(dirname $(JVMCI_SERVICE_PROCESSOR_JAR))
	$(JAR) cf $(JVMCI_SERVICE_PROCESSOR_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_API_JAR): $(JVMCI_API_SRC)  $(JVMCI_API_DEP_JARS)
	$(eval TMP := $(shell mktemp -d JVMCI_API_XXXXX))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH) -cp $(shell echo $(JVMCI_API_DEP_JARS) | tr ' ' ':') $(JVMCI_API_SRC)
	
	$(call process_options,$(TMP),True)
	mkdir -p $$(dirname $(JVMCI_API_JAR))
	$(JAR) cf $(JVMCI_API_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_SERVICE_JAR): $(JVMCI_SERVICE_SRC)  $(JVMCI_SERVICE_DEP_JARS)
	$(eval TMP := $(shell mktemp -d JVMCI_SERVICE_XXXXX))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH) -cp $(shell echo $(JVMCI_SERVICE_DEP_JARS) | tr ' ' ':') $(JVMCI_SERVICE_SRC)
	
	$(call process_options,$(TMP),True)
	mkdir -p $$(dirname $(JVMCI_SERVICE_JAR))
	$(JAR) cf $(JVMCI_SERVICE_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_HOTSPOT_JAR): $(JVMCI_HOTSPOT_SRC) $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR) $(JVMCI_OPTIONS_PROCESSOR_JAR) $(JVMCI_SERVICE_PROCESSOR_JAR) $(JVMCI_HOTSPOT_DEP_JARS)
	$(eval TMP := $(shell mktemp -d JVMCI_HOTSPOT_XXXXX))
	$(JAVAC) -d $(TMP) -processorpath $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR):$(JVMCI_OPTIONS_PROCESSOR_JAR):$(JVMCI_SERVICE_PROCESSOR_JAR) -bootclasspath $(JDK_BOOTCLASSPATH) -cp $(shell echo $(JVMCI_HOTSPOT_DEP_JARS) | tr ' ' ':') $(JVMCI_HOTSPOT_SRC)
	
	$(call process_options,$(TMP),True)
	mkdir -p $$(dirname $(JVMCI_HOTSPOT_JAR))
	$(JAR) cf $(JVMCI_HOTSPOT_JAR) -C $(TMP) .
	rm -r $(TMP)

default: $(JVMCI_API_JAR) $(JVMCI_SERVICE_JAR) $(JVMCI_HOTSPOT_JAR)
.PHONY: default