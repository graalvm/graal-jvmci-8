#
# ----------------------------------------------------------------------------------------------------
#
# Copyright (c) 2007, 2015, Oracle and/or its affiliates. All rights reserved.
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

import os, stat, errno, sys, shutil, zipfile, tarfile, tempfile, re, time, datetime, platform, subprocess, socket
from os.path import join, exists, dirname, basename
from argparse import ArgumentParser, REMAINDER
import xml.dom.minidom
import json, textwrap

import mx
import mx_unittest
from mx_unittest import unittest
from mx_gate import Task
import mx_gate
import mx_jvmci_makefile

_suite = mx.suite('jvmci')

""" The VMs that can be built and run along with an optional description. Only VMs with a
    description are listed in the dialogue for setting the default VM (see get_vm()). """
_vmChoices = {
    'jvmci' : 'VM triggered compilation is performed with a tiered system (C1 + Graal) and Graal is available for hosted compilation.',
    'server' : 'Normal compilation is performed with a tiered system (C1 + C2) and Graal is available for hosted compilation.',
    'client' : None,  # VM compilation with client compiler, hosted compilation with Graal
    'server-nojvmci' : None,  # all compilation with tiered system (i.e., client + server), JVMCI omitted
    'client-nojvmci' : None,  # all compilation with client compiler, JVMCI omitted
    'original' : None,  # default VM copied from bootstrap JDK
    'graal' : None, # alias for jvmci
    'server-nograal' : None,  # alias for server-nojvmci
    'client-nograal' : None,  # alias for client-nojvmci
}

""" The VM that will be run by the 'vm' command and built by default by the 'build' command.
    This can be set via the global '--vm' option or the DEFAULT_VM environment variable.
    It can also be temporarily set by using of a VM context manager object in a 'with' statement. """
_vm = None

""" The VM builds that will be run by the 'vm' command - default is first in list """
_vmbuildChoices = ['product', 'fastdebug', 'debug', 'optimized']

""" The VM build that will be run by the 'vm' command.
    This can be set via the global '--vmbuild' option.
    It can also be temporarily set by using of a VM context manager object in a 'with' statement. """
_vmbuild = _vmbuildChoices[0]

""" The current working directory to switch to before running the VM. """
_vm_cwd = None

""" The base directory in which the JDKs cloned from $JAVA_HOME exist. """
_installed_jdks = None

""" Prefix for running the VM. """
_vm_prefix = None

_make_eclipse_launch = False

_minVersion = mx.VersionSpec('1.8')

# max version (first _unsupported_ version)
_untilVersion = None

class JDKDeployedDist(object):
    def __init__(self, name):
        self._name = name

    def dist(self):
        return mx.distribution(self._name)

    def deploy(self, jdkDir):
        mx.nyi('deploy', self)

class JarJDKDeployedDist(JDKDeployedDist):
    def __init__(self, name, partOfHotSpot=False):
        JDKDeployedDist.__init__(self, name)
        self.partOfHotSpot = partOfHotSpot

    def targetDir(self):
        mx.nyi('targetDir', self)

    def _copyToJdk(self, jdkDir, target):
        targetDir = join(jdkDir, target)
        dist = self.dist()
        mx.logv('Deploying {} to {}'.format(dist.name, targetDir))
        copyToJdk(dist.path, targetDir)
        if dist.sourcesPath:
            copyToJdk(dist.sourcesPath, jdkDir)

    def deploy(self, jdkDir):
        self._copyToJdk(jdkDir, self.targetDir())

class ExtJDKDeployedDist(JarJDKDeployedDist):
    def __init__(self, name, partOfHotSpot=False):
        JarJDKDeployedDist.__init__(self, name, partOfHotSpot)

    def targetDir(self):
        return join('jre', 'lib', 'ext')

class LibJDKDeployedDist(JarJDKDeployedDist):
    def __init__(self, name, partOfHotSpot=False):
        JarJDKDeployedDist.__init__(self, name, partOfHotSpot)

    def targetDir(self):
        return join('jre', 'lib')

class JvmciJDKDeployedDist(JarJDKDeployedDist):
    def __init__(self, name, partOfHotSpot=False):
        JarJDKDeployedDist.__init__(self, name, partOfHotSpot)

    def targetDir(self):
        return join('jre', 'lib', 'jvmci')

    def deploy(self, jdkDir):
        JarJDKDeployedDist.deploy(self, jdkDir)
        _updateJVMCIFiles(jdkDir)

def _exe(l):
    return mx.exe_suffix(l)

def _lib(l):
    return mx.add_lib_suffix(mx.add_lib_prefix(l))

def _lib_dbg(l):
    return mx.add_debug_lib_suffix(mx.add_lib_prefix(l))

class HotSpotVMJDKDeployedDist(JDKDeployedDist):
    def dist(self):
        name = mx.instantiatedDistributionName(self._name, dict(vm=get_vm(), vmbuild=_vmbuild), context=self._name)
        return mx.distribution(name)

    def deploy(self, jdkDir):
        _hs_deploy_map = {
            'jvmti.h' : 'include',
            'sa-jdi.jar' : 'lib',
            _lib('jvm') : join(relativeVmLibDirInJdk(), get_vm()),
            _lib_dbg('jvm') : join(relativeVmLibDirInJdk(), get_vm()),
            _lib('saproc') : relativeVmLibDirInJdk(),
            _lib_dbg('saproc') : relativeVmLibDirInJdk(),
            _lib('jsig') : relativeVmLibDirInJdk(),
            _lib_dbg('jsig') : relativeVmLibDirInJdk(),
        }
        dist = self.dist()
        with tarfile.open(dist.path, 'r') as tar:
            for m in tar.getmembers():
                if m.name in _hs_deploy_map:
                    targetDir = join(jdkDir, _hs_deploy_map[m.name])
                    mx.logv('Deploying {} from {} to {}'.format(m.name, dist.name, targetDir))
                    tar.extract(m, targetDir)
        updateJvmCfg(jdkDir, get_vm())
"""
List of distributions that are deployed into a JDK by mx.
"""
jdkDeployedDists = [
    LibJDKDeployedDist('JVMCI_SERVICE', partOfHotSpot=True),
    JvmciJDKDeployedDist('JVMCI_API', partOfHotSpot=True),
    JvmciJDKDeployedDist('JVMCI_HOTSPOT', partOfHotSpot=True),
    JvmciJDKDeployedDist('JVMCI_HOTSPOTVMCONFIG', partOfHotSpot=True),
    JvmciJDKDeployedDist('JVMCI_OPTIONS', partOfHotSpot=True),
    HotSpotVMJDKDeployedDist('JVM_<vmbuild>_<vm>'),
]

JDK_UNIX_PERMISSIONS_DIR = 0755
JDK_UNIX_PERMISSIONS_FILE = 0644
JDK_UNIX_PERMISSIONS_EXEC = 0755

def isVMSupported(vm):
    if 'client' == vm and len(platform.mac_ver()[0]) != 0:
        # Client VM not supported: java launcher on Mac OS X translates '-client' to '-server'
        return False
    return True

def get_vm_cwd():
    """
    Get the current working directory to switch to before running the VM.
    """
    return _vm_cwd

def get_installed_jdks():
    """
    Get the base directory in which the JDKs cloned from $JAVA_HOME exist.
    """
    return _installed_jdks

def get_vm_prefix():
    """
    Get the prefix for running the VM ("/usr/bin/gdb --args").
    """
    return _vm_prefix

def get_vm_choices():
    return _vmChoices

def get_vm():
    """
    Gets the configured VM, presenting a dialogue if there is no currently configured VM.
    """
    global _vm
    if _vm:
        return _vm
    vm = mx.get_env('DEFAULT_VM')
    envPath = join(_suite.mxDir, 'env')
    if vm and 'graal' in vm:
        if exists(envPath):
            with open(envPath) as fp:
                if 'DEFAULT_VM=' + vm in fp.read():
                    mx.log('Please update the DEFAULT_VM value in ' + envPath + ' to replace "graal" with "jvmci"')
        vm = vm.replace('graal', 'jvmci')
    if vm is None:
        if not mx.is_interactive():
            mx.abort('Need to specify VM with --vm option or DEFAULT_VM environment variable')
        mx.log('Please select the VM to be executed from the following: ')
        items = [k for k in _vmChoices.keys() if _vmChoices[k] is not None]
        descriptions = [_vmChoices[k] for k in _vmChoices.keys() if _vmChoices[k] is not None]
        vm = mx.select_items(items, descriptions, allowMultiple=False)
        mx.ask_persist_env('DEFAULT_VM', vm)
    _vm = vm
    return vm

"""
A context manager that can be used with the 'with' statement to set the VM
used by all VM executions within the scope of the 'with' statement. For example:

    with VM('server'):
        dacapo(['pmd'])
"""
class VM:
    def __init__(self, vm=None, build=None):
        assert vm is None or vm in _vmChoices.keys()
        assert build is None or build in _vmbuildChoices
        self.vm = vm if vm else _vm
        self.build = build if build else _vmbuild

    def __enter__(self):
        global _vm, _vmbuild
        self.previousVm = _vm
        self.previousBuild = _vmbuild
        mx.reInstantiateDistribution('JVM_<vmbuild>_<vm>', dict(vm=self.previousVm, vmbuild=self.previousBuild), dict(vm=self.vm, vmbuild=self.build))
        _vm = self.vm
        _vmbuild = self.build

    def __exit__(self, exc_type, exc_value, traceback):
        global _vm, _vmbuild
        mx.reInstantiateDistribution('JVM_<vmbuild>_<vm>', dict(vm=self.vm, vmbuild=self.build), dict(vm=self.previousVm, vmbuild=self.previousBuild))
        _vm = self.previousVm
        _vmbuild = self.previousBuild

