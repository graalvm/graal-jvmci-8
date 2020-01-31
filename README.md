# graal-jvmci-8

Fork of jdk8u/hotspot with support for JVMCI.


## Building JVMCI JDK 8

To create a JVMCI-enabled JDK 8, make sure you have [`mx`](https://github.com/graalvm/mx) on your system. Then run the following commands:

```
git clone https://github.com/graalvm/graal-jvmci-8
cd graal-jvmci-8
mx --java-home /path/to/jdk8 build
mx --java-home /path/to/jdk8 unittest
export JAVA_HOME=$(mx --java-home /path/to/jdk8 jdkhome)
```

You need to use the same JDK the [GitHub](https://github.com/graalvm/openjdk8-jvmci-builder/releases) downloads are based on as the argument to `--java-home` in the above commands.
The build step above should work on all [supported JDK 8 build platforms](https://wiki.openjdk.java.net/display/Build/Supported+Build+Platforms).
It should also work on other platforms (such as Oracle Linux, CentOS and Fedora as described [here](http://mail.openjdk.java.net/pipermail/graal-dev/2015-December/004050.html)).
If you run into build problems, send a message to http://mail.openjdk.java.net/mailman/listinfo/graal-dev.

### Windows Specifics

Building JDK requires some bash-like environment. Fortunately, the one that comes as a part of the standard
*Git for Windows* installation will suffice, in which case you will just have to set `MKS_HOME` to point
to the directory with Linux tools, e.g.:

```
set MKS_HOME=<GIT_DIR>\usr\bin
```

where `<GIT_DIR>` is a path to your Git installation directory. It is important that there are **NO**
spaces in the path, otherwise the build will fail.

You will also need an *MSVC 2010 SP1* compiler. The following tool chain is recommended:

1. [Microsoft Windows SDK for Windows 7 and .NET Framework 4 (ISO)](https://www.microsoft.com/en-us/download/details.aspx?id=8442)
2. [Microsoft Visual C++ 2010 Service Pack 1 Compiler Update for the Windows SDK 7.1](https://www.microsoft.com/en-us/download/details.aspx?id=4422)
