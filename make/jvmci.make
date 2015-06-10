# This Makefile is generated automatically, do not edit
# This file was built with the command: mx.sh makefile -o ./make/jvmci.make

TARGET=.
# Bootstrap JDK to be used (for javac and jar)
ABS_BOOTDIR=

JAVAC=$(ABS_BOOTDIR)/bin/javac -g -target 1.8
JAR=$(ABS_BOOTDIR)/bin/jar

EXPORTED_FILES_ADDITIONAL=$(TARGET)/options $(TARGET)/services
HS_COMMON_SRC=.

# Directories, where the generated property-files reside within the JAR files
PROVIDERS_INF=/META-INF/providers
SERVICES_INF=/META-INF/services
OPTIONS_INF=/META-INF/options

ifeq ($(ABS_BOOTDIR),)
    $(error Variable ABS_BOOTDIR must be set to a JDK installation.)
endif
ifeq ($(MAKE_VERBOSE),)
    QUIETLY=@
endif

define process_options
    $(eval providers=$(1)/$(PROVIDERS_INF))
    $(eval services=$(1)/$(SERVICES_INF))
    $(eval options=$(1)/$(OPTIONS_INF))
    $(QUIETLY) test -d $(services) || mkdir -p $(services)
    $(QUIETLY) test ! -d $(providers) || (cd $(providers) && for i in $$(ls); do c=$$(cat $$i); echo $$i >> $(abspath $(services))/$$c; rm $$i; done)

    @# Since all projects are built together with one javac call we cannot determine
    @# which project contains HotSpotVMConfig.inline.hpp so we hardcode it.
    $(eval vmconfig=$(1)/hotspot/HotSpotVMConfig.inline.hpp)
    $(eval vmconfigDest=$(HS_COMMON_SRC)/../jvmci/com.oracle.jvmci.hotspot/src_gen/hotspot)
    $(QUIETLY) test ! -f $(vmconfig) || (mkdir -p $(vmconfigDest) && cp $(vmconfig) $(vmconfigDest))
endef

define extract
    $(eval TMP := $(shell mktemp -d $(TARGET)/tmp_XXXXX))
    $(QUIETLY) mkdir -p $(2);
    $(QUIETLY) cd $(TMP) && $(JAR) xf $(abspath $(1)) && \
        ((test ! -d .$(SERVICES_INF) || cp -r .$(SERVICES_INF) $(abspath $(2))) &&  (test ! -d .$(OPTIONS_INF) || cp -r .$(OPTIONS_INF) $(abspath $(2))));
    $(QUIETLY) rm -r $(TMP);
    $(QUIETLY) cp $(1) $(2);
endef

define build_and_jar
    $(info Building $(4))
    $(eval TMP := $(shell mkdir -p $(TARGET) && mktemp -d $(TARGET)/tmp_XXXXX))
    $(QUIETLY) $(JAVAC) -d $(TMP) -processorpath :$(1) -bootclasspath $(JDK_BOOTCLASSPATH) -cp :$(2) $(filter %.java,$?);
    $(QUIETLY) test "$(3)" = "" || cp -r $(3) $(TMP);
    $(QUIETLY) $(call process_options,$(TMP));
    $(QUIETLY) mkdir -p $(shell dirname $(4))
    $(QUIETLY) $(JAR) cf $(4) -C $(TMP) .
    $(QUIETLY) rm -r $(TMP);
endef

all: default

export: all
	$(info Put $(EXPORTED_FILES) into SHARED_DIR $(SHARED_DIR))
	$(QUIETLY) mkdir -p $(SHARED_DIR)
	$(foreach export,$(EXPORTED_FILES),$(call extract,$(export),$(SHARED_DIR)))
.PHONY: export



JDK_BOOTCLASSPATH = $(ABS_BOOTDIR)/jre/lib/resources.jar:$(ABS_BOOTDIR)/jre/lib/rt.jar:$(ABS_BOOTDIR)/jre/lib/jsse.jar:$(ABS_BOOTDIR)/jre/lib/jce.jar:$(ABS_BOOTDIR)/jre/lib/charsets.jar:$(ABS_BOOTDIR)/jre/lib/jfr.jar

JVMCI_OPTIONS_PROCESSOR_SRC = $(shell find jvmci/com.oracle.jvmci.options/src -type f 2> /dev/null)
JVMCI_OPTIONS_PROCESSOR_SRC += $(shell find jvmci/com.oracle.jvmci.options.processor/src -type f 2> /dev/null)

JVMCI_OPTIONS_PROCESSOR_JAR = $(TARGET)/jvmci/com.oracle.jvmci.options.processor/ap/com.oracle.jvmci.options.processor.jar

JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC = $(shell find jvmci/com.oracle.jvmci.hotspotvmconfig/src -type f 2> /dev/null)
JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC += $(shell find jvmci/com.oracle.jvmci.common/src -type f 2> /dev/null)
JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC += $(shell find jvmci/com.oracle.jvmci.hotspotvmconfig.processor/src -type f 2> /dev/null)

JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR = $(TARGET)/jvmci/com.oracle.jvmci.hotspotvmconfig.processor/ap/com.oracle.jvmci.hotspotvmconfig.processor.jar

