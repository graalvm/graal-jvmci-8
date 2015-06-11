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

# Required to construct a whitespace for use with subst
space :=
space +=

# Takes the option files of the options annotation processor and merges them into a single file
# Arguments:
#  1: directory with contents of the JAR file
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

# Extracts META-INF/services and META-INF/options of a JAR file into a given directory
# Arguments:
#  1: JAR file to extract
#  2: target directory
define extract
    $(eval TMP := $(shell mktemp -d $(TARGET)/tmp_XXXXX))
    $(QUIETLY) mkdir -p $(2);
    $(QUIETLY) cd $(TMP) && $(JAR) xf $(abspath $(1)) && \
        ((test ! -d .$(SERVICES_INF) || cp -r .$(SERVICES_INF) $(abspath $(2))) &&  (test ! -d .$(OPTIONS_INF) || cp -r .$(OPTIONS_INF) $(abspath $(2))));
    $(QUIETLY) rm -r $(TMP);
    $(QUIETLY) cp $(1) $(2);
endef

# Calls $(JAVAC) with the bootclasspath $(JDK_BOOTCLASSPATH); sources are taken from the automatic variable $^
# Arguments:
#  1: processorpath
#  2: classpath
#  3: resources to copy
#  4: target JAR file
define build_and_jar
    $(info Building $(4))
    $(eval TMP := $(shell mkdir -p $(TARGET) && mktemp -d $(TARGET)/tmp_XXXXX))
    $(QUIETLY) $(JAVAC) -d $(TMP) -processorpath :$(1) -bootclasspath $(JDK_BOOTCLASSPATH) -cp :$(2) $(filter %.java,$^);
    $(QUIETLY) test "$(3)" = "" || cp -r $(3) $(TMP);
    $(QUIETLY) $(call process_options,$(TMP));
    $(QUIETLY) mkdir -p $(shell dirname $(4))
    $(QUIETLY) $(JAR) cf $(4) -C $(TMP) .
    $(QUIETLY) rm -r $(TMP);
endef

# Verifies if the defs.make contain the exported files of services/
define verify_export_def_make
    $(foreach file,$(1),$(if $(shell grep '$(2)$(file)' $(3) > /dev/null && echo found), , $(error "Pattern '$(2)$(file)' not found in $(3)")))
endef

all: default

export: all
	$(info Put $(EXPORTED_FILES) into SHARED_DIR $(SHARED_DIR))
	$(QUIETLY) mkdir -p $(SHARED_DIR)
	$(foreach export,$(EXPORTED_FILES),$(call extract,$(export),$(SHARED_DIR)))
	$(call verify_export_def_make,$(notdir $(wildcard $(SHARED_DIR)/services/*)),EXPORT_LIST += $$(EXPORT_JRE_LIB_JVMCI_SERVICES_DIR)/,make/defs.make)
	$(call verify_export_def_make,$(notdir $(wildcard $(SHARED_DIR)/options/*)),EXPORT_LIST += $$(EXPORT_JRE_LIB_JVMCI_OPTIONS_DIR)/,make/defs.make)
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
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.code/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.runtime/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.options/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.common/src -type f 2> /dev/null)
JVMCI_API_SRC += $(shell find jvmci/com.oracle.jvmci.debug/src -type f 2> /dev/null)

JVMCI_API_JAR = $(TARGET)/build/jvmci-api.jar

JVMCI_API_DEP_JARS = $(TARGET)/build/jvmci-service.jar jvmci/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_API_JAR)

JVMCI_SERVICE_SRC = $(shell find jvmci/com.oracle.jvmci.service/src -type f 2> /dev/null)

JVMCI_SERVICE_JAR = $(TARGET)/build/jvmci-service.jar

JVMCI_SERVICE_DEP_JARS = jvmci/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_SERVICE_JAR)

JVMCI_HOTSPOT_SRC = $(shell find jvmci/com.oracle.jvmci.hotspotvmconfig/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.amd64/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.compiler/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.amd64/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.sparc/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.sparc/src -type f 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find jvmci/com.oracle.jvmci.hotspot.jfr/src -type f 2> /dev/null)

JVMCI_HOTSPOT_JAR = $(TARGET)/build/jvmci-hotspot.jar

JVMCI_HOTSPOT_DEP_JARS = $(TARGET)/build/jvmci-api.jar $(TARGET)/build/jvmci-service.jar jvmci/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_HOTSPOT_JAR)

$(JVMCI_OPTIONS_PROCESSOR_JAR): $(JVMCI_OPTIONS_PROCESSOR_SRC)  
	$(call build_and_jar,,$(subst  $(space),:,),jvmci/com.oracle.jvmci.options.processor/src/META-INF,$(JVMCI_OPTIONS_PROCESSOR_JAR))


$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR): $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC)  
	$(call build_and_jar,,$(subst  $(space),:,),jvmci/com.oracle.jvmci.hotspotvmconfig.processor/src/META-INF,$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR))


$(JVMCI_SERVICE_PROCESSOR_JAR): $(JVMCI_SERVICE_PROCESSOR_SRC)  
	$(call build_and_jar,,$(subst  $(space),:,),jvmci/com.oracle.jvmci.service.processor/src/META-INF,$(JVMCI_SERVICE_PROCESSOR_JAR))


$(JVMCI_API_JAR): $(JVMCI_API_SRC) $(JVMCI_OPTIONS_PROCESSOR_JAR) $(JVMCI_API_DEP_JARS)
	$(call build_and_jar,$(JVMCI_OPTIONS_PROCESSOR_JAR),$(subst  $(space),:,$(JVMCI_API_DEP_JARS)),,$(JVMCI_API_JAR))


$(JVMCI_SERVICE_JAR): $(JVMCI_SERVICE_SRC)  $(JVMCI_SERVICE_DEP_JARS)
	$(call build_and_jar,,$(subst  $(space),:,$(JVMCI_SERVICE_DEP_JARS)),,$(JVMCI_SERVICE_JAR))


$(JVMCI_HOTSPOT_JAR): $(JVMCI_HOTSPOT_SRC) $(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR) $(JVMCI_OPTIONS_PROCESSOR_JAR) $(JVMCI_SERVICE_PROCESSOR_JAR) $(JVMCI_HOTSPOT_DEP_JARS)
	$(call build_and_jar,$(JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR):$(JVMCI_OPTIONS_PROCESSOR_JAR):$(JVMCI_SERVICE_PROCESSOR_JAR),$(subst  $(space),:,$(JVMCI_HOTSPOT_DEP_JARS)),,$(JVMCI_HOTSPOT_JAR))


default: $(JVMCI_API_JAR) $(JVMCI_SERVICE_JAR) $(JVMCI_HOTSPOT_JAR)
.PHONY: default