def chmodRecursive(dirname, chmodFlagsDir):
    if mx.get_os() == 'windows':
        return

    def _chmodDir(chmodFlags, dirname, fnames):
        os.chmod(dirname, chmodFlagsDir)

    os.path.walk(dirname, _chmodDir, chmodFlagsDir)

def export(args):
    """create archives of builds split by vmbuild and vm"""

    parser = ArgumentParser(prog='mx export')
    args = parser.parse_args(args)

    # collect data about export
    infos = dict()
    infos['timestamp'] = time.time()

    hgcfg = mx.HgConfig()
    hgcfg.check()
    infos['revision'] = hgcfg.tip('.') + ('+' if hgcfg.isDirty('.') else '')
    # TODO: infos['repository']

    infos['jdkversion'] = str(mx.get_jdk().version)

    infos['architecture'] = mx.get_arch()
    infos['platform'] = mx.get_os()

    if mx.get_os != 'windows':
        pass
        # infos['ccompiler']
        # infos['linker']

    infos['hostname'] = socket.gethostname()

    def _writeJson(suffix, properties):
        d = infos.copy()
        for k, v in properties.iteritems():
            assert not d.has_key(k)
            d[k] = v

        jsonFileName = 'export-' + suffix + '.json'
        with open(jsonFileName, 'w') as f:
            print >> f, json.dumps(d)
        return jsonFileName


    def _genFileName(archivtype, middle):
        idPrefix = infos['revision'] + '_'
        idSuffix = '.tar.gz'
        return join(_suite.dir, "graalvm_" + archivtype + "_" + idPrefix + middle + idSuffix)

    def _genFileArchPlatformName(archivtype, middle):
        return _genFileName(archivtype, infos['platform'] + '_' + infos['architecture'] + '_' + middle)


    # archive different build types of hotspot
    for vmBuild in _vmbuildChoices:
        jdkpath = join(_jdksDir(), vmBuild)
        if not exists(jdkpath):
            mx.logv("skipping " + vmBuild)
            continue

        tarName = _genFileArchPlatformName('basejdk', vmBuild)
        mx.logv("creating basejdk " + tarName)
        vmSet = set()
        with tarfile.open(tarName, 'w:gz') as tar:
            for root, _, files in os.walk(jdkpath):
                if basename(root) in _vmChoices.keys():
                    # TODO: add some assert to check path assumption
                    vmSet.add(root)
                    continue

                for f in files:
                    name = join(root, f)
                    # print name
                    tar.add(name, name)

            n = _writeJson("basejdk-" + vmBuild, {'vmbuild' : vmBuild})
            tar.add(n, n)

        # create a separate archive for each VM
        for vm in vmSet:
            bVm = basename(vm)
            vmTarName = _genFileArchPlatformName('vm', vmBuild + '_' + bVm)
            mx.logv("creating vm " + vmTarName)

            debugFiles = set()
            with tarfile.open(vmTarName, 'w:gz') as tar:
                for root, _, files in os.walk(vm):
                    for f in files:
                        # TODO: mac, windows, solaris?
                        if any(map(f.endswith, [".debuginfo"])):
                            debugFiles.add(f)
                        else:
                            name = join(root, f)
                            # print name
                            tar.add(name, name)

                n = _writeJson("vm-" + vmBuild + "-" + bVm, {'vmbuild' : vmBuild, 'vm' : bVm})
                tar.add(n, n)

            if len(debugFiles) > 0:
                debugTarName = _genFileArchPlatformName('debugfilesvm', vmBuild + '_' + bVm)
                mx.logv("creating debugfilesvm " + debugTarName)
                with tarfile.open(debugTarName, 'w:gz') as tar:
                    for f in debugFiles:
                        name = join(root, f)
                        # print name
                        tar.add(name, name)

                    n = _writeJson("debugfilesvm-" + vmBuild + "-" + bVm, {'vmbuild' : vmBuild, 'vm' : bVm})
                    tar.add(n, n)

    # jvmci directory
    jvmciDirTarName = _genFileName('classfiles', 'javac')
    mx.logv("creating jvmci " + jvmciDirTarName)
    with tarfile.open(jvmciDirTarName, 'w:gz') as tar:
        for root, _, files in os.walk("jvmci"):
            for f in [f for f in files if not f.endswith('.java')]:
                name = join(root, f)
                # print name
                tar.add(name, name)

        n = _writeJson("jvmci", {'javacompiler' : 'javac'})
        tar.add(n, n)

def relativeVmLibDirInJdk():
    mxos = mx.get_os()
    if mxos == 'darwin':
        return join('jre', 'lib')
    if mxos == 'windows' or mxos == 'cygwin':
        return join('jre', 'bin')
    return join('jre', 'lib', mx.get_arch())

def vmLibDirInJdk(jdk):
    """
    Gets the directory within a JDK where the server and client
    sub-directories are located.
    """
    return join(jdk, relativeVmLibDirInJdk())

def getVmJliLibDirs(jdk):
    """
    Get the directories within a JDK where the jli library designates to.
    """
    mxos = mx.get_os()
    if mxos == 'darwin':
        return [join(jdk, 'jre', 'lib', 'jli')]
    if mxos == 'windows' or mxos == 'cygwin':
        return [join(jdk, 'jre', 'bin'), join(jdk, 'bin')]
    return [join(jdk, 'jre', 'lib', mx.get_arch(), 'jli'), join(jdk, 'lib', mx.get_arch(), 'jli')]

def getVmCfgInJdk(jdk, jvmCfgFile='jvm.cfg'):
    """
    Get the jvm.cfg file.
    """
    mxos = mx.get_os()
    if mxos == "windows" or mxos == "cygwin":
        return join(jdk, 'jre', 'lib', mx.get_arch(), jvmCfgFile)
    return join(vmLibDirInJdk(jdk), jvmCfgFile)

def _jdksDir():
    return os.path.abspath(join(_installed_jdks if _installed_jdks else _suite.dir, 'jdk' + str(mx.get_jdk().version)))

def _handle_missing_VM(bld, vm=None):
    if not vm:
        vm = get_vm()
    mx.log('The ' + bld + ' ' + vm + ' VM has not been created')
    if mx.is_interactive():
        if mx.ask_yes_no('Build it now', 'y'):
            with VM(vm, bld):
                build([])
            return
    mx.abort('You need to run "mx --vm ' + vm + ' --vmbuild ' + bld + ' build" to build the selected VM')

def get_jvmci_jdk(build=None, vmToCheck=None, create=False, installJars=True):
    """
    Gets the JDK into which JVMCI is installed, creating it first if necessary.
    """
    if not build:
        build = _vmbuild
    jdk = join(_jdksDir(), build)
    if create:
        srcJdk = mx.get_jdk().home
        if not exists(jdk):
            mx.log('Creating ' + jdk + ' from ' + srcJdk)
            shutil.copytree(srcJdk, jdk)

            # Make a copy of the default VM so that this JDK can be
            # reliably used as the bootstrap for a HotSpot build.
            jvmCfg = getVmCfgInJdk(jdk)
            if not exists(jvmCfg):
                mx.abort(jvmCfg + ' does not exist')

            defaultVM = None
            jvmCfgLines = []
            with open(jvmCfg) as f:
                for line in f:
                    if line.startswith('-') and defaultVM is None:
                        parts = line.split()
                        if len(parts) == 2:
                            assert parts[1] == 'KNOWN', parts[1]
                            defaultVM = parts[0][1:]
                            jvmCfgLines += ['# default VM is a copy of the unmodified ' + defaultVM + ' VM\n']
                            jvmCfgLines += ['-original KNOWN\n']
                        else:
                            # skip lines which we cannot parse (e.g. '-hotspot ALIASED_TO -client')
                            mx.log("WARNING: skipping not parsable line \"" + line + "\"")
                    else:
                        jvmCfgLines += [line]

            assert defaultVM is not None, 'Could not find default VM in ' + jvmCfg
            chmodRecursive(jdk, JDK_UNIX_PERMISSIONS_DIR)
            shutil.move(join(vmLibDirInJdk(jdk), defaultVM), join(vmLibDirInJdk(jdk), 'original'))

            if mx.get_os() != 'windows':
                os.chmod(jvmCfg, JDK_UNIX_PERMISSIONS_FILE)
            with open(jvmCfg, 'w') as fp:
                for line in jvmCfgLines:
                    fp.write(line)

            # patch 'release' file (append jvmci revision)
            releaseFile = join(jdk, 'release')
            if exists(releaseFile):
                releaseFileLines = []
                with open(releaseFile) as f:
                    for line in f:
                        releaseFileLines.append(line)

                if mx.get_os() != 'windows':
                    os.chmod(releaseFile, JDK_UNIX_PERMISSIONS_FILE)
                with open(releaseFile, 'w') as fp:
                    for line in releaseFileLines:
                        if line.startswith("SOURCE="):
                            try:
                                sourceLine = line[0:-2]  # remove last char
                                hgcfg = mx.HgConfig()
                                hgcfg.check()
                                revision = hgcfg.tip('.')[:12]  # take first 12 chars
                                fp.write(sourceLine + ' jvmci:' + revision + '\"\n')
                            except:
                                fp.write(line)
                        else:
                            fp.write(line)

            # Install a copy of the disassembler library
            try:
                hsdis([], copyToDir=vmLibDirInJdk(jdk))
            except SystemExit:
                pass
    else:
        if not exists(jdk):
            if _installed_jdks:
                mx.log("The selected JDK directory does not (yet) exist: " + jdk)
            _handle_missing_VM(build, vmToCheck)

    if installJars:
        for jdkDist in jdkDeployedDists:
            dist = jdkDist.dist()
            if exists(dist.path):
                _installDistInJdks(jdkDist)

    if vmToCheck is not None:
        jvmCfg = getVmCfgInJdk(jdk)
        found = False
        with open(jvmCfg) as f:
            for line in f:
                if line.strip() == '-' + vmToCheck + ' KNOWN':
                    found = True
                    break
        if not found:
            _handle_missing_VM(build, vmToCheck)

    return jdk

