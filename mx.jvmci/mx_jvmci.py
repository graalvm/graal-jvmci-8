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

from __future__ import print_function

import os, stat, errno, sys, shutil, zipfile, tarfile, tempfile, re, time, datetime, platform, subprocess, socket
from os.path import join, exists, basename
from argparse import ArgumentParser, REMAINDER
import xml.dom.minidom
import json, textwrap
import difflib
from collections import OrderedDict

import mx
import mx_unittest
from mx_unittest import unittest
from mx_gate import Task
from mx_sigtest import sigtest
import mx_gate

assert sys.version_info[0] >= 3, 'Requires Python 3: ' + str(sys.version_info)

_suite = mx.suite('jvmci')

""" The VMs that can be built and run along with an optional description. Only VMs with a
    description are listed in the dialogue for setting the default VM (see get_vm()). """
_vmChoices = {
    'server' : 'Normal compilation is performed with a tiered system (C1 + C2 or Graal) and Graal is available for hosted compilation.',
    'client' : None,  # VM compilation with client compiler, hosted compilation with Graal
}

_jvmciModes = {
    'hosted' : ['-XX:+UnlockExperimentalVMOptions', '-XX:+EnableJVMCI', '-XX:-UseJVMCICompiler'],
    'jit' : ['-XX:+UnlockExperimentalVMOptions', '-XX:+EnableJVMCI', '-XX:+UseJVMCICompiler'],
    'disabled' : ['-XX:+UnlockExperimentalVMOptions', '-XX:-EnableJVMCI']
}

# Aliases for legacy VM names
_vmAliases = {
    'jvmci' : 'server',
    'graal' : 'server',
}

_JVMCI_JDK_TAG = 'jvmci'

""" The VM that will be run by the 'vm' command and built by default by the 'build' command.
    It can be temporarily set by using a VM context manager object in a 'with' statement. """
_vm = 'server'

class JVMCIMode:
    def __init__(self, jvmciMode):
        self.jvmciMode = jvmciMode
        self.vmArgs = _jvmciModes[jvmciMode]

    def __enter__(self):
        global _jvmciMode
        self.previousMode = _jvmciMode # pylint: disable=used-before-assignment
        _jvmciMode = self

    def __exit__(self, exc_type, exc_value, traceback):
        global _jvmciMode
        _jvmciMode = self.previousMode

""" The JVMCI mode that will be used by the 'vm' command. This can be set with the global '-M' option.
    It can also be temporarily set using the JVMCIMode context manager object in a 'with' statement.
    Defaults to 'hosted'. """
_jvmciMode = JVMCIMode('hosted')

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

_minVersion = mx.VersionSpec('1.8.0_141')

# max version (first _unsupported_ version)
_untilVersion = mx.VersionSpec('1.9')

class JDKDeployedDist(object):
    def __init__(self, name):
        self._name = name

    def dist(self):
        return mx.distribution(self._name)

    def deploy(self, jdkDir):
        mx.nyi('deploy', self)

    def post_parse_cmd_line(self):
        def _install(d):
            _installDistInJdks(self)
        self.dist().add_update_listener(_install)

class JarJDKDeployedDist(JDKDeployedDist):
    def __init__(self, name):
        JDKDeployedDist.__init__(self, name)

    def targetDir(self):
        mx.nyi('targetDir', self)

    def _copyToJdk(self, jdkDir, target):
        targetDir = join(jdkDir, target)
        dist = self.dist()
        mx.logv('Deploying {} to {}'.format(dist.name, targetDir))
        copyToJdk(dist.path, targetDir)
        if exists(dist.sourcesPath):
            copyToJdk(dist.sourcesPath, targetDir)

    def deploy(self, jdkDir):
        self._copyToJdk(jdkDir, self.targetDir())

class LibJDKDeployedDist(JarJDKDeployedDist):
    def __init__(self, name):
        JarJDKDeployedDist.__init__(self, name)

    def targetDir(self):
        return join('jre', 'lib')

class JvmciJDKDeployedDist(JarJDKDeployedDist):
    def __init__(self, name):
        JarJDKDeployedDist.__init__(self, name)

    def targetDir(self):
        return join('jre', 'lib', 'jvmci')

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
        vmbuild = _vmbuildFromJdkDir(jdkDir)
        if vmbuild != _vmbuild:
            return
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

"""
List of distributions that are deployed into a JDK by mx.
"""
jdkDeployedDists = [
    LibJDKDeployedDist('JVMCI_SERVICES'),
    JvmciJDKDeployedDist('JVMCI_API'),
    JvmciJDKDeployedDist('JVMCI_HOTSPOT'),
    HotSpotVMJDKDeployedDist('JVM_<vmbuild>_<vm>'),
]

JDK_UNIX_PERMISSIONS_DIR = 0o755
JDK_UNIX_PERMISSIONS_FILE = 0o644
JDK_UNIX_PERMISSIONS_EXEC = 0o755

def isVMSupported(vm):
    if vm == 'client' and len(platform.mac_ver()[0]) != 0:
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

def get_vm_prefix(asList=True):
    """
    Get the prefix for running the VM ("/usr/bin/gdb --args").
    """
    if asList:
        return _vm_prefix.split() if _vm_prefix is not None else []
    return _vm_prefix

def get_vm_choices():
    """
    Get the names of available VMs.
    """
    return _vmChoices.viewkeys()

def dealiased_vm(vm):
    """
    If 'vm' is an alias, returns the aliased name otherwise returns 'vm'.
    """
    if vm and vm in _vmAliases:
        if vm in ['jvmci', 'graal']:
            mx.log('"--vm ' + vm + '" is deprecated, using "--vm server -Mjit" instead')
            global _jvmciMode
            _jvmciMode = JVMCIMode('jit')
        return _vmAliases[vm]
    return vm

def get_jvmci_mode():
    return _jvmciMode.jvmciMode

def get_jvmci_mode_args():
    return _jvmciMode.vmArgs

def get_vm():
    """
    Gets the configured VM.
    """
    return _vm

"""
A context manager that can be used with the 'with' statement to set the VM
used by all VM executions within the scope of the 'with' statement. For example:

    with VM('server'):
        dacapo(['pmd'])
"""
class VM:
    def __init__(self, vm=None, build=None):
        assert build is None or build in _vmbuildChoices
        self.build = build if build else _vmbuild
        if vm == 'jvmci':
            mx.log('WARNING: jvmci VM is deprecated, using server VM with -Mjit instead')
            self.vm = 'server'
            self.jvmciMode = JVMCIMode('jit')
        else:
            assert vm is None or vm in _vmChoices.keys()
            self.vm = vm if vm else _vm
            self.jvmciMode = None

    def __enter__(self):
        global _vm, _vmbuild
        self.previousVm = _vm
        self.previousBuild = _vmbuild
        mx.reInstantiateDistribution('JVM_<vmbuild>_<vm>', dict(vm=self.previousVm, vmbuild=self.previousBuild), dict(vm=self.vm, vmbuild=self.build))
        _vm = self.vm
        _vmbuild = self.build
        if self.jvmciMode is not None:
            self.jvmciMode.__enter__()

    def __exit__(self, exc_type, exc_value, traceback):
        global _vm, _vmbuild
        mx.reInstantiateDistribution('JVM_<vmbuild>_<vm>', dict(vm=self.vm, vmbuild=self.build), dict(vm=self.previousVm, vmbuild=self.previousBuild))
        _vm = self.previousVm
        _vmbuild = self.previousBuild
        if self.jvmciMode is not None:
            self.jvmciMode.__exit__(exc_type, exc_value, traceback)

