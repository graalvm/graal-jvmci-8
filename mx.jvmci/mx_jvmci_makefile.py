#
# ----------------------------------------------------------------------------------------------------
#
# Copyright (c) 2015, 2015, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#
# ----------------------------------------------------------------------------------------------------
#
import mx, mx_jvmci, os
from argparse import ArgumentParser, REMAINDER


class Makefile:
    def __init__(self):
        self.rules = []
        self.definitions = []

    def add_rule(self, s):
        self.rules.append(s)

    def add_definition(self, s):
        self.definitions.append(s)

    def generate(self):
        return "\n\n".join(self.definitions + self.rules)


def build_makefile(args):
    """Creates a Makefile which is able to build distributions without mx

    The return value indicates how many files were modified"""
    parser = ArgumentParser(prog='mx makefile')
    parser.add_argument('-o', action='store', dest='output', help='Write contents to this file.')
    parser.add_argument('selectedDists', help="Selected distribution names which are going to be built with make.", nargs=REMAINDER)
    opts = parser.parse_args(args)

    if not opts.selectedDists:
        opts.selectedDists = [d.dist() for d in mx_jvmci.jdkDeployedDists if isinstance(d, mx_jvmci.JarJDKDeployedDist) and d.partOfHotSpot]
    else:
        opts.selectedDists = [mx.distribution(name) for name in opts.selectedDists]
    mf = Makefile()
    if do_build_makefile(mf, opts.selectedDists):
        contents = mf.generate()
        if opts.output == None:
            print contents
        else:
            if mx.update_file(opts.output, contents, showDiff=True):
                return 1
    return 0

def get_jdk_deployed_dists():
    return [d.dist() for d in mx_jvmci.jdkDeployedDists]

def _get_dependency_check(p):
    jarFinders = []
    for dep in p.deps:
        jar = None
        if dep.isJreLibrary() and dep.optional:
            jar = dep.jar
        if dep.isJdkLibrary() and dep.optional:
            jar = dep.path
        if jar:
            jarFinders.append("$(shell find $(ABS_BOOTDIR)/ -name '%s'; echo $$?)" % jar)
    return "ifeq ({},'{}')".format("".join(jarFinders), "0" * len(jarFinders)) if len(jarFinders) > 0 else None

def make_dist_rule(dist, mf):
    def path_dist_relative(p):
        return os.path.relpath(p, dist.suite.dir)
    jdkDeployedDists = get_jdk_deployed_dists()
    jarName = os.path.basename(dist.path)
    sourcesVariableName = dist.name + "_SRC"
    depJarVariableName = dist.name + "_DEP_JARS"
    sources = []
    resources = []
    projects = [p for p in dist.archived_deps() if p.isJavaProject()]
    targetPathPrefix = "$(TARGET)/"
    annotationProcessorDeps = set()

    classPath = [targetPathPrefix + os.path.basename(e.path) for e in mx.classpath_entries(dist, includeSelf=False, preferProjects=False)]
    for p in projects:
        projectDir = path_dist_relative(p.dir)
        annotationProcessorDeps.update(p.declaredAnnotationProcessors)
        depCheck = _get_dependency_check(p)
        if depCheck:
            sources.append(depCheck)
        for src in [projectDir + '/' + d for d in p.srcDirs]:
            sources.append(sourcesVariableName + " += $(shell find {} -type f 2> /dev/null)".format(src))
            metaInf = src + "/META-INF"
            if os.path.exists(os.path.join(dist.suite.dir, metaInf)):
                resources.append(metaInf)
        if depCheck:
            sources.append("endif")

    mf.add_definition("\n".join(sources))

    apDistNames = []
    apDistVariableNames = []
    apDependencies = []
    for apd in sorted(annotationProcessorDeps):
        apDistNames.append(apd.name)
        apDistVariableNames.append("$(" + apd.name + "_JAR)")
        apDependencies.append("$(subst  $(space),:,$(" + apd.name + "_DEP_JARS))")
    shouldExport = dist in jdkDeployedDists
    props = {
           "name": dist.name,
           "jarName": targetPathPrefix + jarName,
           "depJarsVariableAccess": "$(" + depJarVariableName + ")" if len(classPath) > 0 else "",
           "depJarsVariable": depJarVariableName,
           "sourcesVariableName": sourcesVariableName,
           "annotationProcessors": " ".join(apDistVariableNames),
           "cpAnnotationProcessors": ":".join(apDistVariableNames + apDependencies),
           "jarDeps": " ".join(classPath),
           "copyResources": " ".join(resources)
           }

    mf.add_definition("{name}_JAR = {jarName}".format(**props))
    if len(classPath) > 0: mf.add_definition("{depJarsVariable} = {jarDeps}".format(**props))
    if shouldExport: mf.add_definition("EXPORTED_FILES += $({name}_JAR)".format(**props))
    mf.add_rule("""$({name}_JAR): $({sourcesVariableName}) {annotationProcessors} {depJarsVariableAccess}
\t$(call build_and_jar,{cpAnnotationProcessors},$(subst  $(space),:,{depJarsVariableAccess}),{copyResources},$({name}_JAR))
""".format(**props))
    return



