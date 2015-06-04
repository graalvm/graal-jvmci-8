[?1034hVERBOSE=
TARGET=.
JDK=

WGET=wget
JAVAC=$(JDK)/bin/javac -g -target 1.8
JAR=$(JDK)/bin/jar

EXPORT_DIR=export
EXPORTED_FILES_ADDITIONAL=$(TARGET)/options $(TARGET)/services
HS_COMMON_SRC=.
# where all other stuff built by mx (graal.jar) resides
MX_TARGET=.
PROVIDERS_INF=/META-INF/providers/
SERVICES_INF=/META-INF/services/
OPTIONS_INF=/META-INF/options/

ifeq ($(JDK),)
    $(error Variable JDK must be set to a JDK installation.)
    endif
    ifneq ($(VERBOSE),)
    SHELL=sh -x
endif

define process_options =
    $(eval providers=$(1)/$(PROVIDERS_INF))
    $(eval services=$(1)/$(SERVICES_INF))
    $(eval options=$(1)/$(OPTIONS_INF))
    test -d $(services) || mkdir -p $(services)
    test ! -d $(providers) || (cd $(providers) && for i in $$(ls $(providers)); do c=$$(cat $$i); echo $$i >> $(services)$$c; rm $$i; done)

    # We're building all projects together with one javac call; thus we cannot determine, from which project the generated file is thus we hardcode it for now
    $(eval vmconfig=$(1)/hotspot/HotSpotVMConfig.inline.hpp)
    $(eval vmconfigDest=$(HS_COMMON_SRC)/../graal/com.oracle.jvmci.hotspot/src_gen/hotspot)
    test ! -f $(vmconfig) || (mkdir -p $(vmconfigDest) && cp $(vmconfig) $(vmconfigDest))
endef

define extract =
    $(eval TMP := $(shell mktemp -d))
    mkdir -p $(2);
    cd $(TMP) && $(JAR) xf $(abspath $(1)) &&         ((test ! -d .$(SERVICES_INF) || cp -r .$(SERVICES_INF) $(abspath $(2))) &&  (test ! -d .$(OPTIONS_INF) || cp -r .$(OPTIONS_INF) $(abspath $(2))))
    rm -r $(TMP)
    cp $(1) $(2)
endef


all: default

export: all
	mkdir -p $(EXPORT_DIR)
	$(foreach export,$(EXPORTED_FILES),$(call extract,$(export),$(EXPORT_DIR)))
.PHONY: export



EXPORTED_FILES += $(MX_TARGET)/build/truffle.jar

EXPORTED_FILES += $(MX_TARGET)/build/graal.jar

EXPORTED_FILES += $(MX_TARGET)/build/graal-truffle.jar

JDK_BOOTCLASSPATH = $(JDK)/jre/lib/resources.jar:$(JDK)/jre/lib/rt.jar:$(JDK)/jre/lib/jsse.jar:$(JDK)/jre/lib/jce.jar:$(JDK)/jre/lib/charsets.jar:$(JDK)/jre/lib/jfr.jar

COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_SRC = $(shell find graal/com.oracle.jvmci.service/src -type f -name *.java 2> /dev/null)
COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.options/src -type f -name *.java 2> /dev/null)
COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.options.processor/src -type f -name *.java 2> /dev/null)

COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_JAR = $(TARGET)/graal/com.oracle.jvmci.options.processor/ap/com.oracle.jvmci.options.processor.jar

COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC = $(shell find graal/com.oracle.jvmci.hotspotvmconfig/src -type f -name *.java 2> /dev/null)
COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.common/src -type f -name *.java 2> /dev/null)
COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC += $(shell find graal/com.oracle.jvmci.hotspotvmconfig.processor/src -type f -name *.java 2> /dev/null)

COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR = $(TARGET)/graal/com.oracle.jvmci.hotspotvmconfig.processor/ap/com.oracle.jvmci.hotspotvmconfig.processor.jar

JVMCI_SERVICE_SRC = $(shell find graal/com.oracle.jvmci.service/src -type f -name *.java 2> /dev/null)

JVMCI_SERVICE_JAR = $(TARGET)/build/jvmci-service.jar

JVMCI_SERVICE_DEP_JARS = lib/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_SERVICE_JAR)