def _updateInstalledJVMCIOptionsFile(jdk):
    jvmciOptions = join(_suite.dir, 'jvmci.options')
    jreLibDir = join(jdk, 'jre', 'lib')
    if exists(jvmciOptions):
        shutil.copy(jvmciOptions, join(jreLibDir, 'jvmci.options'))
    else:
        toDelete = join(jreLibDir, 'jvmci.options')
        if exists(toDelete):
            os.unlink(toDelete)

def copyToJdk(src, dst, permissions=JDK_UNIX_PERMISSIONS_FILE):
    name = os.path.basename(src)
    if not exists(dst):
        os.makedirs(dst)
    dstLib = join(dst, name)
    if mx.get_env('SYMLINK_GRAAL_JAR', None) == 'true':
        # Using symlinks is much faster than copying but may
        # cause issues if the lib is being updated while
        # the VM is running.
        if not os.path.islink(dstLib) or not os.path.realpath(dstLib) == src:
            if exists(dstLib):
                os.remove(dstLib)
            os.symlink(src, dstLib)
    else:
        # do a copy and then a move to get atomic updating (on Unix)
        fd, tmp = tempfile.mkstemp(suffix='', prefix=name, dir=dst)
        shutil.copyfile(src, tmp)
        os.close(fd)
        shutil.move(tmp, dstLib)
        os.chmod(dstLib, permissions)

def _extractJVMCIFiles(jdkJars, jvmciJars, servicesDir, optionsDir, obsoleteCheck):

    oldServices = os.listdir(servicesDir) if exists(servicesDir) else os.makedirs(servicesDir)
    oldOptions = os.listdir(optionsDir) if exists(optionsDir) else os.makedirs(optionsDir)

    jvmciServices = {}
    optionsFiles = []
    for jar in jvmciJars:
        if os.path.isfile(jar):
            with zipfile.ZipFile(jar) as zf:
                for member in zf.namelist():
                    if member.startswith('META-INF/jvmci.services/') and member != 'META-INF/jvmci.services/':
                        service = basename(member)
                        assert service != "", member
                        with zf.open(member) as serviceFile:
                            providers = jvmciServices.setdefault(service, [])
                            for line in serviceFile.readlines():
                                line = line.strip()
                                if line and line not in providers:
                                    providers.append(line)
                    elif member.startswith('META-INF/jvmci.options/') and member != 'META-INF/jvmci.options/':
                        filename = basename(member)
                        assert filename != "", member
                        targetpath = join(optionsDir, filename)
                        optionsFiles.append(filename)
                        with zf.open(member) as optionsFile, \
                             file(targetpath, "wb") as target:
                            if not obsoleteCheck:
                                shutil.copyfileobj(optionsFile, target)
                            if oldOptions and filename in oldOptions:
                                oldOptions.remove(filename)
    for service, providers in jvmciServices.iteritems():
        if not obsoleteCheck:
            fd, tmp = tempfile.mkstemp(prefix=service)
            f = os.fdopen(fd, 'w+')
            for provider in providers:
                f.write(provider + os.linesep)
            target = join(servicesDir, service)
            f.close()
            shutil.move(tmp, target)
            if mx.get_os() != 'windows':
                os.chmod(target, JDK_UNIX_PERMISSIONS_FILE)
        if oldServices and service in oldServices:
            oldServices.remove(service)

    if obsoleteCheck:
        for d, files in [(servicesDir, oldServices), (optionsDir, oldOptions)]:
            if files:
                print 'These files in ' + d + ' look obsolete:\n  ' + '\n  '.join(files)
                if  mx.is_interactive() and mx.ask_yes_no('Delete them', 'n'):
                    for f in files:
                        path = join(d, f)
                        os.remove(path)
                        mx.log('Deleted ' + path)

def _updateJVMCIFiles(jdkDir, obsoleteCheck=False):
    jreJVMCIDir = join(jdkDir, 'jre', 'lib', 'jvmci')
    jvmciJars = [join(jreJVMCIDir, e) for e in os.listdir(jreJVMCIDir) if e.endswith('.jar')]
    jreJVMCIServicesDir = join(jreJVMCIDir, 'services')
    jreJVMCIOptionsDir = join(jreJVMCIDir, 'options')
    _extractJVMCIFiles(_getJdkDeployedJars(jdkDir), jvmciJars, jreJVMCIServicesDir, jreJVMCIOptionsDir, obsoleteCheck)

def _installDistInJdks(deployableDist):
    """
    Installs the jar(s) for a given Distribution into all existing JVMCI JDKs
    """
    jdks = _jdksDir()
    if exists(jdks):
        for e in os.listdir(jdks):
            jdkDir = join(jdks, e)
            deployableDist.deploy(jdkDir)

def _check_for_obsolete_jvmci_files():
    jdks = _jdksDir()
    if exists(jdks):
        for e in os.listdir(jdks):
            jdkDir = join(jdks, e)
            _updateJVMCIFiles(jdkDir, obsoleteCheck=True)

def _getJdkDeployedJars(jdkDir):
    """
    Gets jar paths for all deployed distributions in the context of
    a given JDK directory.
    """
    jars = []
    for dist in jdkDeployedDists:
        if not isinstance(dist, JarJDKDeployedDist):
            continue
        jar = basename(dist.dist().path)
        jars.append(join(dist.targetDir(), jar))
    return jars