def chmodRecursive(dirname, chmodFlagsDir):
    if mx.get_os() == 'windows':
        return

    for root, _, _ in os.walk(dirname):
        os.chmod(root, chmodFlagsDir)

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

    infos['jdkversion'] = str(get_jvmci_bootstrap_jdk().version)

    infos['architecture'] = mx.get_arch()
    infos['platform'] = mx.get_os()

    if mx.get_os() != 'windows':
        pass
        # infos['ccompiler']
        # infos['linker']

    infos['hostname'] = socket.gethostname()

    def _writeJson(suffix, properties):
        d = infos.copy()
        for k, v in properties.items():
            assert not d.has_key(k)
            d[k] = v

        jsonFileName = 'export-' + suffix + '.json'
        with open(jsonFileName, 'w') as f:
            print(json.dumps(d), file=f)
        return jsonFileName


    def _genFileName(archivetype, middle):
        idPrefix = infos['revision'] + '_'
        idSuffix = '.tar.gz'
        return join(_suite.dir, "graalvm_" + archivetype + "_" + idPrefix + middle + idSuffix)

    def _genFileArchPlatformName(archivetype, middle):
        return _genFileName(archivetype, infos['platform'] + '_' + infos['architecture'] + '_' + middle)


    # archive different build types of hotspot
    for vmBuild in _vmbuildChoices:
        jdkDir = _jdkDir(_jdksDir(), vmBuild)
        if not exists(jdkDir):
            mx.logv("skipping " + vmBuild)
            continue

        tarName = _genFileArchPlatformName('basejdk', vmBuild)
        mx.logv("creating basejdk " + tarName)
        vmSet = set()
        with tarfile.open(tarName, 'w:gz') as tar:
            for root, _, files in os.walk(jdkDir):
                if basename(root) in _vmChoices.keys():
                    # TODO: add some assert to check path assumption
                    vmSet.add(root)
                    continue

                for f in files:
                    name = join(root, f)
                    # print(name)
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
                            # print(name)
                            tar.add(name, name)

                n = _writeJson("vm-" + vmBuild + "-" + bVm, {'vmbuild' : vmBuild, 'vm' : bVm})
                tar.add(n, n)

            if len(debugFiles) > 0:
                debugTarName = _genFileArchPlatformName('debugfilesvm', vmBuild + '_' + bVm)
                mx.logv("creating debugfilesvm " + debugTarName)
                with tarfile.open(debugTarName, 'w:gz') as tar:
                    for f in debugFiles:
                        name = join(root, f)
                        # print(name)
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
                # print(name)
                tar.add(name, name)

        n = _writeJson("jvmci", {'javacompiler' : 'javac'})
        tar.add(n, n)

def relativeVmLibDirInJdk():
    mxos = mx.get_os()
    if mxos == 'darwin':
        return join('jre', 'lib')
    if mxos in ['windows', 'cygwin']:
        return join('jre', 'bin')
    return join('jre', 'lib', mx.get_arch())

def vmLibDirInJdk(jdkDir):
    """
    Gets the directory within a JDK where the server and client
    sub-directories are located.
    """
    return join(jdkDir, relativeVmLibDirInJdk())

def getVmJliLibDirs(jdkDir):
    """
    Get the directories within a JDK where the jli library designates to.
    """
    mxos = mx.get_os()
    if mxos == 'darwin':
        return [join(jdkDir, 'jre', 'lib', 'jli')]
    if mxos in ['windows', 'cygwin']:
        return [join(jdkDir, 'jre', 'bin'), join(jdkDir, 'bin')]
    return [join(jdkDir, 'jre', 'lib', mx.get_arch(), 'jli'), join(jdkDir, 'lib', mx.get_arch(), 'jli')]

def getVmCfgInJdk(jdkDir, jvmCfgFile='jvm.cfg'):
    """
    Get the jvm.cfg file.
    """
    mxos = mx.get_os()
    if mxos in ['windows', 'cygwin']:
        return join(jdkDir, 'jre', 'lib', mx.get_arch(), jvmCfgFile)
    return join(vmLibDirInJdk(jdkDir), jvmCfgFile)

def _jdksDir():
    bootstrap_jdk = get_jvmci_bootstrap_jdk()
    if bootstrap_jdk.is_openjdk:
        prefix = 'openjdk'
    else:
        prefix = 'jdk'
    plat = mx.get_os() + '-' + mx.get_arch()
    return os.path.abspath(join(_installed_jdks if _installed_jdks else _suite.dir, prefix + str(bootstrap_jdk.version), plat))

def _jdkDir(jdks, build):
    bootstrap_jdk = get_jvmci_bootstrap_jdk()
    if bootstrap_jdk.home.endswith('/Contents/Home'):
        return join(jdks, build, 'Contents/Home')
    else:
        return join(jdks, build)

def _handle_missing_VM(bld, vm=None):
    if not vm:
        vm = get_vm()
    mx.log('The ' + bld + ' ' + vm + ' VM has not been created')
    if mx.is_interactive():
        if mx.ask_yes_no('Build it now', 'y'):
            with VM(vm, bld):
                build([])
            return
    mx.abort('You need to run "mx --vm=' + vm + ' --vmbuild=' + bld + ' build" to build the selected VM')

def check_VM_exists(vm, jdkDir, build=None):
    if not build:
        build = _vmbuild
    jvmCfg = getVmCfgInJdk(jdkDir)
    found = False
    with open(jvmCfg) as f:
        for line in f:
            if line.strip() == '-' + vm + ' KNOWN':
                found = True
                break
    if not found:
        _handle_missing_VM(build, vm)

