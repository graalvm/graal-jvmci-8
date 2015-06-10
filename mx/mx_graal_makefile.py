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
import mx, mx_graal, os
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
    """Creates a Makefile which is able to build distributions without mx"""
    parser = ArgumentParser(prog='mx makefile')
    parser.add_argument('-o', action='store', dest='output', help='Write contents to this file.')
    parser.add_argument('selectedDists', help="Selected distribution names which are going to be built with make.", nargs=REMAINDER)
    opts = parser.parse_args(args)

    if opts.selectedDists == None or len(opts.selectedDists) == 0:
        opts.selectedDists = [d.name for d in mx_graal._jdkDeployedDists if d.partOfHotSpot]
    mf = Makefile()
    commandline = " ".join(["mx.sh", "makefile"] + args)
    if do_build_makefile(mf, opts.selectedDists, commandline):
        contents = mf.generate()
        if opts.output == None:
            print contents
        else:
            with open(opts.output, "w") as f:
                f.write(contents)


def filter_projects(deps, t):
    def typeFilter(project): # filters
        if isinstance(project, str):
            project = mx.dependency(project, True)
        return isinstance(project, t)
    return [d for d in deps if typeFilter(d)]

def get_jdk_deployed_dists():
    return [d.name for d in mx_graal._jdkDeployedDists]

def update_list(li, elements):
    for e in elements:
        if e not in li:
            li.append(e)

def make_dist_rule(dist, mf, bootClassPath=None):
    def path_dist_relative(p):
        return os.path.relpath(p, dist.suite.dir)
    def short_dist_name(name):
        return name.replace("COM_ORACLE_", "")
    shortName = short_dist_name(dist.name)
    jdkDeployedDists = get_jdk_deployed_dists()
    jarPath = path_dist_relative(dist.path)
    sourcesVariableName = shortName + "_SRC"
    depJarVariableName = shortName + "_DEP_JARS"
    sources = []
    resources = []
    sortedDeps = dist.sorted_deps(True, transitive=False, includeAnnotationProcessors=True)
    projects = filter_projects(sortedDeps, mx.Project)
    targetPathPrefix = "$(TARGET)" + os.path.sep
    libraryDeps = [path_dist_relative(l.get_path(False)) for l in filter_projects(sortedDeps, mx.Library)]

    annotationProcessorDeps = []
    distDeps = dist.get_dist_deps(includeSelf=False, transitive=True)
    distDepProjects = []
    for d in distDeps:
        update_list(distDepProjects, d.sorted_deps(includeLibs=False, transitive=True))

    classPath = [targetPathPrefix + path_dist_relative(d.path) for d in distDeps] + libraryDeps \
        + [path_dist_relative(mx.dependency(name).path) for name in dist.excludedDependencies]
    for p in projects:
        if p.definedAnnotationProcessors != None and p.definedAnnotationProcessorsDist != dist:
            update_list(annotationProcessorDeps, [p])
    for p in projects:
        projectDir = path_dist_relative(p.dir)
        if p not in distDepProjects and p not in annotationProcessorDeps:
            generatedSource = [path_dist_relative(p.source_gen_dir())] if len(annotationProcessorDeps) > 0 else []

            for d in p.srcDirs + generatedSource:
                src = projectDir + os.path.sep + d
                sources.append("$(shell find {} -type f 2> /dev/null)".format(src))
                metaInf = src + os.path.sep + "META-INF"
                if os.path.exists(metaInf):
                    resources.append(metaInf)


    sourceLines = sourcesVariableName + " = " + ("\n" + sourcesVariableName + " += ").join(sources)
    apPaths = []
    apDistNames = []
    apDistVariableNames = []
    for p in annotationProcessorDeps:
        apPaths.append(path_dist_relative(p.definedAnnotationProcessorsDist.path))
        name = short_dist_name(p.definedAnnotationProcessorsDist.name)
        apDistNames.append(name)
        apDistVariableNames.append("$(" + name + "_JAR)")
    shouldExport = dist.name in jdkDeployedDists
    props = {
           "name": shortName,
           "jarPath": targetPathPrefix + jarPath,
           "depends": "",
           "depJarsVariableAccess": "$(" + depJarVariableName + ")" if len(classPath) > 0 else "",
           "depJarsVariable": depJarVariableName,
           "sourceLines": sourceLines,
           "sourcesVariableName": sourcesVariableName,
           "annotationProcessors": " ".join(apDistVariableNames),
           "cpAnnotationProcessors": "-processorpath " + ":".join(apDistVariableNames) if len(apDistVariableNames) > 0 else "",
           "cpAnnotationProcessors2": ":".join(apDistVariableNames),
           "bootCp": ("-bootclasspath " + bootClassPath) if bootClassPath != None else "",
           "cpDeps": ("-cp $(shell echo $(" + depJarVariableName + ") | tr ' ' ':')") if len(classPath) > 0 else "",
           "jarDeps": " ".join(classPath),
           "copyResources": " ".join(resources),
           "targetPathPrefix": targetPathPrefix,
           "shouldExport": shouldExport,
           }

    mf.add_definition(sourceLines)
    mf.add_definition("{name}_JAR = {jarPath}".format(**props))
    if len(classPath) > 0: mf.add_definition("{depJarsVariable} = {jarDeps}".format(**props))
    if shouldExport: mf.add_definition("EXPORTED_FILES += $({name}_JAR)".format(**props))
    mf.add_rule("""$({name}_JAR): $({sourcesVariableName}) {annotationProcessors} {depJarsVariableAccess}
\t$(call build_and_jar,{cpAnnotationProcessors2},$(shell echo {depJarsVariableAccess} | tr ' ' ':'),{copyResources},$({name}_JAR))
""".format(**props))
    return