def do_build_makefile(mf, selectedDists):
    jdk = mx.get_jdk()
    bootClassPath = jdk.bootclasspath(filtered=False)
    bootClassPath = bootClassPath.replace(os.path.realpath(jdk.home), "$(ABS_BOOTDIR)")
    jdkBootClassPathVariableName = "JDK_BOOTCLASSPATH"

    mf.add_definition("""# This Makefile is generated automatically, do not edit

TARGET=.
# Bootstrap JDK to be used (for javac and jar)
ABS_BOOTDIR=

JAVAC=$(ABS_BOOTDIR)/bin/javac -g -target """ + str(jdk.javaCompliance) + """
JAR=$(ABS_BOOTDIR)/bin/jar

HS_COMMON_SRC=.

# Directories, where the generated property-files reside within the JAR files
PROVIDERS_INF=/META-INF/jvmci.providers
SERVICES_INF=/META-INF/jvmci.services
OPTIONS_INF=/META-INF/jvmci.options

JARS = $(foreach dist,$(DISTRIBUTIONS),$($(dist)_JAR))

ifeq ($(ABS_BOOTDIR),)
    $(error Variable ABS_BOOTDIR must be set to a JDK installation.)
endif
ifeq ($(MAKE_VERBOSE),)
    QUIETLY=@
endif

# Required to construct a whitespace for use with subst
space :=
space +=

# Takes the provider files created by ServiceProviderProcessor (the processor
# for the @ServiceProvider annotation) and merges them into a single file.
# Arguments:
#  1: directory with contents of the JAR file
define process_providers
    $(eval providers := $(1)/$(PROVIDERS_INF))
    $(eval services := $(1)/$(SERVICES_INF))
    $(QUIETLY) test -d $(services) || mkdir -p $(services)
    $(QUIETLY) test ! -d $(providers) || (cd $(providers) && for i in $$(ls); do c=$$(cat $$i); echo $$i >> $(abspath $(services))/$$c; rm $$i; done)

    @# Since all projects are built together with one javac call we cannot determine
    @# which project contains HotSpotVMConfig.inline.hpp so we hardcode it.
    $(eval vmconfig := $(1)/hotspot/HotSpotVMConfig.inline.hpp)
    $(eval vmconfigDest := $(HS_COMMON_SRC)/../mxbuild/jvmci/jdk.internal.jvmci.hotspot/src_gen/hotspot)
    $(QUIETLY) test ! -f $(vmconfig) || (mkdir -p $(vmconfigDest) && cp $(vmconfig) $(vmconfigDest))
endef

# Finds the *_OptionsDescriptors classes created by OptionProcessor (the processor for the @Option annotation)
# and appends their names to services/jdk.internal.jvmci.options.OptionDescriptors.
# Arguments:
#  1: directory with contents of the JAR file
define process_options
    $(eval services := $(1)/META-INF/services)
    $(QUIETLY) test -d $(services) || mkdir -p $(services)
    $(eval optionDescriptors := $(1)/META-INF/services/jdk.internal.jvmci.options.OptionDescriptors)
    $(QUIETLY) cd $(1) && for i in $$(find . -name '*_OptionDescriptors.class' 2>/dev/null); do echo $${i} | sed 's:\\./\\(.*\\)\\.class:\\1:g' | tr '/' '.' >> $(abspath $(optionDescriptors)); done
endef

# Extracts META-INF/jvmci.services from a JAR file into a given directory
# Arguments:
#  1: JAR file to extract
#  2: target directory (which already exists)
define extract
    $(eval TMP := $(shell mktemp -d $(TARGET)/tmp_XXXXX))
    $(QUIETLY) cd $(TMP) && $(JAR) xf $(abspath $(1)) && \\
         (test ! -d .$(SERVICES_INF) || cp -r .$(SERVICES_INF) $(abspath $(2)));
    $(QUIETLY) rm -r $(TMP);
    $(QUIETLY) cp $(1) $(2)
endef

# Calls $(JAVAC) with the boot class path $(JDK_BOOTCLASSPATH) and sources taken from the automatic variable $^
# Arguments:
#  1: processorpath
#  2: classpath
#  3: resources to copy
#  4: target JAR file
define build_and_jar
    $(info Building $(4))
    $(eval TMP := $(shell mkdir -p $(TARGET) && mktemp -d $(TARGET)/tmp_XXXXX))
    $(QUIETLY) $(JAVAC) -d $(TMP) -processorpath :$(1) -bootclasspath $(JDK_BOOTCLASSPATH) -cp :$(2) $(filter %.java,$^)
    $(QUIETLY) test "$(3)" = "" || cp -r $(3) $(TMP)
    $(QUIETLY) $(call process_options,$(TMP))
    $(QUIETLY) $(call process_providers,$(TMP))
    $(QUIETLY) mkdir -p $(shell dirname $(4))
    $(QUIETLY) $(JAR) -0cf $(4) -C $(TMP) .
    $(QUIETLY) rm -r $(TMP)
endef

# Verifies that make/defs.make contains an appropriate line for each JVMCI service
# and that only existing JVMCI services are exported.
# Arguments:
#  1: list of service files
#  2: variable name for directory of service files
define verify_defs_make
    $(eval defs := make/defs.make)
    $(eval uncondPattern := EXPORT_LIST += $$$$($(2))/)
    $(eval condPattern := CONDITIONAL_EXPORT_LIST += $$$$($(2))/)
    $(eval unconditionalExports := $(shell grep '^EXPORT_LIST += $$($2)' make/defs.make | sed 's:.*($(2))/::g'))
    $(eval conditionalExports := $(shell grep '^CONDITIONAL_EXPORT_LIST += $$($2)' make/defs.make | sed 's:.*($(2))/::g'))
    $(eval allExports := $(unconditionalExports) $(conditionalExports))
    $(foreach file,$(1),$(if $(findstring $(file),$(allExports)), ,$(error "Line matching '$(uncondPattern)$(file)' or '$(condPattern)$(file)' not found in $(defs)")))
    $(foreach export,$(unconditionalExports),$(if $(findstring $(export),$(1)), ,$(error "The line '$(uncondPattern)$(export)' should not be in $(defs)")))
endef

all: default
\t$(info Put $(EXPORTED_FILES) into SHARED_DIR $(SHARED_DIR))
\t$(shell mkdir -p $(SHARED_DIR))
\t$(foreach export,$(EXPORTED_FILES),$(call extract,$(export),$(SHARED_DIR)))

export: all
\t$(call verify_defs_make,$(notdir $(wildcard $(SHARED_DIR)/jvmci.services/*)),EXPORT_JRE_LIB_JVMCI_SERVICES_DIR)
.PHONY: export

clean:
\t$(QUIETLY) rm $(JARS) 2> /dev/null || true
\t$(QUIETLY) rmdir -p $(dir $(JARS)) 2> /dev/null || true
.PHONY: export clean

""")
    assert selectedDists
    selectedDists = [mx.dependency(s) for s in selectedDists]
    dists = []

    def _visit(dep, edge):
        if dep.isDistribution():
            dists.append(dep)

    mx.walk_deps(roots=selectedDists, visit=_visit, ignoredEdges=[mx.DEP_EXCLUDED])

    mf.add_definition(jdkBootClassPathVariableName + " = " + bootClassPath)
    for dist in dists: make_dist_rule(dist, mf)
    mf.add_definition("DISTRIBUTIONS = " + ' '.join([dist.name for dist in dists]))
    mf.add_rule("default: $({}_JAR)\n.PHONY: default\n".format("_JAR) $(".join([d.name for d in selectedDists])))
    return True