def get_jvmci_jdk_dir(build=None, vmToCheck=None, create=False, deployDists=True):
    """
    Gets the path of the JVMCI JDK corresponding to 'build' (or '_vmbuild'), creating it
    first if it does not exist and 'create' is True. If the JDK was created or
    'deployDists' is True, then the JDK deployable distributions are deployed into
    the JDK.
    """
    if not build:
        build = _vmbuild
    jdkDir = _jdkDir(_jdksDir(), build)
    if create:
        if not exists(jdkDir):
            srcJdk = get_jvmci_bootstrap_jdk().home
            mx.log('Creating ' + jdkDir + ' from ' + srcJdk)
            if jdkDir.endswith('/Contents/Home'):
                srcJdkRoot = srcJdk[:-len('/Contents/Home')]
                jdkDirRoot = jdkDir[:-len('/Contents/Home')]
                shutil.copytree(srcJdkRoot, jdkDirRoot, symlinks=True)
            else:
                shutil.copytree(srcJdk, jdkDir)

            # Make a copy of the default VM so that this JDK can be
            # reliably used as the bootstrap for a HotSpot build.
            jvmCfg = getVmCfgInJdk(jdkDir)
            if not exists(jvmCfg):
                mx.abort(jvmCfg + ' does not exist')

            jvmCfgLines = []
            with open(jvmCfg) as f:
                jvmCfgLines = f.readlines()

            chmodRecursive(jdkDir, JDK_UNIX_PERMISSIONS_DIR)

            if mx.get_os() != 'windows':
                os.chmod(jvmCfg, JDK_UNIX_PERMISSIONS_FILE)
            with open(jvmCfg, 'w') as fp:
                for line in jvmCfgLines:
                    fp.write(line)

            # Install a copy of the disassembler library
            try:
                if mx.get_env('INCLUDE_HSDIS', 'true').lower() == 'true':
                    hsdis_args = []
                    syntax = mx.get_env('HSDIS_SYNTAX')
                    if syntax:
                        hsdis_args = [syntax]
                    hsdis(hsdis_args, copyToDir=vmLibDirInJdk(jdkDir))
            except SystemExit:
                pass
    else:
        if not exists(jdkDir):
            if _installed_jdks:
                mx.log("The selected JDK directory does not (yet) exist: " + jdkDir)
            _handle_missing_VM(build, vmToCheck)

    if deployDists:
        for jdkDist in jdkDeployedDists:
            dist = jdkDist.dist()
            if exists(dist.path):
                _installDistInJdks(jdkDist)

        # patch 'release' file (append jvmci revision)
        releaseFile = join(jdkDir, 'release')
        if exists(releaseFile):
            releaseFileLines = []
            with open(releaseFile) as f:
                for line in f:
                    releaseFileLines.append(line)

            if mx.get_os() != 'windows':
                os.chmod(releaseFile, JDK_UNIX_PERMISSIONS_FILE)
            with open(releaseFile, 'w') as fp:
                for line in releaseFileLines:
                    timmedLine = line.strip()
                    if timmedLine.startswith('SOURCE="') and timmedLine.endswith('"'):
                        try:
                            versions = OrderedDict()
                            for p in timmedLine[len('SOURCE="'):-len('"')].split(' '):
                                if p:
                                    idx = p.index(':')
                                    versions[p[:idx]] = p[idx+1:]
                            if _suite.vc:
                                versions['jvmci'] = _suite.vc.parent(_suite.dir)[:12]
                            else:
                                versions['jvmci'] = "unknown"
                            if 'hotspot' in versions:
                                del versions['hotspot']
                            fp.write('SOURCE=" ' + ' '.join((k + ":" + v for k, v in versions.items())) + '"' + os.linesep)
                            mx.logv("Updating " + releaseFile)
                        except BaseException as e:
                            mx.warn("Exception " + str(e) + " while updating release file")
                            fp.write(line)
                    else:
                        fp.write(line)

    if vmToCheck is not None:
        jvmCfg = getVmCfgInJdk(jdkDir)
        found = False
        with open(jvmCfg) as f:
            for line in f:
                if line.strip() == '-' + vmToCheck + ' KNOWN':
                    found = True
                    break
        if not found:
            _handle_missing_VM(build, vmToCheck)

    return jdkDir

def copyToJdk(src, dst, permissions=JDK_UNIX_PERMISSIONS_FILE):
    name = os.path.basename(src)
    mx.ensure_dir_exists(dst)
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

def _installDistInJdks(deployableDist):
    """
    Installs the jar(s) for a given Distribution into all existing JVMCI JDKs
    """
    jdks = _jdksDir()
    if exists(jdks):
        for e in os.listdir(jdks):
            jdkDir = _jdkDir(jdks, e)
            deployableDist.deploy(jdkDir)

def _vmbuildFromJdkDir(jdkDir):
    """
    Determines the VM build corresponding to 'jdkDir'.
    """
    jdksDir = _jdksDir()
    assert jdkDir.startswith(jdksDir)
    if jdkDir.endswith('/Contents/Home'):
        jdkDirRoot = jdkDir[:-len('/Contents/Home')]
        vmbuild = os.path.relpath(jdkDirRoot, jdksDir)
    else:
        vmbuild = os.path.relpath(jdkDir, jdksDir)
    assert vmbuild in _vmbuildChoices, 'The vmbuild derived from ' + jdkDir + ' is unknown: ' + vmbuild
    return vmbuild

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


def _runActionInWinSDKEnv(action_name, action_command, workingDir):
    """
    Runs an action in a Windows SDK environment.
    """

    # Check if we should use env as is, or set it up
    use_env = mx.get_env('UseEnv', 'false') == 'true'

    if not use_env:
        winSDK = mx.get_env('WIN_SDK', 'C:\\Program Files\\Microsoft SDKs\\Windows\\v7.1\\')
        if not exists(mx._cygpathW2U(winSDK)):
            mx.abort("Could not find Windows SDK : '" + winSDK + "' does not exist")

        winSDKSetEnv = mx._cygpathW2U(join(winSDK, 'Bin', 'SetEnv.cmd'))
        if not exists(winSDKSetEnv):
            mx.abort("Invalid Windows SDK path ({}) : could not find Bin\\SetEnv.cmd (you can use"
                     " the WIN_SDK environment variable to specify an other path)".format(winSDK))

    action_command_file = action_name + '.cmd'
    with open(action_command_file, 'w') as fp:
        with open(os.devnull if use_env else winSDKSetEnv) as in_fp:
            for line in in_fp.readlines():
                if line.startswith('CLS'):
                    # Disable screen clearing
                    pass
                else:
                    fp.write(line)
        print('cd /D ' + workingDir + ' & ' + action_command, file=fp)

    stdout = open(action_name + '.log', 'w') if not mx.get_opts().verbose else None
    cmd = 'cmd.exe /D /E:ON /V:ON /C "' + mx._cygpathU2W(action_command_file) + '"'
    mx.log('Executing{}: {} (output in {})'.format(
        '' if use_env else ' in Windows SDK Debug Environment',
        action_name,
        'console' if stdout is None else stdout.name))
    subprocess.check_call(cmd, stdout=stdout, stderr=subprocess.STDOUT, universal_newlines=True)
    # Files only removed if action exited with 0 return code
    os.remove(action_command_file)
    if stdout is not None:
        stdout.close()
        os.remove(stdout.name)

def jdkhome(vm=None):
    """return the JDK directory selected for the 'vm' command"""
    return get_jvmci_jdk_dir(deployDists=False)

def print_jdkhome(args, vm=None):
    """print the JDK directory selected for the 'vm' command"""
    print(jdkhome(vm))

