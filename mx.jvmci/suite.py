suite = {
  "mxversion" : "5.215.7",
  "name" : "jvmci",

  "version" : "19.3-b03",
  "release" : False,

  "url" : "http://openjdk.java.net/projects/graal",
  "developer" : {
    "name" : "Truffle and Graal developers",
    "email" : "graal-dev@openjdk.java.net",
    "organization" : "Graal",
    "organizationUrl" : "http://openjdk.java.net/projects/graal",
  },
  "repositories" : {
    "lafo-snapshots" : {
      "url" : "https://curio.ssw.jku.at/nexus/content/repositories/snapshots",
      "licenses" : ["GPLv2-CPE", "UPL"]
    },
  },

  "licenses" : {
    "UPL" : {
      "name" : "Universal Permissive License, Version 1.0",
      "url" : "http://opensource.org/licenses/UPL",
    }
  },

  "defaultLicense" : "GPLv2-CPE",

    # ------------- Libraries -------------

  "libraries" : {

    # ------------- Libraries -------------

    "HCFDIS" : {
      "urls" : ["https://lafo.ssw.uni-linz.ac.at/pub/hcfdis-3.jar"],
      "sha1" : "a71247c6ddb90aad4abf7c77e501acc60674ef57",
    },

    "C1VISUALIZER_DIST" : {
      "urls" : ["https://lafo.ssw.uni-linz.ac.at/pub/graal-external-deps/c1visualizer/c1visualizer-1.7.zip"],
      "sha1" : "305a772ccbdc0e42dfa233b0ce6762d0dd1de6de",
    },

    "JOL_CLI" : {
      "sha1" : "45dd0cf195b16e70710a8d6d763cda614cf6f31e",
      "maven" : {
        "groupId" : "org.openjdk.jol",
        "artifactId" : "jol-cli",
        "version" : "0.9",
        "classifier" : "full",
      },
    },

    "BATIK" : {
      "sha1" : "122b87ca88e41a415cf8b523fd3d03b4325134a3",
      "urls" : ["https://lafo.ssw.uni-linz.ac.at/pub/graal-external-deps/batik-all-1.7.jar"],
    },
  },

  "jrelibraries" : {
    "JFR" : {
      "jar" : "jfr.jar",
    }
  },

  "projects" : {

    # ------------- JVMCI:Service -------------

    "jdk.vm.ci.services" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
      "checkstyleVersion" : "8.8",
    },

    # ------------- JVMCI:API -------------

    "jdk.vm.ci.common" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.services",
      ],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.meta" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.code" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.common",
        "jdk.vm.ci.meta"
      ],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.code.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "mx:JUNIT",
        "jdk.vm.ci.common",
        "jdk.vm.ci.code",
        "jdk.vm.ci.runtime",
      ],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.runtime" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.code",
      ],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.runtime.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "mx:JUNIT",
        "jdk.vm.ci.common",
        "jdk.vm.ci.runtime",
      ],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.aarch64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.code"],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,AArch64",
    },

    "jdk.vm.ci.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.code"],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,AMD64",
    },

    "jdk.vm.ci.sparc" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.code"],
      "checkstyle" : "jdk.vm.ci.services",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,SPARC",
    },

    # ------------- JVMCI:HotSpot -------------

    "jdk.vm.ci.hotspot" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.runtime",
      ],
      "checkstyleVersion" : "8.8",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.vm.ci.hotspot.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "mx:JUNIT",
        "jdk.vm.ci.services",
        "jdk.vm.ci.hotspot",
        "jdk.vm.ci.common",
        "jdk.vm.ci.runtime",
        "jdk.vm.ci.code.test",
      ],
      "checkstyle" : "jdk.vm.ci.hotspot",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.vm.ci.hotspot.aarch64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.aarch64",
        "jdk.vm.ci.hotspot",
      ],
      "checkstyle" : "jdk.vm.ci.hotspot",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,AArch64",
    },

    "jdk.vm.ci.hotspot.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.amd64",
        "jdk.vm.ci.hotspot",
      ],
      "checkstyle" : "jdk.vm.ci.hotspot",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,AMD64",
    },

    "jdk.vm.ci.hotspot.sparc" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.sparc",
        "jdk.vm.ci.hotspot",
      ],
      "checkstyle" : "jdk.vm.ci.hotspot",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,SPARC",
    },

    "jdk.vm.ci.hotspot.jfr" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.hotspot",
        "JFR",
      ],
      "checkstyle" : "jdk.vm.ci.hotspot",
      "javaCompliance" : "1.8",
      "profile" : "",
      "workingSets" : "JVMCI,HotSpot",
    },

    "hotspot" : {
      "native" : True,
      "dependencies" : [
        "jdk.vm.ci.hotspot",
      ],
      "class" : "HotSpotProject",
      "output" : "build",
      # vs-<arch>/<buildname>/generated/jvmtifiles/jvmti.h
      # vs-<arch>/<buildname>/<vmbuild>/<lib:jvm>
      # vs-<arch>/<buildname>/<vmbuild>/<libdebug:jvm>
      "os_arch" : {
        "windows" : {
          "<others>" : {
            "results" : [
              'vs-<arch>/<buildname>/generated/jvmtifiles/jvmti.h',
              'vs-<arch>/<buildname>/<vmbuild>/<lib:jvm>',
              'vs-<arch>/<buildname>/<vmbuild>/<libdebug:jvm>',
            ]
          }
        },
        "<others>": {
          "<others>" : {
            "results" : [
              '<os>/<os>_<arch>_<buildname>/generated/jvmtifiles/jvmti.h',
              '<os>/<os>_<arch>_<buildname>/generated/sa-jdi.jar',
              '<os>/<os>_<arch>_<buildname>/<vmbuild>/<lib:jvm>',
              '<os>/<os>_<arch>_<buildname>/<vmbuild>/<libdebug:jvm>',
              '<os>/<os>_<arch>_<buildname>/<vmbuild>/<lib:saproc>',
              '<os>/<os>_<arch>_<buildname>/<vmbuild>/<libdebug:saproc>',
              '<os>/<os>_<arch>_<buildname>/<vmbuild>/<lib:jsig>',
              '<os>/<os>_<arch>_<buildname>/<vmbuild>/<libdebug:jsig>',
            ]
          }
        },
      },
    }
  },

  "distributions" : {

    # ------------- Distributions -------------

    "JVM_<vmbuild>_<vm>" : {
      "dependencies" : ["hotspot"],
      "native" : True,
      "os_arch" : {
        "linux" : {
          "amd64" : {
            "path" : "build/<vmbuild>/linux/amd64/<vm>/jvm.tar",
          },
          "sparcv9" : {
            "path" : "build/<vmbuild>/linux/sparcv9/<vm>/jvm.tar",
          }
        },
        "darwin" : {
          "amd64" : {
            "path" : "build/<vmbuild>/darwin/amd64/<vm>/jvm.tar",
          }
        },
        "windows" : {
          "amd64" : {
            "path" : "build/<vmbuild>/windows/amd64/<vm>/jvm.tar",
          }
        },
        "solaris" : {
          "amd64" : {
            "path" : "build/<vmbuild>/solaris/amd64/<vm>/jvm.tar",
          },
          "sparcv9" : {
            "path" : "build/<vmbuild>/solaris/sparcv9/<vm>/jvm.tar",
          }
        },
      },
    },

    "JVMCI_SERVICES" : {
      "subDir" : "jvmci",
      "dependencies" : ["jdk.vm.ci.services"],
    },

    "JVMCI_API" : {
      "subDir" : "jvmci",
      "dependencies" : [
        "jdk.vm.ci.runtime",
        "jdk.vm.ci.common",
        "jdk.vm.ci.aarch64",
        "jdk.vm.ci.amd64",
        "jdk.vm.ci.sparc",
      ],
      "distDependencies" : [
        "JVMCI_SERVICES",
      ],
    },

    "JVMCI_HOTSPOT" : {
      "subDir" : "jvmci",
      "dependencies" : [
        "jdk.vm.ci.hotspot.aarch64",
        "jdk.vm.ci.hotspot.amd64",
        "jdk.vm.ci.hotspot.sparc",
        "jdk.vm.ci.hotspot.jfr",
      ],
      "distDependencies" : [
        "JVMCI_SERVICES",
        "JVMCI_API",
      ],
    },

    "JVMCI_TEST" : {
      "subDir" : "jvmci",
      "dependencies" : [
        "jdk.vm.ci.runtime.test",
        "jdk.vm.ci.hotspot.test",
      ],
      "distDependencies" : [
        "JVMCI_API",
        "JVMCI_HOTSPOT",
      ],
      "exclude" : ["mx:JUNIT"],
    },
  },
}
