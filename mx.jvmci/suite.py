suite = {
  "mxversion" : "5.0",
  "name" : "jvmci",

    # ------------- Libraries -------------

  "libraries" : {

    # ------------- Libraries -------------

    "HCFDIS" : {
      "path" : "lib/hcfdis-3.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/hcfdis-3.jar"],
      "sha1" : "a71247c6ddb90aad4abf7c77e501acc60674ef57",
    },

    "C1VISUALIZER_DIST" : {
      "path" : "lib/c1visualizer_2015-07-22.zip",
      "urls" : ["https://java.net/downloads/c1visualizer/c1visualizer_2015-07-22.zip"],
      "sha1" : "7ead6b2f7ed4643ef4d3343a5562e3d3f39564ac",
    },

    "JOL_INTERNALS" : {
      "path" : "lib/jol-internals.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/truffle/jol/jol-internals.jar"],
      "sha1" : "508bcd26a4d7c4c44048990c6ea789a3b11a62dc",
    },

    "BATIK" : {
      "path" : "lib/batik-all-1.7.jar",
      "sha1" : "122b87ca88e41a415cf8b523fd3d03b4325134a3",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/graal-external-deps/batik-all-1.7.jar"],
    },
  },

  "jrelibraries" : {
    "JFR" : {
      "jar" : "jfr.jar",
    }
  },

  "projects" : {

    # ------------- JVMCI:Service -------------

    "jdk.internal.jvmci.service" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.service.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.service"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen,HotSpot",
    },

    # ------------- JVMCI:API -------------

    "jdk.internal.jvmci.common" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.meta" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.code" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.meta"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.runtime" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.code"
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.runtime.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "mx:JUNIT",
        "jdk.internal.jvmci.common",
        "jdk.internal.jvmci.runtime",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.debug" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "dependencies" : [
        "jdk.internal.jvmci.service",
      ],
      "annotationProcessors" : ["JVMCI_OPTIONS_PROCESSOR"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Debug",
    },

    "jdk.internal.jvmci.options" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.internal.jvmci.compiler" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.options",
        "jdk.internal.jvmci.code",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "annotationProcessors" : ["JVMCI_OPTIONS_PROCESSOR"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.internal.jvmci.options.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.options",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen",
    },

    "jdk.internal.jvmci.options.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.options",
        "mx:JUNIT",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    # ------------- JVMCI:HotSpot -------------

    "jdk.internal.jvmci.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.code"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,AMD64",
    },

    "jdk.internal.jvmci.sparc" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.code"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,SPARC",
    },

    "jdk.internal.jvmci.hotspot" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.hotspotvmconfig",
        "jdk.internal.jvmci.runtime",
        "jdk.internal.jvmci.common",
        "jdk.internal.jvmci.options",
        "jdk.internal.jvmci.runtime",
        "jdk.internal.jvmci.debug",
      ],
      "annotationProcessors" : [
        "JVMCI_HOTSPOTVMCONFIG_PROCESSOR",
        "JVMCI_OPTIONS_PROCESSOR",
        "JVMCI_SERVICE_PROCESSOR",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.internal.jvmci.hotspotvmconfig" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot",
    },

    "jdk.internal.jvmci.hotspotvmconfig.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.hotspotvmconfig", "jdk.internal.jvmci.common"],
      "checkstyle" : "jdk.internal.jvmci.service",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,Codegen",
    },

    "jdk.internal.jvmci.hotspot.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.amd64",
        "jdk.internal.jvmci.hotspot",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "annotationProcessors" : [
        "JVMCI_SERVICE_PROCESSOR",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,AMD64",
    },

    "jdk.internal.jvmci.hotspot.sparc" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.sparc",
        "jdk.internal.jvmci.hotspot",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "annotationProcessors" : [
        "JVMCI_SERVICE_PROCESSOR",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot,SPARC",
    },

    "jdk.internal.jvmci.hotspot.jfr" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.hotspot",
        "JFR",
      ],
      "checkstyle" : "jdk.internal.jvmci.service",
      "annotationProcessors" : ["JVMCI_SERVICE_PROCESSOR"],
      "javaCompliance" : "1.8",
      "profile" : "",
      "workingSets" : "JVMCI,HotSpot",
    },

    "hotspot" : {
      "native" : True,
      "class" : "HotSpotProject",
      "output" : "build<nojvmci>",
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

  "distributions" : {

    # ------------- Distributions -------------

    "JVM_<vmbuild>_<vm>" : {
      "dependencies" : ["hotspot"],
      "native" : True,
      "os_arch" : {
        "linux" : {
          "amd64" : {
            "path" : "build/<vmbuild>/linux/amd64/<vm>/jvm.tar",
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
      "dependencies" : ["jdk.internal.jvmci.service"],
    },

    "JVMCI_OPTIONS" : {
      "path" : "build/jvmci-options.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-options.src.zip",
      "dependencies" : ["jdk.internal.jvmci.options"],
    },

    "JVMCI_API" : {
      "path" : "build/jvmci-api.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-api.src.zip",
      "dependencies" : [
        "jdk.internal.jvmci.runtime",
        "jdk.internal.jvmci.common",
        "jdk.internal.jvmci.compiler",
        "jdk.internal.jvmci.debug",
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
        "jdk.internal.jvmci.hotspotvmconfig",
      ],
    },

    "JVMCI_HOTSPOT" : {
      "path" : "build/jvmci-hotspot.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-hotspot.src.zip",
      "dependencies" : [
        "jdk.internal.jvmci.hotspot.amd64",
        "jdk.internal.jvmci.hotspot.sparc",
        "jdk.internal.jvmci.hotspot.jfr",
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
        "jdk.internal.jvmci.options.test",
        "jdk.internal.jvmci.runtime.test",
      ],
      "distDependencies" : [
        "JVMCI_API",
      ],
    },

    "JVMCI_OPTIONS_PROCESSOR" : {
      "path" : "build/jvmci-options-processor.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-options-processor.src.zip",
      "dependencies" : ["jdk.internal.jvmci.options.processor"],
      "distDependencies" : [
        "JVMCI_OPTIONS",
      ],
    },

    "JVMCI_HOTSPOTVMCONFIG_PROCESSOR" : {
      "path" : "build/jvmci-hotspotvmconfig-processor.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-hotspotvmconfig-processor.src.zip",
      "dependencies" : ["jdk.internal.jvmci.hotspotvmconfig.processor"],
      "distDependencies" : [
        "JVMCI_API",
        "JVMCI_HOTSPOTVMCONFIG",
      ],
    },

    "JVMCI_SERVICE_PROCESSOR" : {
      "path" : "build/jvmci-service-processor.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-service-processor.src.zip",
      "dependencies" : ["jdk.internal.jvmci.service.processor"],
      "distDependencies" : [
        "JVMCI_SERVICE",
      ],
    },
  },
}