def buildvars(args):
    """describe the variables that can be set by the -D option to the 'mx build' commmand"""

    buildVars = {
        'ALT_BOOTDIR' : 'The location of the bootstrap JDK installation (default: ' + get_jvmci_bootstrap_jdk().home + ')',
        'ALT_OUTPUTDIR' : 'Build directory',
        'HOTSPOT_BUILD_JOBS' : 'Number of CPUs used by make (default: ' + str(mx.cpu_count()) + ')',
        'INSTALL' : 'Install the built VM into the JDK? (default: y)',
        'ZIP_DEBUGINFO_FILES' : 'Install zipped debug symbols file? (default: 0)',
    }

    mx.log('HotSpot build variables that can be set by the -D option to "mx build":')
    mx.log('')
    for n in sorted(buildVars.keys()):
        mx.log(n)
        mx.log(textwrap.fill(buildVars[n], initial_indent='    ', subsequent_indent='    ', width=200))

    mx.log('')
    mx.log('Note that these variables can be given persistent values in the file ' + join(_suite.mxDir, 'env') + ' (see \'mx about\').')

def _hotspotReplaceResultsVar(m):
    var = m.group(1)
    if var == 'os':
        return _hotspotOs(mx.get_os())
    if var == 'buildname':
        return _hotspotGetVariant()
    if var == 'vmbuild':
        return _vmbuild
    return mx._replaceResultsVar(m)

class HotSpotProject(mx.NativeProject):  # pylint: disable=too-many-ancestors
    def __init__(self, suite, name, deps, workingSets, results, output, **args):
        mx.NativeProject.__init__(self, suite, name, "", [], deps, workingSets, results, output, join(suite.dir, "src")) # TODO...

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
    variant = {'client': 'compiler1', 'server': 'compiler2'}.get(vm, vm)
    return variant

def get_hotspot_release_version(tag=mx.DEFAULT_JDK_TAG):
    """
    Gets the value to use for the HOTSPOT_RELEASE_VERSION make variable for the output of ``java -version``.
    """
    jdk = mx.get_jdk(tag=tag)
    output = subprocess.check_output([jdk.java, '-version'], stderr=subprocess.STDOUT, universal_newlines=True).strip().split('\n')
    # Last line of `java -version` output is the HotSpot version info. Here are some samples:
    #   OpenJDK 64-Bit Server VM (build 25.71-b01-internal-jvmci-0.49-dev, mixed mode)
    #   Java HotSpot(TM) 64-Bit Server VM (build 25.192-b12, mixed mode)
    last = output[-1]
    m = re.search(r'.*\(build (\d+.\d+-b\d+)', last)
    if m is None:
        mx.abort('Could not find HotSpot version in last line of `java -version` output:\n' + last)
    return m.group(1)

class HotSpotBuildTask(mx.NativeBuildTask):
    def __init__(self, project, args, vmbuild, vm):
        mx.NativeBuildTask.__init__(self, args, project)
        self.vm = vm
        self.vmbuild = vmbuild
        self.is_windows = mx.get_os() == 'windows' or mx.get_os() == 'cygwin'

    def __str__(self):
        return 'Building HotSpot[{}, {}]'.format(self.vmbuild, self.vm)

    def build(self):
        if self.vm.startswith('server'):
            buildSuffix = ''
        else:
            assert self.vm.startswith('client'), self.vm
            buildSuffix = '1'

        jvmci_version = _get_jvmci_version()
        hs_release_version = get_hotspot_release_version()

        if self.is_windows:
            mksHome = mx.get_env('MKS_HOME', 'C:\\cygwin\\bin')
            if ' ' in mksHome:
                mx.abort('Spaces are not supported in MKS_HOME: "{}"'.format(mksHome))

            variant = _hotspotGetVariant(self.vm)
            project_config = variant + '_' + self.vmbuild
            jvmciHome = mx._cygpathU2W(_suite.dir)

            # exclude aarch64 files to be compiled on windows
            aarch64_src_dir = os.path.join(jvmciHome, 'src', 'cpu', 'aarch64')
            if os.path.isdir(aarch64_src_dir):
                shutil.rmtree(aarch64_src_dir, ignore_errors=False)

            project_file = jvmciHome + r'\build\vs-amd64\jvm.vcxproj'
            if exists(mx._cygpathW2U(project_file)):
                _runActionInWinSDKEnv('HotSpotClean', 'msbuild ' + project_file + ' /p:Configuration=' + project_config + ' /p:Platform=x64 /target:clean', jvmciHome)
            winCompileCmd = r'set HOTSPOT_RELEASE_VERSION=' + hs_release_version + '-' + jvmci_version + \
                            r'& set HotSpotMksHome=' + mksHome + \
                            r'& set JAVA_HOME=' + mx._cygpathU2W(get_jvmci_bootstrap_jdk().home) + \
                            r'& set path=!JAVA_HOME!\bin;%path%;!HotSpotMksHome!;' \
                            r'& cd /D "' + jvmciHome + r'\make\windows"& call create.bat ' + jvmciHome
            _runActionInWinSDKEnv('PrepareHotSpotBuild', winCompileCmd, jvmciHome)
            winBuildCmd = 'msbuild ' + project_file + ' /p:Configuration=' + project_config + ' /p:Platform=x64 /p:TargetRuntime=Native'
            _runActionInWinSDKEnv('RunHotSpotBuild', winBuildCmd, jvmciHome)
        else:
            def filterXusage(line):
                if not 'Xusage.txt' in line:
                    sys.stderr.write(line + os.linesep)
            cpus = mx.cpu_count()
            makeDir = join(_suite.dir, 'make')
            runCmd = [mx.gmake_cmd(), '-C', makeDir]
            if self.args.keep_going:
                runCmd += ['-k']

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
            setMakeVar('ALT_BOOTDIR', get_jvmci_bootstrap_jdk().home, env=env)
            # setMakeVar("EXPORT_PATH", jdk)
            if mx.get_os() == 'linux' and platform.processor() == 'sparc64':
                # SPARC/Linux
                setMakeVar('INCLUDE_TRACE', 'false', env=env)
                setMakeVar('DISABLE_COMMERCIAL_FEATURES', 'true', env=env)

            setMakeVar('MAKE_VERBOSE', 'y' if mx.get_opts().verbose else '')
            setMakeVar('HOTSPOT_RELEASE_VERSION', hs_release_version)
            setMakeVar('USER_RELEASE_SUFFIX', jvmci_version)
            setMakeVar('ENABLE_JFR', 'true') # JDK-8223147
            setMakeVar('INCLUDE_JVMCI', 'true')
            # setMakeVar('INSTALL', 'y', env=env)
            if mx.get_os() == 'darwin' and platform.mac_ver()[0] != '':
                # Force use of clang on MacOS
                setMakeVar('USE_CLANG', 'true')

            if mx.get_os() == 'solaris':
                # If using sparcWorks, setup flags to avoid make complaining about CC version
                cCompilerVersion = subprocess.Popen('CC -V', stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, universal_newlines=True).stderr.readlines()[0]
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
            if not mx.get_opts().verbose:
                mx.log('--------------- make command line ----------------------')

            envPrefix = ' '.join([key + '=' + env[key] for key in env.keys() if key not in os.environ or env[key] != os.environ[key]])
            if len(envPrefix):
                mx.log('env ' + envPrefix + ' \\')

            runCmd.append(self.vmbuild + buildSuffix)
            runCmd.append("docs")
            # runCmd.append("export_" + build)

            if not mx.get_opts().verbose:
                mx.log(' '.join(runCmd))
                mx.log('--------------------------------------------------------')
            mx.run(runCmd, err=filterXusage, env=env)
        self._newestOutput = None

    def needsBuild(self, newestInput):
        # Skip super (NativeBuildTask) because it always returns true
        (superNeeds, superReason) = mx.ProjectBuildTask.needsBuild(self, newestInput)
        if superNeeds:
            return (superNeeds, superReason)
        newestOutput = self.newestOutput()
        for d in ['src', 'make', join('jvmci', 'jdk.vm.ci.hotspot', 'src_gen', 'hotspot')]:  # TODO should this be replaced by a dependency to the project?
            for root, dirnames, files in os.walk(join(_suite.dir, d), followlinks=True):
                # ignore src/share/tools
                if root == join(_suite.dir, 'src', 'share'):
                    dirnames.remove('tools')
                for f in (join(root, name) for name in files):
                    ts = mx.TimeStampFile(f)
                    if newestOutput:
                        if not newestOutput.exists():
                            return (True, '{} does not exist'.format(newestOutput))
                        if ts.isNewerThan(newestOutput):
                            return (True, '{} is newer than {}'.format(ts, newestOutput))

        # Rebuild if jvmci version changed since last build
        mxOutputDir = mx.ensure_dir_exists(_suite.get_mx_output_dir())
        jvmciVersionFile = join(mxOutputDir, 'jvmci-version')
        if mx.update_file(jvmciVersionFile, _get_jvmci_version()):
            buildtree_make = mx.TimeStampFile(join(_suite.dir, 'make', _hotspotOs(mx.get_os()), 'makefiles', 'buildtree.make'))
            if buildtree_make.exists():
                # A changed jvmci version requires touching make/*/makefiles/buildtree.make
                # so that the generated makefiles are regenerated.
                buildtree_make.touch()
                # Need to also touch src/share/vm/runtime/vm_version.cpp so that the VM is rebuilt
                mx.TimeStampFile(join(_suite.dir, 'src', 'share', 'vm', 'runtime', 'vm_version.cpp')).touch()
            return (True, 'updated jvmci version')

        return (False, None)

    def buildForbidden(self):
        if mx.NativeBuildTask.buildForbidden(self):
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
                raise exc

        def rmIfExists(name):
            if os.path.isdir(name):
                shutil.rmtree(name, ignore_errors=False, onerror=handleRemoveReadonly)
            elif os.path.isfile(name):
                os.unlink(name)

        if self.is_windows:
            rmIfExists(join(_suite.dir, 'build', 'vs-amd64'))
        else:
            makeFiles = join(_suite.dir, 'make')
            if mx.get_opts().verbose:
                outCapture = None
            else:
                def _consume(s):
                    pass
                outCapture = _consume
            mx.run([mx.gmake_cmd(), 'ARCH_DATA_MODEL=64', 'ALT_BOOTDIR=' + get_jvmci_bootstrap_jdk().home, 'clean'], out=outCapture, cwd=makeFiles)
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
                         '    mx --vmbuild=' + firstBuildTarget + ' build')
            return result

    # Call mx.build to compile the Java sources
    parser = AP()
    parser.add_argument('-D', action='append', help='set a HotSpot build variable (run \'mx buildvars\' to list variables)', metavar='name=value')
    parser.add_argument('-k', '--keep-going', action='store_true', help='Pass -k to make command')

    # initialize jdk
    get_jvmci_jdk_dir(create=True)

    mx.build(['--source', '1.7'] + args, parser=parser)