def do_build_makefile(mf, selectedDists, commandline):
    java = mx.java()
    bootClassPath = java.bootclasspath()
    bootClassPath = bootClassPath.replace(java.jdk, "$(ABS_BOOTDIR)")
    jdkBootClassPathVariableName = "JDK_BOOTCLASSPATH"

    mf.add_definition("""# This Makefile is generated automatically, do not edit
# This file was built with the command: """ + commandline + """

TARGET=.
# Bootstrap JDK to be used (for javac and jar)
ABS_BOOTDIR=

JAVAC=$(ABS_BOOTDIR)/bin/javac -g -target """ + str(java.javaCompliance) + """
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
    $(eval TMP := $(shell mktemp -d tmp_XXXXX))
    $(QUIETLY) mkdir -p $(2);
    $(QUIETLY) cd $(TMP) && $(JAR) xf $(abspath $(1)) && \\
        ((test ! -d .$(SERVICES_INF) || cp -r .$(SERVICES_INF) $(abspath $(2))) &&  (test ! -d .$(OPTIONS_INF) || cp -r .$(OPTIONS_INF) $(abspath $(2))));
    $(QUIETLY) rm -r $(TMP);
    $(QUIETLY) cp $(1) $(2);
endef

define build_and_jar
    $(info Building $(4))
    $(eval TMP := $(shell mktemp -d JVMCI_HOTSPOT_XXXXX))
    $(QUIETLY) $(JAVAC) -d $(TMP) -processorpath :$(1) -bootclasspath $(JDK_BOOTCLASSPATH) -cp :$(2) $(filter %.java,$?);
    $(QUIETLY) test "$(3)" = "" || cp -r $(3) $(TMP);
    $(QUIETLY) $(call process_options,$(TMP));
    $(QUIETLY) mkdir -p $(shell dirname $(4))
    $(QUIETLY) $(JAR) cf $(4) -C $(TMP) .
    $(QUIETLY) rm -r $(TMP);
endef

all: default

export: all
\t$(info Put $(EXPORTED_FILES) into SHARED_DIR $(SHARED_DIR))
\t$(QUIETLY) mkdir -p $(SHARED_DIR)
\t$(foreach export,$(EXPORTED_FILES),$(call extract,$(export),$(SHARED_DIR)))
.PHONY: export

""")
    s = mx.suite("graal")
    dists = []
    ap = []
    projects = []
    for d in s.dists:
        if d.name in selectedDists:
            update_list(dists, d.get_dist_deps(True, True))
            update_list(projects, d.sorted_deps(includeLibs=False, transitive=True))

    for p in projects:
        deps = p.all_deps([], False, includeSelf=True, includeJreLibs=False, includeAnnotationProcessors=True)
        for d in deps:
            if d.definedAnnotationProcessorsDist != None:
                apd = d.definedAnnotationProcessorsDist
                update_list(ap, [apd])

    if len(dists) > 0:
        mf.add_definition(jdkBootClassPathVariableName + " = " + bootClassPath)
        bootClassPathVarAccess = "$(" + jdkBootClassPathVariableName + ")"
        for d in ap: make_dist_rule(d, mf, bootClassPathVarAccess)
        for d in dists: make_dist_rule(d, mf, bootClassPathVarAccess)
        mf.add_rule("default: $({}_JAR)\n.PHONY: default".format("_JAR) $(".join([d.name for d in dists])))
        return True
    else:
        for d in dists:
            selectedDists.remove(d.name)
        print "Distribution(s) '" + "', '".join(selectedDists) + "' does not exist."