JVMCI_SERVICE_PROCESSOR_SRC = $(shell find jvmci/com.oracle.jvmci.service/src -type f 2> /dev/null)
JVMCI_SERVICE_PROCESSOR_SRC += $(shell find jvmci/com.oracle.jvmci.service.processor/src -type f 2> /dev/null)

JVMCI_SERVICE_PROCESSOR_JAR = $(TARGET)/jvmci/com.oracle.jvmci.service.processor/ap/com.oracle.jvmci.service.processor.jar

JVMCI_API_SRC = $(shell find jvmci/com.oracle.jvmci.meta/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.meta/jvmci/com.oracle.jvmci.meta/src_gen -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.code/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.code/jvmci/com.oracle.jvmci.code/src_gen -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.runtime/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.runtime/jvmci/com.oracle.jvmci.runtime/src_gen -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.options/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.options/jvmci/com.oracle.jvmci.options/src_gen -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.common/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.common/jvmci/com.oracle.jvmci.common/src_gen -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.debug/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.debug/jvmci/com.oracle.jvmci.debug/src_gen -type f 2> /dev/null)

JVMCI_API_JAR = $(TARGET)/build/jvmci-api.jar

JVMCI_API_DEP_JARS = $(TARGET)/build/jvmci-service.jar jvmci/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_API_JAR)

JVMCI_SERVICE_SRC = $(shell find jvmci/com.oracle.jvmci.service/src -type f 2> /dev/null)

JVMCI_SERVICE_JAR = $(TARGET)/build/jvmci-service.jar

JVMCI_SERVICE_DEP_JARS = jvmci/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_SERVICE_JAR)

JVMCI_HOTSPOT_SRC = $(shell find jvmci/com.oracle.jvmci.hotspotvmconfig/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspotvmconfig/jvmci/com.oracle.jvmci.hotspotvmconfig/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.amd64/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.amd64/jvmci/com.oracle.jvmci.amd64/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.compiler/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.compiler/jvmci/com.oracle.jvmci.compiler/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot/jvmci/com.oracle.jvmci.hotspot/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.amd64/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.amd64/jvmci/com.oracle.jvmci.hotspot.amd64/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.sparc/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.sparc/jvmci/com.oracle.jvmci.sparc/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.sparc/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.sparc/jvmci/com.oracle.jvmci.hotspot.sparc/src_gen -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.jfr/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.jfr/jvmci/com.oracle.jvmci.hotspot.jfr/src_gen -type f 2> /dev/null)

JVMCI_HOTSPOT_JAR = $(TARGET)/build/jvmci-hotspot.jar

JVMCI_HOTSPOT_DEP_JARS = $(TARGET)/build/jvmci-api.jar $(TARGET)/build/jvmci-service.jar jvmci/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_HOTSPOT_JAR)

$(JVMCI_OPTIONS_PROCESSOR_JAR): $(JVMCI_OPTIONS_PROCESSOR_SRC)  
	$(call build_and_jar,,$(shell echo  | tr ' ' ':'),jvmci/com.oracle.jvmci.options.processor/src/META-INF,$(JVMCI_OPTIONS_PROCESSOR_JAR))


$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR): $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC)  
	$(call build_and_jar,,$(shell echo  | tr ' ' ':'),jvmci/com.oracle.jvmci.hotspotvmconfig.processor/src/META-INF,$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR))


$(JVMCI_SERVICE_PROCESSOR_JAR): $(JVMCI_SERVICE_PROCESSOR_SRC)  
	$(call build_and_jar,,$(shell echo  | tr ' ' ':'),jvmci/com.oracle.jvmci.service.processor/src/META-INF,$(JVMCI_SERVICE_PROCESSOR_JAR))


$(JVMCI_API_JAR): $(JVMCI_API_SRC) $(JVMCI_OPTIONS_PROCESSOR_JAR) $(JVMCI_API_DEP_JARS)
	$(call build_and_jar,$(JVMCI_OPTIONS_PROCESSOR_JAR),$(shell echo $(JVMCI_API_DEP_JARS) | tr ' ' ':'),,$(JVMCI_API_JAR))


$(JVMCI_SERVICE_JAR): $(JVMCI_SERVICE_SRC)  $(JVMCI_SERVICE_DEP_JARS)
	$(call build_and_jar,,$(shell echo $(JVMCI_SERVICE_DEP_JARS) | tr ' ' ':'),,$(JVMCI_SERVICE_JAR))


$(JVMCI_HOTSPOT_JAR): $(JVMCI_HOTSPOT_SRC) $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR) $(JVMCI_OPTIONS_PROCESSOR_JAR) $(JVMCI_SERVICE_PROCESSOR_JAR) $(JVMCI_HOTSPOT_DEP_JARS)
	$(call build_and_jar,$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR):$(JVMCI_OPTIONS_PROCESSOR_JAR):$(JVMCI_SERVICE_PROCESSOR_JAR),$(shell echo $(JVMCI_HOTSPOT_DEP_JARS) | tr ' ' ':'),,$(JVMCI_HOTSPOT_JAR))


default: $(JVMCI_API_JAR) $(JVMCI_SERVICE_JAR) $(JVMCI_HOTSPOT_JAR)
.PHONY: default