mx_gate.add_jacoco_includes(['jdk.vm.ci.*'])

def run_vm(args, vm=None, nonZeroIsFatal=True, out=None, err=None, cwd=None, timeout=None, vmbuild=None):
    """run a Java program by executing the java executable in a JVMCI JDK"""
    jdkTag = mx.get_jdk_option().tag
    if jdkTag and jdkTag != _JVMCI_JDK_TAG:
        mx.abort('The "--jdk" option must have the tag "' + _JVMCI_JDK_TAG + '" when running a command requiring a JVMCI VM')
    jdk = get_jvmci_jdk(vmbuild=vmbuild)
    return jdk.run_java(args, vm=vm, nonZeroIsFatal=nonZeroIsFatal, out=out, err=err, cwd=cwd, timeout=timeout)

def _unittest_config_participant(config):
    vmArgs, mainClass, mainClassArgs = config
    # Remove entries from class path that are in JVMCI loaded jars
    cpIndex, cp = mx.find_classpath_arg(vmArgs)
    if cp:
        excluded = set()
        for jdkDist in jdkDeployedDists:
            dist = jdkDist.dist()
            excluded.update([d.output_dir() for d in dist.archived_deps() if d.isJavaProject()])
            excluded.add(dist.path)
        cp = os.pathsep.join([e for e in cp.split(os.pathsep) if e not in excluded])
        vmArgs[cpIndex] = cp

    # Run the VM in a mode where application/test classes can
    # access JVMCI loaded classes.
    vmArgs = ['-XX:-UseJVMCIClassLoader'] + vmArgs
    return (vmArgs, mainClass, mainClassArgs)

def _unittest_vm_launcher(vmArgs, mainClass, mainClassArgs):
    run_vm(vmArgs + [mainClass] + mainClassArgs)

mx_unittest.add_config_participant(_unittest_config_participant)

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
    for vm in vms:
        if not isVMSupported(vm):
            mx.log('The ' + vm + ' VM is not supported on this platform - skipping')
            continue

        for vmbuild in builds:
            if not args.console:
                logFile = join(vm + '-' + vmbuild + '.log')
                log = open(join(_suite.dir, logFile), 'wb')
                start = time.time()
                mx.log('BEGIN: ' + vm + '-' + vmbuild + '\t(see: ' + logFile + ')')
                verbose = ['-v'] if mx.get_opts().verbose else []
                # Run as subprocess so that output can be directed to a file
                cmd = [sys.executable, '-u', mx.__file__] + verbose + ['--vm=' + vm, '--vmbuild=' + vmbuild, 'build']
                mx.logv("executing command: " + str(cmd))
                subprocess.check_call(cmd, cwd=_suite.dir, stdout=log, stderr=subprocess.STDOUT)
                duration = datetime.timedelta(seconds=time.time() - start)
                mx.log('END:   ' + vm + '-' + vmbuild + '\t[' + str(duration) + ']')
            else:
                with VM(vm, vmbuild):
                    build([])
            if not args.no_check:
                vmargs = ['-version']
                run_vm(vmargs, vm=vm, vmbuild=vmbuild)
    allDuration = datetime.timedelta(seconds=time.time() - allStart)
    mx.log('TOTAL TIME:   ' + '[' + str(allDuration) + ']')


