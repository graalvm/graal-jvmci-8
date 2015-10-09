suite = {
  "mxversion" : "5.5.6",
  "name" : "jvmci",
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
      "path" : "lib/hcfdis-3.jar",
      "urls" : ["https://lafo.ssw.uni-linz.ac.at/pub/hcfdis-3.jar"],
      "sha1" : "a71247c6ddb90aad4abf7c77e501acc60674ef57",
    },

    "C1VISUALIZER_DIST" : {
      "path" : "lib/c1visualizer_2015-07-22.zip",
      "urls" : ["https://java.net/downloads/c1visualizer/c1visualizer_2015-07-22.zip"],
      "sha1" : "7ead6b2f7ed4643ef4d3343a5562e3d3f39564ac",
    },

    "JOL_INTERNALS" : {
      "path" : "lib/jol-internals.jar",
      "urls" : ["https://lafo.ssw.uni-linz.ac.at/pub/truffle/jol/jol-internals.jar"],
      "sha1" : "508bcd26a4d7c4c44048990c6ea789a3b11a62dc",
    },

    "BATIK" : {
      "path" : "lib/batik-all-1.7.jar",
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

    "jdk.vm.ci.service" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.service.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.service"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen,HotSpot",
    },

    # ------------- JVMCI:API -------------

    "jdk.vm.ci.common" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.meta" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.code" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.meta"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.runtime" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.code"
      ],
      "checkstyle" : "jdk.vm.ci.service",
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
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.vm.ci.inittimer" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.vm.ci.options" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.vm.ci.service",
      "dependencies" : ["jdk.vm.ci.inittimer"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.vm.ci.compiler" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.options",
        "jdk.vm.ci.runtime",
      ],
      "checkstyle" : "jdk.vm.ci.service",
      "annotationProcessors" : ["JVMCI_OPTIONS_PROCESSOR"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.vm.ci.options.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.options",
      ],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen",
    },

    "jdk.vm.ci.options.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.options",
        "mx:JUNIT",
      ],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    # ------------- JVMCI:HotSpot -------------

    "jdk.vm.ci.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.code"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,AMD64",
    },

    "jdk.vm.ci.sparc" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.code"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,SPARC",
    },

    "jdk.vm.ci.hotspot" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.hotspotvmconfig",
        "jdk.vm.ci.compiler",
        "jdk.vm.ci.common",
        "jdk.vm.ci.service",
      ],
      "annotationProcessors" : [
        "JVMCI_HOTSPOTVMCONFIG_PROCESSOR",
        "JVMCI_OPTIONS_PROCESSOR",
      ],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.vm.ci.hotspotvmconfig" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot",
    },

    "jdk.vm.ci.hotspotvmconfig.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.vm.ci.hotspotvmconfig", "jdk.vm.ci.common"],
      "checkstyle" : "jdk.vm.ci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,Codegen",
    },

    "jdk.vm.ci.hotspot.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.vm.ci.amd64",
        "jdk.vm.ci.hotspot",
      ],
      "checkstyle" : "jdk.vm.ci.service",
      "annotationProcessors" : [
        "JVMCI_SERVICE_PROCESSOR",
      ],
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
      "checkstyle" : "jdk.vm.ci.service",
      "annotationProcessors" : [
        "JVMCI_SERVICE_PROCESSOR",
      ],
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
      "checkstyle" : "jdk.vm.ci.service",
      "annotationProcessors" : ["JVMCI_SERVICE_PROCESSOR"],
      "javaCompliance" : "1.8",
      "profile" : "",
      "workingSets" : "JVMCI,HotSpot",
    },

    "hotspot" : {
      "native" : True,
      "class" : "HotSpotProject",
      "output" : "build<nojvmci>",
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
          "sparcv9" : {
            "path" : "build/<vmbuild>/solaris/sparcv9/<vm>/jvm.tar",
          }
        },
      }
    },

    "JVMCI_SERVICE" : {
      "path" : "build/jvmci-service.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-service.src.zip",
      "dependencies" : ["jdk.vm.ci.service"],
    },

    "JVMCI_OPTIONS" : {
      "path" : "build/jvmci-options.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-options.src.zip",
      "dependencies" : ["jdk.vm.ci.options"],
    },

    "JVMCI_API" : {
      "path" : "build/jvmci-api.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-api.src.zip",
      "dependencies" : [
        "jdk.vm.ci.inittimer",
        "jdk.vm.ci.runtime",
        "jdk.vm.ci.common",
        "jdk.vm.ci.compiler",
        "jdk.vm.ci.amd64",
        "jdk.vm.ci.sparc",
      ],
      "distDependencies" : [
        "JVMCI_OPTIONS",
        "JVMCI_SERVICE",
      ],
    },

    "JVMCI_HOTSPOTVMCONFIG" : {
      "path" : "build/jvmci-hotspotvmconfig.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-hotspotvmconfig.src.zip",
      "dependencies" : [
        "jdk.vm.ci.hotspotvmconfig",
      ],
    },

    "JVMCI_HOTSPOT" : {
      "path" : "build/jvmci-hotspot.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-hotspot.src.zip",
      "dependencies" : [
        "jdk.vm.ci.hotspot.amd64",
        "jdk.vm.ci.hotspot.sparc",
        "jdk.vm.ci.hotspot.jfr",
      ],
      "distDependencies" : [
        "JVMCI_HOTSPOTVMCONFIG",
        "JVMCI_SERVICE",
        "JVMCI_API",
      ],
    },

    "JVMCI_TEST" : {
      "path" : "build/jvmci-test.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-test.src.zip",
      "dependencies" : [
        "jdk.vm.ci.options.test",
        "jdk.vm.ci.runtime.test",
      ],
      "distDependencies" : [
        "JVMCI_API",
      ],
      "exclude" : ["mx:JUNIT"],
    },

    "JVMCI_OPTIONS_PROCESSOR" : {
      "path" : "build/jvmci-options-processor.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-options-processor.src.zip",
      "dependencies" : ["jdk.vm.ci.options.processor"],
      "distDependencies" : [
        "JVMCI_OPTIONS",
      ],
    },

    "JVMCI_HOTSPOTVMCONFIG_PROCESSOR" : {
      "path" : "build/jvmci-hotspotvmconfig-processor.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-hotspotvmconfig-processor.src.zip",
      "dependencies" : ["jdk.vm.ci.hotspotvmconfig.processor"],
      "distDependencies" : [
        "JVMCI_API",
        "JVMCI_HOTSPOTVMCONFIG",
      ],
    },

    "JVMCI_SERVICE_PROCESSOR" : {
      "path" : "build/jvmci-service-processor.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-service-processor.src.zip",
      "dependencies" : ["jdk.vm.ci.service.processor"],
      "distDependencies" : [
        "JVMCI_SERVICE",
      ],
    },
  },
}
