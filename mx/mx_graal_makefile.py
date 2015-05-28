import mx, os, sys
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


def build_makefile(args):
    """Build a Makefile from the suitte.py to build graa.jar without python"""
    if len(args) == 0 or args[0] == "-":
        do_build_makefile(lambda l: sys.stdout.write(l + os.linesep))
    elif args[0] == "-o":
        with open(args[1], "w") as f:
            do_build_makefile(lambda l: f.write(l + os.linesep))

def relative_dep_path(d):
    if isinstance(d, str): d = mx.dependency(d)
    return os.path.basename(d.get_path(False))

def createMakeRule(p, bootClasspath):
    def filterDeps(deps, t):
        def typeFilter(project): # filters
            if isinstance(project, str):
                project = mx.dependency(project, True)
            return isinstance(project, t)
        return [d for d in deps if typeFilter(d)]


    canonicalDeps = p.canonical_deps()
    canonicalProjectDep = filterDeps(canonicalDeps, mx.Project)
    canonicalProjectDepDirs = ['$(TARGET)/' +i for i in canonicalProjectDep]
    canonicalLibDep = filterDeps(canonicalDeps, mx.Library)
    canonicalLibDepJars = ["$(LIB)/" + relative_dep_path(d) for d in canonicalLibDep]

    allDep = p.all_deps([], True, False, includeAnnotationProcessors=True)
    allProcessorDistNames = [x.definedAnnotationProcessorsDist.name for x in filterDeps(allDep, mx.Project) if x.definedAnnotationProcessors != None]
    allProjectDep = filterDeps(allDep, mx.Project)
    allProjectDepDir = ['$(TARGET)/' +i.name for i in allProjectDep]
    allLibDep = filterDeps(allDep, mx.Library)
    allLibDepJar = ["$(LIB)/" + relative_dep_path(d) for d in allLibDep]

    processor = p.annotation_processors_path()
    if processor != None: processor = processor.replace(p.suite.dir, "$(TARGET)")

    cp = allLibDepJar +allProjectDepDir
    props = {
             'name': p.name,
             'project_deps': ' '.join(canonicalProjectDepDirs + canonicalLibDepJars + allProcessorDistNames),
             'cp_deps': ('-cp ' + ':'.join(cp)) if len(cp) > 0 else '',
             'cp_boot': ('-bootclasspath ' + bootClasspath) if len(bootClasspath) > 0 else '',
             'processor': ('-processorpath ' + processor) if processor != None else ''
    }
    return """$(TARGET)/{name}: $(shell find graal/{name}/src/ -type f -name *.java) {project_deps}
\t$(eval TMP := $(shell mktemp -d))
\ttest ! -d $(TARGET)/{name} || cp -Rp $(TARGET)/{name} $(TMP)
\t$(JAVAC) -d $(TMP) {cp_boot} {processor} {cp_deps} $(shell find graal/{name}/src/ -type f -name *.java)
\ttest ! -d graal/{name}/src/META-INF || (mkdir -p $(TARGET)/{name}/META-INF/ &&  cp -r graal/{name}/src/META-INF/ $(TARGET)/{name}/)
\tmkdir -p $(TARGET)/{name}
\tcp -r $(TMP)/* $(TARGET)/{name}
\ttouch $(TARGET)/{name}
\trm -r $(TMP)
""".format(**props)

def createDistributionRule(dist):
    sorted_deps = set(dist.sorted_deps(False, True))
    depDirs = ' '.join(['$(TARGET)/' + i.name for i in sorted_deps])
    depDirsStar = ' '.join(['$(TARGET)/' + i.name + '/*' for i in sorted_deps])
    jarPath = os.path.relpath(dist.path, dist.suite.dir)
    jarDir = os.path.dirname(jarPath)
    props = {
             'dist_name': dist.name,
             'depDirs': depDirs,
             'depDirsStar': depDirsStar,
             'jar_path': jarPath,
             'jar_dir': jarDir,
             'providers_dir': '$(TMP)/META-INF/providers/ ',
             'services_dir': '$(TMP)/META-INF/services/'
             }
    return """{dist_name}: {depDirs}
\t$(eval TMP := $(shell mktemp -d))
\tmkdir -p $(TARGET){jar_dir}
\ttouch $(TARGET)/{jar_path}
\tcp -r {depDirsStar} $(TMP)
\ttest -d {services_dir} || mkdir -p {services_dir} 
\ttest ! -d {providers_dir} || (cd {providers_dir} && for i in $$(ls); do c=$$(cat $$i); echo $$i >> {services_dir}$$c; done)
\ttest ! -d {providers_dir} || rm -r {providers_dir}
\t$(JAR) cvf $(TARGET){jar_path} -C $(TMP) .
\trm -r $(TMP)
""".format(**props)

def createDownloadRule(lib):
    http_urls = [u for u in lib.urls if u.startswith("http")]
    if len(http_urls) == 0: http_urls = [u for u in lib.urls if u.startswith("jar")]
    if len(http_urls) == 0: raise BaseException("No http url specified for downloading library %s: available urls: %s" % (lib.name, lib.urls))
    url = http_urls[0]
    tofile = '$(LIB)/' + relative_dep_path(lib)
    if url.startswith("jar"):
        props = {
            'url': url[url.find(":")+1:url.rfind("!")],
            'archive_file': url[url.rfind("!")+1:],
            'dest': tofile
        }
        dl = """\t$(eval TMP := $(shell mktemp -d))
\tcd $(TMP) && $(WGET) -O dl.zip {url} && $(JAR) xf dl.zip
\tmv $(TMP)/{archive_file} {dest}
\trm -rf $(TMP)""".format(**props)
    else:
        dl = "\t$(WGET) -O {} {}".format(tofile, url)
    return """{}:\n{}""".format(tofile, dl)


def create_suite_build(suite, out):
    for p in suite.projects:
        java = mx.java(p.javaCompliance)
        bootClassPath = java.bootclasspath()
        bootClassPath = bootClassPath.replace(java.jdk, "$(JDK)")
        out(createMakeRule(p, bootClassPath))
    for l in suite.libs:
        out(createDownloadRule(l))

    distributionNames = []
    for d in suite.dists:
        distributionNames.append(d.name)
        out(createDistributionRule(d))
    out("{0}: {1}\n.PHONY: {1}".format(suite.name, " ".join(distributionNames)))


def do_build_makefile(out):
    out("""VERBOSE=
TARGET=build/
LIB=$(TARGET)/lib
JDK=

WGET=wget
JAVAC=$(JDK)/bin/javac
JAR=$(JDK)/bin/jar


ifeq ($(JDK),)
$(error Variable JDK must be set to a JDK installation.)
endif
ifneq ($(VERBOSE),)
SHELL=sh -x
endif

all: default

$(TARGET):
\tmkdir -p $(TARGET)

$(LIB):
\tmkdir -p $(LIB)
""")
    suiteNames = []
    for s in mx.suites():
        suiteNames.append(s.name)
        create_suite_build(s, out)

    out("""default: $(TARGET) $(LIB) {0}
.PHONY: {0}
    """.format(" ".join(suiteNames)))