def _jvmci_gate_runner(args, tasks):
    unittest_vmbuild = None
    # Build server-jvmci now so we can run the unit tests
    with Task('BuildHotSpotJVMCI: product', tasks, tags=[mx_gate.Tags.build]) as t:
        if t:
            buildvms(['--vms', 'server', '--builds', 'product'])
            unittest_vmbuild = 'product'

    # Build the fastdebug VM
    with Task('BuildHotSpotJVMCI: fastdebug', tasks, tags=[mx_gate.Tags.build]) as t:
        if t:
            buildvms(['--vms', 'server', '--builds', 'fastdebug'])
            run_vm(['-XX:+ExecuteInternalVMTests', '-version'], vm='server', vmbuild='fastdebug')
            unittest_vmbuild = unittest_vmbuild or 'fastdebug'

    # Run unit tests
    if unittest_vmbuild:
        with VM('server', unittest_vmbuild):
            with JVMCIMode('hosted'):
                with Task('JVMCI UnitTests: hosted', tasks) as t:
                    if t: unittest(['--suite', 'jvmci', '--enable-timing', '--verbose', '--fail-fast'])

    # Prevent JVMCI modifications from breaking the client build
    if args.buildNonJVMCI:
        with Task('BuildHotSpotVarieties', tasks, disableJacoco=True, tags=[mx_gate.Tags.build]) as t:
            if t:
                buildvms(['--vms', 'client', '--builds', 'fastdebug'])

    with Task('JVMCI Signature Tests', tasks) as t:
        if t: sigtest(['--check', 'binary'])


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
        if not run_vm(['-XX:-TieredCompilation', '-XX:+DeoptimizeALot', '-XX:+VerifyOops'] + args + ['-version']) == 0:
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
    logFile = join(_suite.dir, '.ideal_graph_visualizer.log')
    with open(logFile, 'w') as fp:
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
            mx.abort("IGV ant build & launch failed. Check '" + logFile + "'. You can also try to delete " + join(_suite.dir, 'src/share/tools/IdealGraphVisualizer/nbplatform') + ".")

def c1visualizer(args):
    """run the Cl Compiler Visualizer"""
    extractPath = join(_suite.get_output_root())
    if mx.get_os() == 'windows':
        executable = join(extractPath, 'c1visualizer', 'bin', 'c1visualizer.exe')
    else:
        executable = join(extractPath, 'c1visualizer', 'bin', 'c1visualizer')

    # Check whether the current C1Visualizer installation is up-to-date
    if exists(executable) and not exists(mx.library('C1VISUALIZER_DIST').get_path(resolve=False)):
        mx.log('Updating C1Visualizer')
        shutil.rmtree(join(extractPath, 'c1visualizer'))

    archive = mx.library('C1VISUALIZER_DIST').get_path(resolve=True)

    if not exists(executable):
        zf = zipfile.ZipFile(archive, 'r')
        zf.extractall(extractPath)

    if not exists(executable):
        mx.abort('C1Visualizer binary does not exist: ' + executable)

    if mx.get_os() != 'windows':
        # Make sure that execution is allowed. The zip file does not always specfiy that correctly
        os.chmod(executable, 0o777)

    mx.run([executable])


def hsdis(args, copyToDir=None):
    """download the hsdis library

    This is needed to support HotSpot's assembly dumping features.
    By default it downloads the Intel syntax version, use the 'att' argument to install AT&T syntax."""
    flavor = None
    if mx.get_arch() == "amd64":
        flavor = 'intel'
        if 'att' in args:
            flavor = 'att'

    libpattern = mx.add_lib_suffix('hsdis-' + mx.get_arch() + '-' + mx.get_os() + '-%s')

    sha1s = {
        r'att\hsdis-amd64-windows-%s.dll' : 'bcbd535a9568b5075ab41e96205e26a2bac64f72',
        r'att/hsdis-amd64-linux-%s.so' : '36a0b8e30fc370727920cc089f104bfb9cd508a0',
        r'att/hsdis-amd64-darwin-%s.dylib' : 'c1865e9a58ca773fdc1c5eea0a4dfda213420ffb',
        r'intel\hsdis-amd64-windows-%s.dll' : '6a388372cdd5fe905c1a26ced614334e405d1f30',
        r'intel/hsdis-amd64-linux-%s.so' : '0d031013db9a80d6c88330c42c983fbfa7053193',
        r'intel/hsdis-amd64-darwin-%s.dylib' : '67f6d23cbebd8998450a88b5bef362171f66f11a',
        r'hsdis-sparcv9-solaris-%s.so': '970640a9af0bd63641f9063c11275b371a59ee60',
        r'hsdis-sparcv9-linux-%s.so': '0c375986d727651dee1819308fbbc0de4927d5d9',
        r'hsdis-aarch64-linux-%s.so': 'fcc9b70ac91c00db8a50b0d4345490a68e3743e1',
    }

    if flavor:
        flavoredLib = join(flavor, libpattern)
    else:
        flavoredLib = libpattern
    if flavoredLib not in sha1s:
        mx.warn("hsdis with flavor '{}' not supported on this platform or architecture".format(flavor))
        return

    sha1 = sha1s[flavoredLib]
    lib = flavoredLib % (sha1)
    path = join(_suite.get_output_root(), lib)
    if not exists(path):
        sha1path = path + '.sha1'
        mx.download_file_with_sha1('hsdis', path, ['https://lafo.ssw.uni-linz.ac.at/pub/graal-external-deps/hsdis/' + lib.replace(os.sep, '/')], sha1, sha1path, True, True, sources=False)
    if copyToDir is not None and exists(copyToDir):
        destFileName = mx.add_lib_suffix('hsdis-' + mx.get_arch())
        shutil.copy(path, copyToDir + os.sep + destFileName)

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
                    address = int(address, 16)
                    symbols[address] = symbol
        for f in args.files:
            with open(f) as fp:
                lines = fp.read().splitlines()
            updated = False
            for i in range(0, len(lines)):
                l = lines[i]
                for m in addressRE.finditer(l):
                    sval = m.group(0)
                    val = int(sval, 16)
                    sym = symbols.get(val)
                    if sym:
                        l = l.replace(sval, sym)
                        updated = True
                        lines[i] = l
            if updated:
                mx.log('updating ' + f)
                with open('new_' + f, "w") as fp:
                    for l in lines:
                        print(l, file=fp)