# run a command in the windows SDK Debug Shell
def _runInDebugShell(cmd, workingDir, logFile=None, findInOutput=None, respondTo=None):
    if respondTo is None:
        respondTo = {}
    newLine = os.linesep
    startToken = 'RUNINDEBUGSHELL_STARTSEQUENCE'
    endToken = 'RUNINDEBUGSHELL_ENDSEQUENCE'

    winSDK = mx.get_env('WIN_SDK', 'C:\\Program Files\\Microsoft SDKs\\Windows\\v7.1\\')

    if not exists(mx._cygpathW2U(winSDK)):
        mx.abort("Could not find Windows SDK : '" + winSDK + "' does not exist")

    winSDKSetEnv = mx._cygpathW2U(join(winSDK, 'Bin', 'SetEnv.cmd'))
    if not exists(winSDKSetEnv):
        mx.abort("Invalid Windows SDK path (" + winSDK + ") : could not find Bin/SetEnv.cmd (you can use the WIN_SDK environment variable to specify an other path)")

    wincmd = 'cmd.exe /E:ON /V:ON /K "' + mx._cygpathU2W(winSDKSetEnv) + '"'
    p = subprocess.Popen(wincmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout = p.stdout
    stdin = p.stdin
    if logFile:
        log = open(logFile, 'w')
    ret = False

    def _writeProcess(s):
        stdin.write(s + newLine)

    _writeProcess("echo " + startToken)
    while True:
        # encoding may be None on windows plattforms
        if sys.stdout.encoding is None:
            encoding = 'utf-8'
        else:
            encoding = sys.stdout.encoding

        line = stdout.readline().decode(encoding)
        if logFile:
            log.write(line.encode('utf-8'))
        line = line.strip()
        mx.log(line)
        if line == startToken:
            _writeProcess('cd /D ' + workingDir + ' & ' + cmd + ' & echo ' + endToken)
        for regex in respondTo.keys():
            match = regex.search(line)
            if match:
                _writeProcess(respondTo[regex])
        if findInOutput:
            match = findInOutput.search(line)
            if match:
                ret = True
        if line == endToken:
            if not findInOutput:
                _writeProcess('echo ERRXXX%errorlevel%')
            else:
                break
        if line.startswith('ERRXXX'):
            if line == 'ERRXXX0':
                ret = True
            break
    _writeProcess("exit")
    if logFile:
        log.close()
    return ret

def jdkhome(vm=None):
    """return the JDK directory selected for the 'vm' command"""
    return get_jvmci_jdk(installJars=False)

def print_jdkhome(args, vm=None):
    """print the JDK directory selected for the 'vm' command"""
    print jdkhome(vm)

def buildvars(args):
    """describe the variables that can be set by the -D option to the 'mx build' commmand"""

    buildVars = {
        'ALT_BOOTDIR' : 'The location of the bootstrap JDK installation (default: ' + mx.get_jdk().home + ')',
        'ALT_OUTPUTDIR' : 'Build directory',
        'HOTSPOT_BUILD_JOBS' : 'Number of CPUs used by make (default: ' + str(mx.cpu_count()) + ')',
        'INSTALL' : 'Install the built VM into the JDK? (default: y)',
        'ZIP_DEBUGINFO_FILES' : 'Install zipped debug symbols file? (default: 0)',
    }

    mx.log('HotSpot build variables that can be set by the -D option to "mx build":')
    mx.log('')
    for n in sorted(buildVars.iterkeys()):
        mx.log(n)
        mx.log(textwrap.fill(buildVars[n], initial_indent='    ', subsequent_indent='    ', width=200))

    mx.log('')
    mx.log('Note that these variables can be given persistent values in the file ' + join(_suite.mxDir, 'env') + ' (see \'mx about\').')

cached_graal_version = None

def _hotspotReplaceResultsVar(m):
    var = m.group(1)
    if var == 'os':
        return _hotspotOs(mx.get_os())
    if var == 'nojvmci':
        if get_vm().endswith('nojvmci'):
            return '-nojvmci'
        return ''
    if var == 'buildname':
        return _hotspotGetVariant()
    if var == 'vmbuild':
        return _vmbuild
    return mx._replaceResultsVar(m)

class HotSpotProject(mx.NativeProject):
    def __init__(self, suite, name, deps, workingSets, results, output, **args):
        mx.NativeProject.__init__(self, suite, name, "", "src", deps, workingSets, results, output, join(suite.dir, "src")) # TODO...

    def getOutput(self, replaceVar=_hotspotReplaceResultsVar):
        return mx.NativeProject.getOutput(self, replaceVar=replaceVar)

    def getResults(self, replaceVar=_hotspotReplaceResultsVar):
        return mx.NativeProject.getResults(self, replaceVar=replaceVar)

    def getBuildTask(self, args):
        return HotSpotBuildTask(self, args, _vmbuild, get_vm())

def _hotspotOs(mx_os):
    if mx_os == 'darwin':
        return 'bsd'
    return mx_os

def _hotspotGetVariant(vm=None):
    if not vm:
        vm = get_vm()
    variant = {'client': 'compiler1', 'server': 'compiler2', 'client-nojvmci': 'compiler1', 'server-nojvmci': 'compiler2'}.get(vm, vm)
    return variant

class HotSpotBuildTask(mx.NativeBuildTask):
    def __init__(self, project, args, vmbuild, vm):
        mx.NativeBuildTask.__init__(self, args, project)
        self.vm = vm
        self.vmbuild = vmbuild

    def __str__(self):
        return 'Building HotSpot[{}, {}]'.format(self.vmbuild, self.vm)

    def build(self):
        isWindows = platform.system() == 'Windows' or "CYGWIN" in platform.system()

        if self.vm.startswith('server'):
            buildSuffix = ''
        elif self.vm.startswith('client'):
            buildSuffix = '1'
        else:
            assert self.vm == 'jvmci', self.vm
            buildSuffix = 'jvmci'

        if isWindows:
            t_compilelogfile = mx._cygpathU2W(os.path.join(_suite.dir, "graalCompile.log"))
            mksHome = mx.get_env('MKS_HOME', 'C:\\cygwin\\bin')

            variant = _hotspotGetVariant(self.vm)
            project_config = variant + '_' + self.vmbuild
            jvmciHome = mx._cygpathU2W(_suite.dir)
            _runInDebugShell('msbuild ' + jvmciHome + r'\build\vs-amd64\jvm.vcproj /p:Configuration=' + project_config + ' /target:clean', jvmciHome)
            winCompileCmd = r'set HotSpotMksHome=' + mksHome + r'& set JAVA_HOME=' + mx._cygpathU2W(mx.get_jdk().home) + r'& set path=%JAVA_HOME%\bin;%path%;%HotSpotMksHome%& cd /D "' + jvmciHome + r'\make\windows"& call create.bat ' + jvmciHome
            print winCompileCmd
            winCompileSuccess = re.compile(r"^Writing \.vcxproj file:")
            if not _runInDebugShell(winCompileCmd, jvmciHome, t_compilelogfile, winCompileSuccess):
                mx.abort('Error executing create command')
            winBuildCmd = 'msbuild ' + jvmciHome + r'\build\vs-amd64\jvm.vcxproj /p:Configuration=' + project_config + ' /p:Platform=x64'
            if not _runInDebugShell(winBuildCmd, jvmciHome, t_compilelogfile):
                mx.abort('Error building project')
        else:
            def filterXusage(line):
                if not 'Xusage.txt' in line:
                    sys.stderr.write(line + os.linesep)
            cpus = self.parallelism
            makeDir = join(_suite.dir, 'make')
            runCmd = [mx.gmake_cmd(), '-C', makeDir]

            env = os.environ.copy()

            # These must be passed as environment variables
            env.setdefault('LANG', 'C')
            #env['JAVA_HOME'] = jdk

            def setMakeVar(name, default, env=None):
                """Sets a make variable on the command line to the value
                   of the variable in 'env' with the same name if defined
                   and 'env' is not None otherwise to 'default'
                """
                runCmd.append(name + '=' + (env.get(name, default) if env else default))

            if self.args.D:
                for nv in self.args.D:
                    name, value = nv.split('=', 1)
                    setMakeVar(name.strip(), value)

            setMakeVar('ARCH_DATA_MODEL', '64', env=env)
            setMakeVar('HOTSPOT_BUILD_JOBS', str(cpus), env=env)
            setMakeVar('ALT_BOOTDIR', mx.get_jdk().home, env=env)
            # setMakeVar("EXPORT_PATH", jdk)

            setMakeVar('MAKE_VERBOSE', 'y' if mx._opts.verbose else '')
            if self.vm.endswith('nojvmci'):
                setMakeVar('INCLUDE_JVMCI', 'false')
                setMakeVar('ALT_OUTPUTDIR', join(_suite.dir, 'build-nojvmci', _hotspotOs(mx.get_os())), env=env)
            else:
                version = _suite.release_version()
                setMakeVar('USER_RELEASE_SUFFIX', 'jvmci-' + version)
                setMakeVar('INCLUDE_JVMCI', 'true')
            # setMakeVar('INSTALL', 'y', env=env)
            if mx.get_os() == 'darwin' and platform.mac_ver()[0] != '':
                # Force use of clang on MacOS
                setMakeVar('USE_CLANG', 'true')
            if mx.get_os() == 'solaris':
                # If using sparcWorks, setup flags to avoid make complaining about CC version
                cCompilerVersion = subprocess.Popen('CC -V', stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True).stderr.readlines()[0]
                if cCompilerVersion.startswith('CC: Sun C++'):
                    compilerRev = cCompilerVersion.split(' ')[3]
                    setMakeVar('ENFORCE_COMPILER_REV', compilerRev, env=env)
                    setMakeVar('ENFORCE_CC_COMPILER_REV', compilerRev, env=env)
                    if self.vmbuild == 'jvmg':
                        # We want ALL the symbols when debugging on Solaris
                        setMakeVar('STRIP_POLICY', 'no_strip')
            # This removes the need to unzip the *.diz files before debugging in gdb
            setMakeVar('ZIP_DEBUGINFO_FILES', '0', env=env)

            if buildSuffix == "1":
                setMakeVar("BUILD_CLIENT_ONLY", "true")

            # Clear this variable as having it set can cause very confusing build problems
            env.pop('CLASSPATH', None)

            # Issue an env prefix that can be used to run the make on the command line
            if not mx._opts.verbose:
                mx.log('--------------- make command line ----------------------')

            envPrefix = ' '.join([key + '=' + env[key] for key in env.iterkeys() if not os.environ.has_key(key) or env[key] != os.environ[key]])
            if len(envPrefix):
                mx.log('env ' + envPrefix + ' \\')

            runCmd.append(self.vmbuild + buildSuffix)
            runCmd.append("docs")
            # runCmd.append("export_" + build)

            if not mx._opts.verbose:
                mx.log(' '.join(runCmd))
                mx.log('--------------------------------------------------------')
            mx.run(runCmd, err=filterXusage, env=env)
        self._newestOutput = None

    def needsBuild(self, newestInput):
        newestOutput = self.newestOutput()
        for d in ['src', 'make', join('jvmci', 'jdk.internal.jvmci.hotspot', 'src_gen', 'hotspot')]:  # TODO should this be replaced by a dependency to the project?
            for root, dirnames, files in os.walk(join(_suite.dir, d)):
                # ignore <graal>/src/share/tools
                if root == join(_suite.dir, 'src', 'share'):
                    dirnames.remove('tools')
                for f in (join(root, name) for name in files):
                    if len(f) != 0 and os.path.getmtime(f) > newestOutput:
                        return (True, 'out of date (witness: {})'.format(f))
        return (False, None)

    def buildForbidden(self):
        if mx.NativeBuildTask.buildForbidden(self):
            return True
        if self.vm == 'original':
            if self.vmbuild != 'product':
                mx.log('only product build of original VM exists')
            return True
        if not isVMSupported(self.vm):
            mx.log('The ' + self.vm + ' VM is not supported on this platform - skipping')
            return True
        return False

    def clean(self, forBuild=False):
        if forBuild:  # Let make handle incremental builds
            return
        def handleRemoveReadonly(func, path, exc):
            excvalue = exc[1]
            if mx.get_os() == 'windows' and func in (os.rmdir, os.remove) and excvalue.errno == errno.EACCES:
                os.chmod(path, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)  # 0777
                func(path)
            else:
                raise

        def rmIfExists(name):
            if os.path.isdir(name):
                shutil.rmtree(name, ignore_errors=False, onerror=handleRemoveReadonly)
            elif os.path.isfile(name):
                os.unlink(name)

        rmIfExists(join(_suite.dir, 'build'))
        rmIfExists(join(_suite.dir, 'build-nojvmci'))
        rmIfExists(_jdksDir())
        self._newestOutput = None

def build(args, vm=None):
    """build the VM binary

    The global '--vm' and '--vmbuild' options select which VM type and build target to build."""

    # Override to fail quickly if extra arguments are given
    # at the end of the command line. This allows for a more
    # helpful error message.
    class AP(ArgumentParser):
        def __init__(self):
            ArgumentParser.__init__(self, prog='mx build')
        def parse_args(self, args):
            result = ArgumentParser.parse_args(self, args)
            if len(result.remainder) != 0:
                firstBuildTarget = result.remainder[0]
                mx.abort('To specify the ' + firstBuildTarget + ' VM build target, you need to use the global "--vmbuild" option. For example:\n' +
                         '    mx --vmbuild ' + firstBuildTarget + ' build')
            return result

    # Call mx.build to compile the Java sources
    parser = AP()
    parser.add_argument('-D', action='append', help='set a HotSpot build variable (run \'mx buildvars\' to list variables)', metavar='name=value')

    # initialize jdk
    get_jvmci_jdk(create=True)

    mx.build(['--source', '1.7'] + args, parser=parser)


def updateJvmCfg(jdk, vm):
    jvmCfg = getVmCfgInJdk(jdk)
    if not exists(jvmCfg):
        mx.abort(jvmCfg + ' does not exist')

    prefix = '-' + vm + ' '
    vmKnown = prefix + 'KNOWN\n'
    lines = []
    found = False
    with open(jvmCfg) as f:
        for line in f:
            if line.strip() == vmKnown.strip():
                found = True
            lines.append(line)

    if not found:
        mx.log('Prepending "' + prefix + 'KNOWN" to ' + jvmCfg)
        if mx.get_os() != 'windows':
            os.chmod(jvmCfg, JDK_UNIX_PERMISSIONS_FILE)
        with open(jvmCfg, 'w') as f:
            written = False
            for line in lines:
                if line.startswith('#'):
                    f.write(line)
                    continue
                if not written:
                    f.write(vmKnown)
                    if vm == 'jvmci':
                        # Legacy support
                        f.write('-graal ALIASED_TO -jvmci\n')
                    written = True
                if line.startswith(prefix):
                    line = vmKnown
                    if written:
                        continue
                f.write(line)

mx_gate.add_jacoco_includes(['jdk.internal.jvmci.*'])

def parseVmArgs(args, vm=None, cwd=None, vmbuild=None):
    """run the VM selected by the '--vm' option"""

    if vm is None:
        vm = get_vm()

    if not isVMSupported(vm):
        mx.abort('The ' + vm + ' is not supported on this platform')

    if cwd is None:
        cwd = _vm_cwd
    elif _vm_cwd is not None and _vm_cwd != cwd:
        mx.abort("conflicting working directories: do not set --vmcwd for this command")

    build = vmbuild if vmbuild else _vmbuild
    jdk = get_jvmci_jdk(build, vmToCheck=vm, installJars=False)
    _updateInstalledJVMCIOptionsFile(jdk)
    mx.expand_project_in_args(args)
    if _make_eclipse_launch:
        mx.make_eclipse_launch(_suite, args, _suite.name + '-' + build, name=None, deps=mx.dependencies())
    jacocoArgs = mx_gate.get_jacoco_agent_args()
    if jacocoArgs:
        args = jacocoArgs + args
    exe = join(jdk, 'bin', mx.exe_suffix('java'))
    pfx = _vm_prefix.split() if _vm_prefix is not None else []

    if '-version' in args:
        ignoredArgs = args[args.index('-version') + 1:]
        if  len(ignoredArgs) > 0:
            mx.log("Warning: The following options will be ignored by the vm because they come after the '-version' argument: " + ' '.join(ignoredArgs))

    args = mx.get_jdk().processArgs(args)
    return (pfx, exe, vm, args, cwd)

def vm(args, vm=None, nonZeroIsFatal=True, out=None, err=None, cwd=None, timeout=None, vmbuild=None):
    (pfx_, exe_, vm_, args_, cwd) = parseVmArgs(args, vm, cwd, vmbuild)
    return mx.run(pfx_ + [exe_, '-' + vm_] + args_, nonZeroIsFatal=nonZeroIsFatal, out=out, err=err, cwd=cwd, timeout=timeout)

def _unittest_config_participant(config):
    vmArgs, mainClass, mainClassArgs = config
    if isJVMCIEnabled(get_vm()):
        # Remove entries from class path that are in JVMCI loaded jars
        cpIndex, cp = mx.find_classpath_arg(vmArgs)
        if cp:
            excluded = set()
            for jdkDist in jdkDeployedDists:
                dist = jdkDist.dist()
                excluded.update([d.output_dir() for d in dist.archived_deps() if d.isJavaProject()])
            cp = os.pathsep.join([e for e in cp.split(os.pathsep) if e not in excluded])
            vmArgs[cpIndex] = cp

        # Run the VM in a mode where application/test classes can
        # access JVMCI loaded classes.
        vmArgs = ['-XX:-UseJVMCIClassLoader'] + vmArgs
        return (vmArgs, mainClass, mainClassArgs)
    return config

def _unittest_vm_launcher(vmArgs, mainClass, mainClassArgs):
    vm(vmArgs + [mainClass] + mainClassArgs)

mx_unittest.add_config_participant(_unittest_config_participant)
mx_unittest.set_vm_launcher('JVMCI VM launcher', _unittest_vm_launcher)

def shortunittest(args):
    """alias for 'unittest --whitelist test/whitelist_shortunittest.txt'"""

    args = ['--whitelist', 'test/whitelist_shortunittest.txt'] + args
    mx_unittest.unittest(args)

def buildvms(args):
    """build one or more VMs in various configurations"""

    vmsDefault = ','.join(_vmChoices.keys())
    vmbuildsDefault = ','.join(_vmbuildChoices)

    parser = ArgumentParser(prog='mx buildvms')
    parser.add_argument('--vms', help='a comma separated list of VMs to build (default: ' + vmsDefault + ')', metavar='<args>', default=vmsDefault)
    parser.add_argument('--builds', help='a comma separated list of build types (default: ' + vmbuildsDefault + ')', metavar='<args>', default=vmbuildsDefault)
    parser.add_argument('-n', '--no-check', action='store_true', help='omit running "java -version" after each build')
    parser.add_argument('-c', '--console', action='store_true', help='send build output to console instead of log file')

    args = parser.parse_args(args)
    vms = args.vms.split(',')
    builds = args.builds.split(',')

    allStart = time.time()
    for v in vms:
        if not isVMSupported(v):
            mx.log('The ' + v + ' VM is not supported on this platform - skipping')
            continue

        for vmbuild in builds:
            if v == 'original' and vmbuild != 'product':
                continue
            if not args.console:
                logFile = join(v + '-' + vmbuild + '.log')
                log = open(join(_suite.dir, logFile), 'wb')
                start = time.time()
                mx.log('BEGIN: ' + v + '-' + vmbuild + '\t(see: ' + logFile + ')')
                verbose = ['-v'] if mx._opts.verbose else []
                # Run as subprocess so that output can be directed to a file
                cmd = [sys.executable, '-u', mx.__file__] + verbose + ['--vm', v, '--vmbuild', vmbuild, 'build']
                mx.logv("executing command: " + str(cmd))
                subprocess.check_call(cmd, cwd=_suite.dir, stdout=log, stderr=subprocess.STDOUT)
                duration = datetime.timedelta(seconds=time.time() - start)
                mx.log('END:   ' + v + '-' + vmbuild + '\t[' + str(duration) + ']')
            else:
                with VM(v, vmbuild):
                    build([])
            if not args.no_check:
                vmargs = ['-version']
                if v == 'jvmci':
                    vmargs.insert(0, '-XX:-BootstrapJVMCI')
                vm(vmargs, vm=v, vmbuild=vmbuild)
    allDuration = datetime.timedelta(seconds=time.time() - allStart)
    mx.log('TOTAL TIME:   ' + '[' + str(allDuration) + ']')


def _jvmci_gate_runner(args, tasks):
    with Task('Check jvmci.make in sync with suite.py', tasks) as t:
        if t:
            jvmciMake = join(_suite.dir, 'make', 'jvmci.make')
            if mx_jvmci_makefile.build_makefile(['-o', jvmciMake]) != 0:
                t.abort('Rerun "mx makefile -o ' + jvmciMake + ' and check-in the modified ' + jvmciMake)

    # Build server-hosted-jvmci now so we can run the unit tests
    with Task('BuildHotSpotJVMCIHosted: product', tasks) as t:
        if t: buildvms(['--vms', 'server', '--builds', 'product'])

    # Run unit tests on server-hosted-jvmci
    with VM('server', 'product'):
        with Task('JVMCI UnitTests: hosted-product', tasks) as t:
            if t: unittest(['--suite', 'jvmci', '--enable-timing', '--verbose', '--fail-fast'])

    # Build the other VM flavors
    with Task('BuildHotSpotJVMCIOthers: fastdebug,product', tasks) as t:
        if t: buildvms(['--vms', 'jvmci,server', '--builds', 'fastdebug,product'])

    with Task('CleanAndBuildIdealGraphVisualizer', tasks, disableJacoco=True) as t:
        if t and platform.processor() != 'sparc':
            buildxml = mx._cygpathU2W(join(_suite.dir, 'src', 'share', 'tools', 'IdealGraphVisualizer', 'build.xml'))
            mx.run(['ant', '-f', buildxml, '-q', 'clean', 'build'], env=_igvBuildEnv())

    # Prevent JVMCI modifications from breaking the standard builds
    if args.buildNonJVMCI:
        with Task('BuildHotSpotVarieties', tasks, disableJacoco=True) as t:
            if t:
                buildvms(['--vms', 'client,server', '--builds', 'fastdebug,product'])
                if mx.get_os() not in ['windows', 'cygwin']:
                    buildvms(['--vms', 'server-nojvmci', '--builds', 'product,optimized'])

mx_gate.add_gate_runner(_suite, _jvmci_gate_runner)
mx_gate.add_gate_argument('-g', '--only-build-jvmci', action='store_false', dest='buildNonJVMCI', help='only build the JVMCI VM')

def deoptalot(args):
    """bootstrap a VM with DeoptimizeALot and VerifyOops on

    If the first argument is a number, the process will be repeated
    this number of times. All other arguments are passed to the VM."""
    count = 1
    if len(args) > 0 and args[0].isdigit():
        count = int(args[0])
        del args[0]

    for _ in range(count):
        if not vm(['-XX:-TieredCompilation', '-XX:+DeoptimizeALot', '-XX:+VerifyOops'] + args + ['-version']) == 0:
            mx.abort("Failed")

def longtests(args):

    deoptalot(['15', '-Xmx48m'])

def _igvJdk():
    v8u20 = mx.VersionSpec("1.8.0_20")
    v8u40 = mx.VersionSpec("1.8.0_40")
    v8 = mx.VersionSpec("1.8")
    def _igvJdkVersionCheck(version):
        return version >= v8 and (version < v8u20 or version >= v8u40)
    return mx.get_jdk(_igvJdkVersionCheck, versionDescription='>= 1.8 and < 1.8.0u20 or >= 1.8.0u40', purpose="building & running IGV").home

def _igvBuildEnv():
        # When the http_proxy environment variable is set, convert it to the proxy settings that ant needs
    env = dict(os.environ)
    proxy = os.environ.get('http_proxy')
    if not (proxy is None) and len(proxy) > 0:
        if '://' in proxy:
            # Remove the http:// prefix (or any other protocol prefix)
            proxy = proxy.split('://', 1)[1]
        # Separate proxy server name and port number
        proxyName, proxyPort = proxy.split(':', 1)
        proxyEnv = '-DproxyHost="' + proxyName + '" -DproxyPort=' + proxyPort
        env['ANT_OPTS'] = proxyEnv

    env['JAVA_HOME'] = _igvJdk()
    return env

def igv(args):
    """run the Ideal Graph Visualizer"""
    logFile = '.ideal_graph_visualizer.log'
    with open(join(_suite.dir, logFile), 'w') as fp:
        mx.logv('[Ideal Graph Visualizer log is in ' + fp.name + ']')
        nbplatform = join(_suite.dir, 'src', 'share', 'tools', 'IdealGraphVisualizer', 'nbplatform')

        # Remove NetBeans platform if it is earlier than the current supported version
        if exists(nbplatform):
            updateTrackingFile = join(nbplatform, 'platform', 'update_tracking', 'org-netbeans-core.xml')
            if not exists(updateTrackingFile):
                mx.log('Could not find \'' + updateTrackingFile + '\', removing NetBeans platform')
                shutil.rmtree(nbplatform)
            else:
                dom = xml.dom.minidom.parse(updateTrackingFile)
                currentVersion = mx.VersionSpec(dom.getElementsByTagName('module_version')[0].getAttribute('specification_version'))
                supportedVersion = mx.VersionSpec('3.43.1')
                if currentVersion < supportedVersion:
                    mx.log('Replacing NetBeans platform version ' + str(currentVersion) + ' with version ' + str(supportedVersion))
                    shutil.rmtree(nbplatform)
                elif supportedVersion < currentVersion:
                    mx.log('Supported NetBeans version in igv command should be updated to ' + str(currentVersion))

        if not exists(nbplatform):
            mx.logv('[This execution may take a while as the NetBeans platform needs to be downloaded]')

        env = _igvBuildEnv()
        # make the jar for Batik 1.7 available.
        env['IGV_BATIK_JAR'] = mx.library('BATIK').get_path(True)
        if mx.run(['ant', '-f', mx._cygpathU2W(join(_suite.dir, 'src', 'share', 'tools', 'IdealGraphVisualizer', 'build.xml')), '-l', mx._cygpathU2W(fp.name), 'run'], env=env, nonZeroIsFatal=False):
            mx.abort("IGV ant build & launch failed. Check '" + logFile + "'. You can also try to delete 'src/share/tools/IdealGraphVisualizer/nbplatform'.")

def c1visualizer(args):
    """run the Cl Compiler Visualizer"""
    libpath = join(_suite.dir, 'lib')
    if mx.get_os() == 'windows':
        executable = join(libpath, 'c1visualizer', 'bin', 'c1visualizer.exe')
    else:
        executable = join(libpath, 'c1visualizer', 'bin', 'c1visualizer')

    # Check whether the current C1Visualizer installation is the up-to-date
    if exists(executable) and not exists(mx.library('C1VISUALIZER_DIST').get_path(resolve=False)):
        mx.log('Updating C1Visualizer')
        shutil.rmtree(join(libpath, 'c1visualizer'))

    archive = mx.library('C1VISUALIZER_DIST').get_path(resolve=True)

    if not exists(executable):
        zf = zipfile.ZipFile(archive, 'r')
        zf.extractall(libpath)

    if not exists(executable):
        mx.abort('C1Visualizer binary does not exist: ' + executable)

    if mx.get_os() != 'windows':
        # Make sure that execution is allowed. The zip file does not always specfiy that correctly
        os.chmod(executable, 0777)

    mx.run([executable])

def _get_jmh_path():
    path = mx.get_env('JMH_BENCHMARKS', None)
    if not path:
        probe = join(dirname(_suite.dir), 'java-benchmarks')
        if exists(probe):
            path = probe

    if not path:
        mx.abort("Please set the JMH_BENCHMARKS environment variable to point to the java-benchmarks workspace")
    if not exists(path):
        mx.abort("The directory denoted by the JMH_BENCHMARKS environment variable does not exist: " + path)
    return path

def makejmhdeps(args):
    """creates and installs Maven dependencies required by the JMH benchmarks

    The dependencies are specified by files named pom.mxdeps in the
    JMH directory tree. Each such file contains a list of dependencies
    defined in JSON format. For example:

    '[{"artifactId" : "compiler.test", "groupId" : "com.oracle.graal", "deps" : ["com.oracle.graal.compiler.test"]}]'

    will result in a dependency being installed in the local Maven repository
    that can be referenced in a pom.xml file as follows:

          <dependency>
            <groupId>com.oracle.graal</groupId>
            <artifactId>compiler.test</artifactId>
            <version>1.0-SNAPSHOT</version>
          </dependency>"""

    parser = ArgumentParser(prog='mx makejmhdeps')
    parser.add_argument('-s', '--settings', help='alternative path for Maven user settings file', metavar='<path>')
    parser.add_argument('-p', '--permissive', action='store_true', help='issue note instead of error if a Maven dependency cannot be built due to missing projects/libraries')
    args = parser.parse_args(args)

    def makejmhdep(artifactId, groupId, deps):
        path = artifactId + '.jar'
        allDeps = []
        if args.permissive:
            for name, dep in [(d, mx.dependency(d, fatalIfMissing=False)) for d in deps]:
                if dep is None:
                    mx.log('Skipping dependency ' + groupId + '.' + artifactId + ' as ' + name + ' cannot be resolved')
                    return
                if dep.isDistribution():
                    allDeps = allDeps + [d.name for d in dep.archived_deps() if d.isJavaProject()]
                else:
                    allDeps.append(name)
        d = mx.Distribution(_suite, name=artifactId, subDir=_suite.dir, path=path, sourcesPath=path, deps=allDeps, mainClass=None, excludedLibs=[], distDependencies=[], javaCompliance=None)
        d.make_archive()
        env = os.environ.copy()
        env['JAVA_HOME'] = get_jvmci_jdk(vmToCheck='server')
        env['MAVEN_OPTS'] = '-server -XX:-UseJVMCIClassLoader'
        cmd = ['mvn', 'install:install-file', '-DgroupId=' + groupId, '-DartifactId=' + artifactId,
               '-Dversion=1.0-SNAPSHOT', '-Dpackaging=jar', '-Dfile=' + d.path]
        if not mx._opts.verbose:
            cmd.append('-q')
        if args.settings:
            cmd = cmd + ['-s', args.settings]
        mx.run(cmd, env=env)
        os.unlink(d.path)

    jmhPath = _get_jmh_path()
    for root, _, filenames in os.walk(jmhPath):
        for f in [join(root, n) for n in filenames if n == 'pom.mxdeps']:
            mx.logv('[processing ' + f + ']')
            try:
                with open(f) as fp:
                    for d in json.load(fp):
                        artifactId = d['artifactId']
                        groupId = d['groupId']
                        deps = d['deps']
                        makejmhdep(artifactId, groupId, deps)
            except ValueError as e:
                mx.abort('Error parsing {0}:\n{1}'.format(f, e))

def buildjmh(args):
    """build the JMH benchmarks"""

    parser = ArgumentParser(prog='mx buildjmh')
    parser.add_argument('-s', '--settings', help='alternative path for Maven user settings file', metavar='<path>')
    parser.add_argument('-c', action='store_true', dest='clean', help='clean before building')
    args = parser.parse_args(args)

    jmhPath = _get_jmh_path()
    mx.log('JMH benchmarks: ' + jmhPath)

    # Ensure the mx injected dependencies are up to date
    makejmhdeps(['-p'] + (['-s', args.settings] if args.settings else []))

    timestamp = mx.TimeStampFile(join(_suite.mxDir, 'jmh', jmhPath.replace(os.sep, '_') + '.timestamp'))
    mustBuild = args.clean
    if not mustBuild:
        try:
            hgfiles = [join(jmhPath, f) for f in subprocess.check_output(['hg', '-R', jmhPath, 'locate']).split('\n')]
            mustBuild = timestamp.isOlderThan(hgfiles)
        except:
            # not a Mercurial repository or hg commands are not available.
            mustBuild = True

    if mustBuild:
        buildOutput = []
        def _redirect(x):
            if mx._opts.verbose:
                mx.log(x[:-1])
            else:
                buildOutput.append(x)
        env = os.environ.copy()
        env['JAVA_HOME'] = get_jvmci_jdk(vmToCheck='server')
        env['MAVEN_OPTS'] = '-server -XX:-UseJVMCIClassLoader'
        mx.log("Building benchmarks...")
        cmd = ['mvn']
        if args.settings:
            cmd = cmd + ['-s', args.settings]
        if args.clean:
            cmd.append('clean')
        cmd.append('package')
        retcode = mx.run(cmd, cwd=jmhPath, out=_redirect, env=env, nonZeroIsFatal=False)
        if retcode != 0:
            mx.log(''.join(buildOutput))
            mx.abort(retcode)
        timestamp.touch()
    else:
        mx.logv('[all Mercurial controlled files in ' + jmhPath + ' are older than ' + timestamp.path + ' - skipping build]')

def jmh(args):
    """run the JMH benchmarks

    This command respects the standard --vm and --vmbuild options
    for choosing which VM to run the benchmarks with."""
    if '-h' in args:
        mx.help_(['jmh'])
        mx.abort(1)

    vmArgs, benchmarksAndJsons = mx.extract_VM_args(args)
    if isJVMCIEnabled(get_vm()) and  '-XX:-UseJVMCIClassLoader' not in vmArgs:
        vmArgs = ['-XX:-UseJVMCIClassLoader'] + vmArgs

    benchmarks = [b for b in benchmarksAndJsons if not b.startswith('{')]
    jmhArgJsons = [b for b in benchmarksAndJsons if b.startswith('{')]
    jmhOutDir = join(_suite.mxDir, 'jmh')
    if not exists(jmhOutDir):
        os.makedirs(jmhOutDir)
    jmhOut = join(jmhOutDir, 'jmh.out')
    jmhArgs = {'-rff' : jmhOut, '-v' : 'EXTRA' if mx._opts.verbose else 'NORMAL'}

    # e.g. '{"-wi" : 20}'
    for j in jmhArgJsons:
        try:
            for n, v in json.loads(j).iteritems():
                if v is None:
                    del jmhArgs[n]
                else:
                    jmhArgs[n] = v
        except ValueError as e:
            mx.abort('error parsing JSON input: {0}\n{1}'.format(j, e))

    jmhPath = _get_jmh_path()
    mx.log('Using benchmarks in ' + jmhPath)

    matchedSuites = set()
    numBench = [0]
    for micros in os.listdir(jmhPath):
        absoluteMicro = os.path.join(jmhPath, micros)
        if not os.path.isdir(absoluteMicro):
            continue
        if not micros.startswith("micros-"):
            mx.logv('JMH: ignored ' + absoluteMicro + " because it doesn't start with 'micros-'")
            continue

        microJar = os.path.join(absoluteMicro, "target", "microbenchmarks.jar")
        if not exists(microJar):
            mx.log('Missing ' + microJar + ' - please run "mx buildjmh"')
            continue
        if benchmarks:
            def _addBenchmark(x):
                if x.startswith("Benchmark:"):
                    return
                match = False
                for b in benchmarks:
                    match = match or (b in x)

                if match:
                    numBench[0] += 1
                    matchedSuites.add(micros)

            mx.run_java(['-jar', microJar, "-l"], cwd=jmhPath, out=_addBenchmark, addDefaultArgs=False)
        else:
            matchedSuites.add(micros)

    mx.logv("matchedSuites: " + str(matchedSuites))
    plural = 's' if not benchmarks or numBench[0] > 1 else ''
    number = str(numBench[0]) if benchmarks else "all"
    mx.log("Running " + number + " benchmark" + plural + '...')

    regex = []
    if benchmarks:
        regex.append(r".*(" + "|".join(benchmarks) + ").*")

    for suite in matchedSuites:
        absoluteMicro = os.path.join(jmhPath, suite)
        (pfx, exe, vm, forkedVmArgs, _) = parseVmArgs(vmArgs)
        if pfx:
            mx.log("JMH ignores prefix: \"" + ' '.join(pfx) + "\"")
        javaArgs = ['-jar', os.path.join(absoluteMicro, "target", "microbenchmarks.jar"),
                    '--jvm', exe,
                    '--jvmArgs', ' '.join(["-" + vm] + forkedVmArgs)]
        for k, v in jmhArgs.iteritems():
            javaArgs.append(k)
            if len(str(v)):
                javaArgs.append(str(v))
        mx.run_java(javaArgs + regex, addDefaultArgs=False, cwd=jmhPath)

def microbench(args):
    """run JMH microbenchmark projects"""
    vmArgs, jmhArgs = mx.extract_VM_args(args, useDoubleDash=True)

    # look for -f in JMH arguments
    containsF = False
    forking = True
    for i in range(len(jmhArgs)):
        arg = jmhArgs[i]
        if arg.startswith('-f'):
            containsF = True
            if arg == '-f' and (i+1) < len(jmhArgs):
                arg += jmhArgs[i+1]
            try:
                if int(arg[2:]) == 0:
                    forking = False
            except ValueError:
                pass

    # default to -f1 if not specified otherwise
    if not containsF:
        jmhArgs += ['-f1']

    # find all projects with a direct JMH dependency
    jmhProjects = []
    for p in mx.projects():
        if 'JMH' in p.deps:
            jmhProjects.append(p.name)
    cp = mx.classpath(jmhProjects)

    # execute JMH runner
    args = ['-cp', cp]
    if not forking:
        args += vmArgs
    args += ['org.openjdk.jmh.Main']
    if forking:
        (_, _, jvm, _, _) = parseVmArgs(vmArgs)
        args += ['--jvmArgsPrepend', ' '.join(['-' + jvm] + vmArgs)]
    vm(args + jmhArgs)

def hsdis(args, copyToDir=None):
    """download the hsdis library

    This is needed to support HotSpot's assembly dumping features.
    By default it downloads the Intel syntax version, use the 'att' argument to install AT&T syntax."""
    flavor = 'intel'
    if 'att' in args:
        flavor = 'att'
    if mx.get_arch() == "sparcv9":
        flavor = "sparcv9"
    lib = mx.add_lib_suffix('hsdis-' + mx.get_arch())
    path = join(_suite.dir, 'lib', lib)

    sha1s = {
        'att/hsdis-amd64.dll' : 'bcbd535a9568b5075ab41e96205e26a2bac64f72',
        'att/hsdis-amd64.so' : '58919ba085d4ef7a513f25bae75e7e54ee73c049',
        'intel/hsdis-amd64.dll' : '6a388372cdd5fe905c1a26ced614334e405d1f30',
        'intel/hsdis-amd64.so' : '844ed9ffed64fe9599638f29a8450c50140e3192',
        'intel/hsdis-amd64.dylib' : 'fdb13ef0d7d23d93dacaae9c98837bea0d4fc5a2',
        'sparcv9/hsdis-sparcv9.so': '970640a9af0bd63641f9063c11275b371a59ee60',
    }

    flavoredLib = flavor + "/" + lib
    if flavoredLib not in sha1s:
        mx.logv("hsdis not supported on this plattform or architecture")
        return

    if not exists(path):
        sha1 = sha1s[flavoredLib]
        sha1path = path + '.sha1'
        mx.download_file_with_sha1('hsdis', path, ['http://lafo.ssw.uni-linz.ac.at/hsdis/' + flavoredLib], sha1, sha1path, True, True, sources=False)
    if copyToDir is not None and exists(copyToDir):
        shutil.copy(path, copyToDir)

def hcfdis(args):
    """disassemble HexCodeFiles embedded in text files

    Run a tool over the input files to convert all embedded HexCodeFiles
    to a disassembled format."""

    parser = ArgumentParser(prog='mx hcfdis')
    parser.add_argument('-m', '--map', help='address to symbol map applied to disassembler output')
    parser.add_argument('files', nargs=REMAINDER, metavar='files...')

    args = parser.parse_args(args)

    path = mx.library('HCFDIS').get_path(resolve=True)
    mx.run_java(['-cp', path, 'com.oracle.max.hcfdis.HexCodeFileDis'] + args.files)

    if args.map is not None:
        addressRE = re.compile(r'0[xX]([A-Fa-f0-9]+)')
        with open(args.map) as fp:
            lines = fp.read().splitlines()
        symbols = dict()
        for l in lines:
            addressAndSymbol = l.split(' ', 1)
            if len(addressAndSymbol) == 2:
                address, symbol = addressAndSymbol
                if address.startswith('0x'):
                    address = long(address, 16)
                    symbols[address] = symbol
        for f in args.files:
            with open(f) as fp:
                lines = fp.read().splitlines()
            updated = False
            for i in range(0, len(lines)):
                l = lines[i]
                for m in addressRE.finditer(l):
                    sval = m.group(0)
                    val = long(sval, 16)
                    sym = symbols.get(val)
                    if sym:
                        l = l.replace(sval, sym)
                        updated = True
                        lines[i] = l
            if updated:
                mx.log('updating ' + f)
                with open('new_' + f, "w") as fp:
                    for l in lines:
                        print >> fp, l

def isJVMCIEnabled(vm):
    return vm != 'original' and not vm.endswith('nojvmci')

def jol(args):
    """Java Object Layout"""
    joljar = mx.library('JOL_INTERNALS').get_path(resolve=True)
    candidates = mx.findclass(args, logToConsole=False, matcher=lambda s, classname: s == classname or classname.endswith('.' + s) or classname.endswith('$' + s))

    if len(candidates) > 0:
        candidates = mx.select_items(sorted(candidates))
    else:
        # mx.findclass can be mistaken, don't give up yet
        candidates = args

    vm(['-javaagent:' + joljar, '-cp', os.pathsep.join([mx.classpath(), joljar]), "org.openjdk.jol.MainObjectInternals"] + candidates)

mx.update_commands(_suite, {
    'build': [build, ''],
    'buildjmh': [buildjmh, '[-options]'],
    'buildvars': [buildvars, ''],
    'buildvms': [buildvms, '[-options]'],
    'c1visualizer' : [c1visualizer, ''],
    'export': [export, '[-options] [zipfile]'],
    'hsdis': [hsdis, '[att]'],
    'hcfdis': [hcfdis, ''],
    'igv' : [igv, ''],
    'jdkhome': [print_jdkhome, ''],
    'jmh': [jmh, '[VM options] [filters|JMH-args-as-json...]'],
    'makejmhdeps' : [makejmhdeps, ''],
    'shortunittest' : [shortunittest, '[unittest options] [--] [VM options] [filters...]', mx_unittest.unittestHelpSuffix],
    'vm': [vm, '[-options] class [args...]'],
    'deoptalot' : [deoptalot, '[n]'],
    'longtests' : [longtests, ''],
    'jol' : [jol, ''],
    'makefile' : [mx_jvmci_makefile.build_makefile, 'build makefiles for JDK build'],
})

mx.add_argument('--vmcwd', dest='vm_cwd', help='current directory will be changed to <path> before the VM is executed', default=None, metavar='<path>')
mx.add_argument('--installed-jdks', help='the base directory in which the JDKs cloned from $JAVA_HOME exist. ' +
                'The VM selected by --vm and --vmbuild options is under this directory (i.e., ' +
                join('<path>', '<jdk-version>', '<vmbuild>', 'jre', 'lib', '<vm>', mx.add_lib_prefix(mx.add_lib_suffix('jvm'))) + ')', default=None, metavar='<path>')

mx.add_argument('--vm', action='store', dest='vm', choices=_vmChoices.keys(), help='the VM type to build/run')
mx.add_argument('--vmbuild', action='store', dest='vmbuild', choices=_vmbuildChoices, help='the VM build to build/run (default: ' + _vmbuildChoices[0] + ')')
mx.add_argument('--ecl', action='store_true', dest='make_eclipse_launch', help='create launch configuration for running VM execution(s) in Eclipse')
mx.add_argument('--vmprefix', action='store', dest='vm_prefix', help='prefix for running the VM (e.g. "/usr/bin/gdb --args")', metavar='<prefix>')
mx.add_argument('--gdb', action='store_const', const='/usr/bin/gdb --args', dest='vm_prefix', help='alias for --vmprefix "/usr/bin/gdb --args"')
mx.add_argument('--lldb', action='store_const', const='lldb --', dest='vm_prefix', help='alias for --vmprefix "lldb --"')

class JVMCIArchiveParticipant:
    def __init__(self, dist):
        self.dist = dist
        self.jvmciServices = {}

    def __opened__(self, arc, srcArc, services):
        self.services = services
        self.arc = arc
        self.expectedOptionsProviders = set()

    def __add__(self, arcname, contents):
        if arcname.startswith('META-INF/jvmci.services/'):
            service = arcname[len('META-INF/jvmci.services/'):]
            self.jvmciServices.setdefault(service, []).extend([provider for provider in contents.split('\n')])
            return True
        if arcname.startswith('META-INF/jvmci.providers/'):
            provider = arcname[len('META-INF/jvmci.providers/'):]
            for service in contents.split(os.linesep):
                self.jvmciServices.setdefault(service, []).append(provider)
            return True
        elif arcname.startswith('META-INF/jvmci.options/'):
            # Need to create service files for the providers of the
            # jdk.internal.jvmci.options.Options service created by
            # jdk.internal.jvmci.options.processor.OptionProcessor.
            optionsOwner = arcname[len('META-INF/jvmci.options/'):]
            provider = optionsOwner + '_Options'
            self.expectedOptionsProviders.add(provider.replace('.', '/') + '.class')
            self.services.setdefault('jdk.internal.jvmci.options.Options', []).append(provider)
        return False

    def __addsrc__(self, arcname, contents):
        return False

    def __closing__(self):
        self.expectedOptionsProviders -= set(self.arc.zf.namelist())
        assert len(self.expectedOptionsProviders) == 0, 'missing generated Options providers:\n  ' + '\n  '.join(self.expectedOptionsProviders)
        for service, providers in self.jvmciServices.iteritems():
            arcname = 'META-INF/jvmci.services/' + service
            # Convert providers to a set before printing to remove duplicates
            self.arc.zf.writestr(arcname, '\n'.join(frozenset(providers))+ '\n')

def mx_post_parse_cmd_line(opts):
    # TODO _minVersion check could probably be part of a Suite in mx?
    def _versionCheck(version):
        return version >= _minVersion and (not _untilVersion or version >= _untilVersion)
    versionDesc = ">=" + str(_minVersion)
    if _untilVersion:
        versionDesc += " and <=" + str(_untilVersion)
    mx.get_jdk(_versionCheck, versionDescription=versionDesc, defaultJdk=True)

    if hasattr(opts, 'vm') and opts.vm is not None:
        global _vm
        _vm = opts.vm
        _vm = _vm.replace('graal', 'jvmci')
    if hasattr(opts, 'vmbuild') and opts.vmbuild is not None:
        global _vmbuild
        _vmbuild = opts.vmbuild
    global _make_eclipse_launch
    _make_eclipse_launch = getattr(opts, 'make_eclipse_launch', False)
    global _vm_cwd
    _vm_cwd = opts.vm_cwd
    global _installed_jdks
    _installed_jdks = opts.installed_jdks
    global _vm_prefix
    _vm_prefix = opts.vm_prefix

    mx.instantiateDistribution('JVM_<vmbuild>_<vm>', dict(vmbuild=_vmbuild, vm=get_vm()))

    for jdkDist in jdkDeployedDists:
        def _close(jdkDeployable):
            def _install(dist):
                _installDistInJdks(jdkDeployable)
            return _install
        dist = jdkDist.dist()
        dist.add_update_listener(_close(jdkDist))
        if isinstance(jdkDist, JvmciJDKDeployedDist):
            dist.set_archiveparticipant(JVMCIArchiveParticipant(dist))
