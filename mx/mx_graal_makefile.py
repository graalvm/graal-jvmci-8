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
import mx, os
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
    args = parser.parse_args(args)
    
    if args.selectedDists == None or len(args.selectedDists) == 0:
        parser.print_help()
        return
    mf = Makefile()
    if do_build_makefile(mf, args.selectedDists):
        contents = mf.generate()
        if args.output == None:
            print contents
        else:
            with open(args.output, "w") as f:
                f.write(contents)
    

def filter_projects(deps, t):
    def typeFilter(project): # filters
        if isinstance(project, str):
            project = mx.dependency(project, True)
        return isinstance(project, t)
    return [d for d in deps if typeFilter(d)]


def make_dist_rule(dist, mf, bootClassPath=None):
    def path_dist_relative(p):
        return os.path.relpath(p, dist.suite.dir)
    jarPath = path_dist_relative(dist.path)
    sourcesVariableName = dist.name + "_SRC"
    depJarVariableName = dist.name + "_DEP_JARS";
    sources = []
    resources = []
    sortedDeps = dist.sorted_deps(True, transitive=False, includeAnnotationProcessors=True)
    projects = filter_projects(sortedDeps, mx.Project)
    targetPathPrefix = "$(TARGET)" + os.path.sep
    libraryDeps = [path_dist_relative(l.get_path(False)) for l in filter_projects(sortedDeps, mx.Library)]
    
    annotationProcessorDeps = set()
    distDeps = dist.get_dist_deps(includeSelf=False, transitive=True)
    distDepProjects = set()
    for d in distDeps: 
        distDepProjects.update(d.sorted_deps(includeLibs=False, transitive=True))
    classPath = [targetPathPrefix + path_dist_relative(d.path) for d in distDeps] + libraryDeps
    
    for p in projects:
        if p.definedAnnotationProcessors != None and p.definedAnnotationProcessorsDist != dist:
            annotationProcessorDeps.add(p)
    for p in projects:
        projectDir = path_dist_relative(p.dir)
        if p not in distDepProjects and p not in annotationProcessorDeps:
            generatedSource = [path_dist_relative(p.source_gen_dir())] if len(annotationProcessorDeps) > 0 else []
            
            for d in p.srcDirs + generatedSource:
                src = projectDir + os.path.sep + d
                sources.append("$(shell find {} -type file -name *.java 2> /dev/null)".format(src))
                metaInf = src + os.path.sep + "META-INF";
                if os.path.exists(metaInf):
                    resources.append(metaInf)
        
            
    sourceLines = sourcesVariableName + " = " + ("\n" + sourcesVariableName + " += ").join(sources)
    annotationProcessorPaths = []
    annotationProcessorDistNames = []
    annotationProcessorDistVariableNames = []
    for p in annotationProcessorDeps:
        annotationProcessorPaths.append(path_dist_relative(p.definedAnnotationProcessorsDist.path))
        name = p.definedAnnotationProcessorsDist.name
        annotationProcessorDistNames.append(name)
        annotationProcessorDistVariableNames.append("$(" + name + "_JAR)")
    
    props = {
           "name": dist.name,
           "jarPath": targetPathPrefix + jarPath,
           "depends": "",
           "depJarsVariableAccess": "$(" + depJarVariableName + ")" if len(classPath) > 0 else "",
           "depJarsVariable": depJarVariableName,
           "sourceLines": sourceLines,
           "sourcesVariableName": sourcesVariableName,
           "annotationProcessors": " ".join(annotationProcessorDistVariableNames),
           "cpAnnotationProcessors": "-processorpath " + ":".join(annotationProcessorDistVariableNames) if len(annotationProcessorDistVariableNames) > 0 else "",
           "bootCp": ("-bootclasspath " + bootClassPath) if bootClassPath != None else "",
           "cpDeps": ("-cp " + ":".join(classPath)) if len(classPath) > 0 else "",
           "jarDeps": " ".join(classPath),
           "copyResources": "cp -r {} $(TMP)".format(" ".join(resources)) if len(resources) > 0 else "",
           "targetPathPrefix": targetPathPrefix
           }
    
    mf.add_definition(sourceLines)
    mf.add_definition("{name}_JAR = {jarPath}".format(**props))
    if len(classPath) > 0: mf.add_definition("{depJarsVariable} = {jarDeps}".format(**props))
    mf.add_rule("""$({name}_JAR): $({sourcesVariableName}) {annotationProcessors} {depJarsVariableAccess} $(TARGET)/build
\t$(eval TMP := $(shell mktemp -d))
\t$(JAVAC) -d $(TMP) {cpAnnotationProcessors} {bootCp} {cpDeps} $({sourcesVariableName})
\t{copyResources}
\t$(call process_options,$(TMP))
\tmkdir -p $$(dirname $({name}_JAR))
\t$(JAR) cf $({name}_JAR) -C $(TMP) .
\trm -r $(TMP)""".format(**props))
    return


def do_build_makefile(mf, selectedDists):
    java = mx.java()
    bootClassPath = java.bootclasspath()
    bootClassPath = bootClassPath.replace(java.jdk, "$(JDK)")
    jdkBootClassPathVariableName = "JDK_BOOTCLASSPATH"
    
    mf.add_definition("""VERBOSE=
TARGET=.
JDK=

WGET=wget
JAVAC=$(JDK)/bin/javac -g -target """ + str(java.javaCompliance) + """
JAR=$(JDK)/bin/jar


ifeq ($(JDK),)
$(error Variable JDK must be set to a JDK installation.)
endif
ifneq ($(VERBOSE),)
SHELL=sh -x
endif

define process_options =
$(eval providers=$(1)/META-INF/providers/)
$(eval services=$(1)/META-INF/services/)
test -d $(services) || mkdir -p $(services)
test ! -d $(providers) ||   (cd $(providers) && for i in $$(ls $(providers)); do c=$$(cat $$i); echo $$i >> $(services)$$c; rm $$i; done)
endef

all: default

$(TARGET)/build:
\tmkdir -p $(TARGET)/build

$(LIB):
\tmkdir -p $(LIB)
""")
    s = mx.suite("graal")
    dists = set()
    ap = set()
    projects = set()

    for d in s.dists:
        if d.name in selectedDists:
            dists.update(d.get_dist_deps(True, True))
            projects.update(d.sorted_deps(includeLibs=False, transitive=True))
    for p in projects:
        deps = p.all_deps([], False, includeSelf=True, includeJreLibs=False, includeAnnotationProcessors=True)
        for d in deps:
            if d.definedAnnotationProcessorsDist != None:
                apd = d.definedAnnotationProcessorsDist
                ap.add(apd)
    
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
            
    
            
    