def jniconfig(args):
    """Generate or verify a JNI config file for use by SVM"""

    parser = ArgumentParser(prog='mx jniconfig')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-c', '--compare', metavar='oldfile', action='store', help='Compare the contents of oldfile against the the newly generated file')
    group.add_argument('-g', '--generate', metavar='newfile', action='store', help='Generate the jni config for the current JVM')

    args = parser.parse_args(args)

    run_vm(['-XX:JVMCILibDumpJNIConfig=libgraal.dump'])
    with open('libgraal.dump') as fp:
        lines = fp.read().splitlines()
    os.unlink('libgraal.dump')

    classes = {}
    for l in lines:
        w = l.split(' ')
        classname = w[1].replace('/', '.')
        classdict = classes.setdefault(classname, {})
        classdict['name'] = classname
        if w[0] == 'field':
            classdict.setdefault('fields', set()).add(w[2])
        if w[0] == 'method':
            classdict.setdefault('methods', set()).add(w[2])
        # include array types by default
        arrayname = '[L' + classname + ';'
        arraydict = classes.setdefault(arrayname, {})
        arraydict['name'] = arrayname

    # canonically order the field and method arrays
    for classname in classes:
        c = classes[classname]
        if c.get('fields'):
            c['fields'] = [{"name" : x} for x in sorted(c['fields'])]
        if c.get('methods'):
            c['methods'] = [{"name" : x} for x in sorted(c['methods'])]

    # Convert toplevel dictionary into an ordered list since that's the output format
    newconfig = []
    for key in sorted(classes.keys()):
        newconfig.append(classes[key])

    if args.compare:
        with open(args.compare) as fp:
            oldconfig = json.load(fp)

        oldconfig = sorted(oldconfig, key=lambda value: value['name'])
        for c in oldconfig:
            if c.get('fields'):
                c['fields'] = sorted(c['fields'], key=lambda value: value['name'])
            if c.get('methods'):
                c['methods'] = sorted(c['methods'], key=lambda value: value['name'])

        olddict = {}
        for c in oldconfig:
            olddict[c['name']] = c
        oldnames = set(olddict.keys())

        newdict = {}
        for c in newconfig:
            newdict[c['name']] = c
        newnames = set(newdict.keys())

        missing = (oldnames - newnames)
        if missing:
            print('Missing types:\n  ' + '\n  '.join(missing))
        added = (newnames - oldnames)
        if added:
            print('Added types:')
            for a in added:
                print(json.dumps(a, sort_keys=True, indent=2))

        for name in oldnames & newnames:
            o = olddict[name]
            n = newdict[name]
            old = json.dumps(o, sort_keys=True, indent=2)
            new = json.dumps(n, sort_keys=True, indent=2)
            diff = '\n'.join(difflib.unified_diff(old.split('\n'), new.split('\n')))
            if diff:
                print(name)
                print(diff)
    else:
        with open(args.generate, 'w') as fp:
            fp.write(json.dumps(newconfig, sort_keys=True, indent=2))
        print('Wrote new JNI config to ' + args.generate)

def jol(args):
    """Java Object Layout"""
    joljar = mx.library('JOL_CLI').get_path(resolve=True)

    commands = ['estimates', 'externals', 'footprint', 'heapdump', 'heapdumpstats', 'idealpack', 'internals', 'shapes', 'string-compress', 'help']
    command = 'internals'
    if len(args) == 0:
        command = 'help'
    elif args[0] in commands:
        command, args = args[0], args[1:]

    # classpath operations
    if command in ['estimates', 'externals', 'footprint', 'internals']:
        candidates = mx.findclass(args, logToConsole=False, matcher=lambda s, classname: s == classname or classname.endswith('.' + s) or classname.endswith('$' + s))
        if len(candidates) > 0:
            args = mx.select_items(sorted(candidates))
        if len(args) > 0:
            args = ['-cp', mx.classpath(jdk=mx.get_jdk())] + args

    run_vm(['-javaagent:' + joljar, '-cp', joljar, 'org.openjdk.jol.Main', command] + args)

def deploy_binary(args):
    for vmbuild in ['product', 'fastdebug']:
        for vm in ['server']:
            if vmbuild != _vmbuild or vm != get_vm():
                mx.instantiateDistribution('JVM_<vmbuild>_<vm>', dict(vmbuild=vmbuild, vm=vm))
    mx.deploy_binary(args)

_jvmci_version = None

def _jvmci_versions_in_current_branch():
    """
    Get the list of JVMCI versions from the tags in this suite's repository that are ancestors
    of the current HEAD.

    :rtype: list of tuples composed of major, minor, build
    """
    tags_out = subprocess.check_output(['git', 'log', '--simplify-by-decoration', '--pretty=format:%d', 'HEAD'], cwd=_suite.dir, universal_newlines=True)
    tags_out = tags_out.strip()
    jvmci_tag_re = re.compile(r"^tag: jvmci-(\d+)\.(\d+)-b(\d+)")
    versions = []
    for line in tags_out.split('\n'):
        line = line.strip()
        if not line:
            continue
        assert line.startswith('(') and line.endswith(')'), "Unexpected format: " + line
        for decoration in line[1:-1].split(', '):
            m = jvmci_tag_re.match(decoration)
            if m:
                versions.append(tuple([int(g) for g in m.groups()]))
    if not versions:
        mx.abort('No JVMCI versions found. If this is a shallow clone of the git repo, it may need to be unshallowed and load tags with `git fetch --unshallow --tags`')
    return versions

def _get_jvmci_version():
    global _jvmci_version
    if _jvmci_version is None:
        version_file = join(_suite.dir, "jvmci.version")
        if exists(version_file):
            with open(version_file, 'r') as f:
                _jvmci_version = f.read().strip()
        else:
            versions = _jvmci_versions_in_current_branch()
            newest_version = sorted(versions, reverse=True)[0]
            parent_tags = _suite.vc.parent_tags(_suite.dir)
            result = "jvmci-%d.%d-b%02d" % newest_version
            status = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=_suite.dir, universal_newlines=True)
            if result not in parent_tags or len(status) != 0:
                dev_version = newest_version[0], newest_version[1], newest_version[2] + 1
                result = "jvmci-%d.%d-b%02d-dev" % dev_version
            _jvmci_version = result
    return _jvmci_version

def show_jvmci_version(args):
    """show the jvmci version derived from the "version" and "release" suite attributes"""
    mx.log(_get_jvmci_version())

mx.update_commands(_suite, {
    'build': [build, ''],
    'buildvars': [buildvars, ''],
    'buildvms': [buildvms, '[-options]'],
    'c1visualizer' : [c1visualizer, ''],
    'deploy-binary' : [deploy_binary, ''],
    'export': [export, '[-options] [zipfile]'],
    'hsdis': [hsdis, '[att]'],
    'hcfdis': [hcfdis, ''],
    'igv' : [igv, ''],
    'jdkhome': [print_jdkhome, ''],
    'jniconfig': [jniconfig, ''],
    'jvmci-version': [show_jvmci_version, ''],
    'shortunittest' : [shortunittest, '[unittest options] [--] [VM options] [filters...]', mx_unittest.unittestHelpSuffix],
    'vm': [run_vm, '[-options] class [args...]'],
    'deoptalot' : [deoptalot, '[n]'],
    'longtests' : [longtests, ''],
    'jol' : [jol, ''],
})

mx.add_argument('--vmcwd', dest='vm_cwd', help='current directory will be changed to <path> before the VM is executed', default=None, metavar='<path>')
mx.add_argument('--installed-jdks', help='the base directory in which the JDKs cloned from $JAVA_HOME exist. ' +
                'The VM selected by --vm and --vmbuild options is under this directory (i.e., ' +
                join('<path>', '<jdk-version>', '<vmbuild>', 'jre', 'lib', '<vm>', mx.add_lib_prefix(mx.add_lib_suffix('jvm'))) + ')', default=None, metavar='<path>')