JVMCI_HOTSPOT_SRC = $(shell find graal/com.oracle.jvmci.hotspotvmconfig/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspotvmconfig/graal/com.oracle.jvmci.hotspotvmconfig/src_gen -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot/src -type f -name *.java 2> /dev/null)
JVMCI_HOTSPOT_SRC += $(shell find graal/com.oracle.jvmci.hotspot/graal/com.oracle.jvmci.hotspot/src_gen -type f -name *.java 2> /dev/null)

JVMCI_HOTSPOT_JAR = $(TARGET)/build/jvmci-hotspot.jar

JVMCI_HOTSPOT_DEP_JARS = $(TARGET)/build/jvmci-service.jar $(TARGET)/build/jvmci-api.jar lib/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_HOTSPOT_JAR)

JVMCI_API_SRC = $(shell find graal/com.oracle.jvmci.meta/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.code/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.runtime/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.options/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.common/src -type f -name *.java 2> /dev/null)
JVMCI_API_SRC += $(shell find graal/com.oracle.jvmci.debug/src -type f -name *.java 2> /dev/null)

JVMCI_API_JAR = $(TARGET)/build/jvmci-api.jar

JVMCI_API_DEP_JARS = $(TARGET)/build/jvmci-service.jar lib/findbugs-SuppressFBWarnings.jar

EXPORTED_FILES += $(JVMCI_API_JAR)

$(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_JAR): $(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_SRC)  
	$(eval TMP := $(shell mktemp -d))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH)  $(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_SRC)
	cp -r graal/com.oracle.jvmci.options.processor/src/META-INF $(TMP)
	$(call process_options,$(TMP),False)
	mkdir -p $$(dirname $(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_JAR))
	$(JAR) cf $(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_JAR) -C $(TMP) .
	rm -r $(TMP)

$(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR): $(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC)  
	$(eval TMP := $(shell mktemp -d))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH)  $(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_SRC)
	cp -r graal/com.oracle.jvmci.hotspotvmconfig.processor/src/META-INF $(TMP)
	$(call process_options,$(TMP),False)
	mkdir -p $$(dirname $(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR))
	$(JAR) cf $(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_SERVICE_JAR): $(JVMCI_SERVICE_SRC)  $(JVMCI_SERVICE_DEP_JARS)
	$(eval TMP := $(shell mktemp -d))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH) -cp lib/findbugs-SuppressFBWarnings.jar $(JVMCI_SERVICE_SRC)
	
	$(call process_options,$(TMP),True)
	mkdir -p $$(dirname $(JVMCI_SERVICE_JAR))
	$(JAR) cf $(JVMCI_SERVICE_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_HOTSPOT_JAR): $(JVMCI_HOTSPOT_SRC) $(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR) $(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_JAR) $(JVMCI_HOTSPOT_DEP_JARS)
	$(eval TMP := $(shell mktemp -d))
	$(JAVAC) -d $(TMP) -processorpath $(COM_ORACLE_JVMCI_HOTSPOTVMCONFIG_PROCESSOR_JAR):$(COM_ORACLE_JVMCI_OPTIONS_PROCESSOR_JAR) -bootclasspath $(JDK_BOOTCLASSPATH) -cp $(TARGET)/build/jvmci-service.jar:$(TARGET)/build/jvmci-api.jar:lib/findbugs-SuppressFBWarnings.jar $(JVMCI_HOTSPOT_SRC)
	
	$(call process_options,$(TMP),True)
	mkdir -p $$(dirname $(JVMCI_HOTSPOT_JAR))
	$(JAR) cf $(JVMCI_HOTSPOT_JAR) -C $(TMP) .
	rm -r $(TMP)

$(JVMCI_API_JAR): $(JVMCI_API_SRC)  $(JVMCI_API_DEP_JARS)
	$(eval TMP := $(shell mktemp -d))
	$(JAVAC) -d $(TMP)  -bootclasspath $(JDK_BOOTCLASSPATH) -cp $(TARGET)/build/jvmci-service.jar:lib/findbugs-SuppressFBWarnings.jar $(JVMCI_API_SRC)
	
	$(call process_options,$(TMP),True)
	mkdir -p $$(dirname $(JVMCI_API_JAR))
	$(JAR) cf $(JVMCI_API_JAR) -C $(TMP) .
	rm -r $(TMP)

default: $(JVMCI_SERVICE_JAR) $(JVMCI_HOTSPOT_JAR) $(JVMCI_API_JAR)
.PHONY: default