mx.add_argument('-M', '--jvmci-mode', action='store', dest='jvmci_mode', choices=_jvmciModes.keys(), help='the JVMCI mode to use (default: ' + _jvmciMode.jvmciMode + ')')
mx.add_argument('--vm', action='store', dest='vm', choices=list(_vmChoices.keys()) + list(_vmAliases.keys()), help='the VM type to build/run')
mx.add_argument('--vmbuild', action='store', dest='vmbuild', choices=_vmbuildChoices, help='the VM build to build/run (default: ' + _vmbuildChoices[0] + ')')
mx.add_argument('--ecl', action='store_true', dest='make_eclipse_launch', help='create launch configuration for running VM execution(s) in Eclipse')
mx.add_argument('--vmprefix', action='store', dest='vm_prefix', help='prefix for running the VM (e.g. "/usr/bin/gdb --args")', metavar='<prefix>')
mx.add_argument('--gdb', action='store_const', const='/usr/bin/gdb --args', dest='vm_prefix', help='alias for --vmprefix "/usr/bin/gdb --args"')
mx.add_argument('--lldb', action='store_const', const='lldb --', dest='vm_prefix', help='alias for --vmprefix "lldb --"')

_jvmci_bootstrap_jdk = None

def get_jvmci_bootstrap_jdk():
    """
    Gets the JDK from which a JVMCI JDK is created.
    """
    global _jvmci_bootstrap_jdk
    if not _jvmci_bootstrap_jdk:
        def _versionCheck(version):
            return version >= _minVersion and ((not _untilVersion) or version < _untilVersion)
        versionDesc = ">=" + str(_minVersion)
        if _untilVersion:
            versionDesc += " and <" + str(_untilVersion)
        _jvmci_bootstrap_jdk = mx.get_jdk(_versionCheck, versionDescription=versionDesc, tag='default')
        java_version = subprocess.check_output([_jvmci_bootstrap_jdk.java, '-version'], stderr=subprocess.STDOUT, universal_newlines=True)
        _jvmci_bootstrap_jdk.is_openjdk = 'openjdk' in java_version.lower()
        is_oraclejdk = not _jvmci_bootstrap_jdk.is_openjdk
        if is_oraclejdk and platform.mac_ver()[0] != '':
            # OracleJDK on macOS should have a `Contents/Home` top level directory
            if not _jvmci_bootstrap_jdk.home.endswith('/Contents/Home'):
                mx.abort("JAVA_HOME for OracleJDK on MacOS is expected to end with /Contents/Home: " + _jvmci_bootstrap_jdk.home)
    return _jvmci_bootstrap_jdk

_jvmci_bootclasspath_prepends = []

def add_bootclasspath_prepend(dep):
    assert isinstance(dep, mx.ClasspathDependency)
    _jvmci_bootclasspath_prepends.append(dep)

class JVMCI8JDKConfig(mx.JDKConfig):
    def __init__(self, vmbuild):
        # Ignore the deployable distributions here - they are only deployed during building.
        # This significantly reduces the latency of the "mx java" command.
        self.vmbuild = vmbuild
        jdkDir = get_jvmci_jdk_dir(build=self.vmbuild, create=True, deployDists=False)
        mx.JDKConfig.__init__(self, jdkDir, tag=_JVMCI_JDK_TAG)

    def parseVmArgs(self, args, addDefaultArgs=True):
        args = mx.expand_project_in_args(args, insitu=False)
        jacocoArgs = mx_gate.get_jacoco_agent_args()
        if jacocoArgs:
            args = jacocoArgs + args

        args = ['-Xbootclasspath/p:' + dep.classpath_repr() for dep in _jvmci_bootclasspath_prepends] + args

        if '-version' in args:
            ignoredArgs = args[args.index('-version') + 1:]
            if  len(ignoredArgs) > 0:
                mx.log("Warning: The following options will be ignored by the vm because they come after the '-version' argument: " + ' '.join(ignoredArgs))
        return self.processArgs(args, addDefaultArgs=addDefaultArgs)

    # Overrides JDKConfig
    def run_java(self, args, vm=None, nonZeroIsFatal=True, out=None, err=None, cwd=None, timeout=None, env=None, addDefaultArgs=True):
        if vm is None:
            vm = get_vm()

        if not isVMSupported(vm):
            mx.abort('The ' + vm + ' is not supported on this platform')

        if cwd is None:
            cwd = _vm_cwd
        elif _vm_cwd is not None and _vm_cwd != cwd:
            mx.abort("conflicting working directories: do not set --vmcwd for this command")

        args = self.parseVmArgs(args, addDefaultArgs=addDefaultArgs)
        if _make_eclipse_launch:
            mx.make_eclipse_launch(_suite, args, _suite.name + '-' + build, name=None, deps=mx.dependencies())

        pfx = _vm_prefix.split() if _vm_prefix is not None else []
        args = get_jvmci_mode_args() + args
        cmd = pfx + [self.java] + ['-' + vm] + args
        return mx.run(cmd, nonZeroIsFatal=nonZeroIsFatal, out=out, err=err, cwd=cwd, timeout=timeout)

"""
The dict of JVMCI JDKs indexed by vmbuild names.
"""
_jvmci_jdks = {}

def get_jvmci_jdk(vmbuild=None):
    """
    Gets the JVMCI JDK corresponding to 'vmbuild'.
    """
    if not vmbuild:
        vmbuild = _vmbuild
    jdk = _jvmci_jdks.get(vmbuild)
    if jdk is None:
        jdk = JVMCI8JDKConfig(vmbuild)
        _jvmci_jdks[vmbuild] = jdk
    return jdk

mx_unittest.set_vm_launcher('JVMCI VM launcher', _unittest_vm_launcher, get_jvmci_jdk)

class JVMCI8JDKFactory(mx.JDKFactory):
    def getJDKConfig(self):
        jdk = get_jvmci_jdk(_vmbuild)
        check_VM_exists(get_vm(), jdk.home)
        return jdk

    def description(self):
        return "JVMCI JDK"

mx.addJDKFactory(_JVMCI_JDK_TAG, mx.JavaCompliance('8'), JVMCI8JDKFactory())

def mx_post_parse_cmd_line(opts):
    mx.set_java_command_default_jdk_tag(_JVMCI_JDK_TAG)

    # Execute for the side-effect of checking that the
    # boot strap JDK has a compatible version
    get_jvmci_bootstrap_jdk()

    jdkTag = mx.get_jdk_option().tag
    if hasattr(opts, 'vm') and opts.vm is not None:
        global _vm
        _vm = dealiased_vm(opts.vm)
        if jdkTag and jdkTag != _JVMCI_JDK_TAG:
            mx.warn('Ignoring "--vm" option as "--jdk" tag is not "' + _JVMCI_JDK_TAG + '"')
    if opts.jvmci_mode is not None:
        global _jvmciMode
        _jvmciMode = JVMCIMode(opts.jvmci_mode)
    if hasattr(opts, 'vmbuild') and opts.vmbuild is not None:
        global _vmbuild
        _vmbuild = opts.vmbuild
        if jdkTag and jdkTag != _JVMCI_JDK_TAG:
            mx.warn('Ignoring "--vmbuild" option as "--jdk" tag is not "' + _JVMCI_JDK_TAG + '"')

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
        jdkDist.post_parse_cmd_line()
