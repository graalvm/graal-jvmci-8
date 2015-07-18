suite = {
  "mxversion" : "4.3.4",
  "name" : "jvmci",

    # ------------- Libraries -------------

  "libraries" : {

    # ------------- Libraries -------------

    "HCFDIS" : {
      "path" : "lib/hcfdis-2.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/hcfdis-2.jar"],
      "sha1" : "bc8b2253436485e9dbaf81771c259ccfa1a24c80",
    },

    "C1VISUALIZER_DIST" : {
      "path" : "lib/c1visualizer_2014-04-22.zip",
      "urls" : ["https://java.net/downloads/c1visualizer/c1visualizer_2014-04-22.zip"],
      "sha1" : "220488d87affb569b893c7201f8ce5d2b0e03141",
    },

    "JOL_INTERNALS" : {
      "path" : "lib/jol-internals.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/truffle/jol/jol-internals.jar"],
      "sha1" : "508bcd26a4d7c4c44048990c6ea789a3b11a62dc",
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
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.service.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.service"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen,HotSpot",
    },

    # ------------- JVMCI:API -------------

    "jdk.internal.jvmci.common" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.meta" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.code" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.meta"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.runtime" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.code"
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.runtime.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "jdk.internal.jvmci.common",
        "jdk.internal.jvmci.runtime",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "jdk.internal.jvmci.debug" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "dependencies" : [
        "jdk.internal.jvmci.options",
        "jdk.internal.jvmci.code",
        "jdk.internal.jvmci.service",
      ],
      "annotationProcessors" : ["jdk.internal.jvmci.options.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Debug",
    },

    "jdk.internal.jvmci.debug.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "jdk.internal.jvmci.debug",
      ],
      "annotationProcessors" : ["jdk.internal.jvmci.options.processor"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Debug,Test",
    },

    "jdk.internal.jvmci.options" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.internal.jvmci.compiler" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.debug",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["jdk.internal.jvmci.options.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.internal.jvmci.options.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.options",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen",
    },

    "jdk.internal.jvmci.options.test" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "jdk.internal.jvmci.options",
        "JUNIT",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["jdk.internal.jvmci.options.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    # ------------- JVMCI:HotSpot -------------

    "jdk.internal.jvmci.amd64" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.code"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,AMD64",
    },

    "jdk.internal.jvmci.sparc" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.code"],
      "checkstyle" : "com.oracle.graal.graph",
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
        "jdk.internal.jvmci.compiler",
      ],
      "annotationProcessors" : [
        "jdk.internal.jvmci.hotspotvmconfig.processor",
        "jdk.internal.jvmci.options.processor",
        "jdk.internal.jvmci.service.processor",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "jdk.internal.jvmci.hotspotvmconfig" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot",
    },

    "jdk.internal.jvmci.hotspotvmconfig.processor" : {
      "subDir" : "jvmci",
      "sourceDirs" : ["src"],
      "dependencies" : ["jdk.internal.jvmci.hotspotvmconfig", "jdk.internal.jvmci.common"],
      "checkstyle" : "com.oracle.graal.graph",
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
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : [
        "jdk.internal.jvmci.hotspotvmconfig.processor",
        "jdk.internal.jvmci.service.processor",
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
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : [
        "jdk.internal.jvmci.hotspotvmconfig.processor",
        "jdk.internal.jvmci.service.processor",
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
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["jdk.internal.jvmci.service.processor"],
      "javaCompliance" : "1.8",
      "profile" : "",
      "workingSets" : "JVMCI,HotSpot",
    },
  },

  "distributions" : {

    # ------------- Distributions -------------

    "JVMCI_SERVICE" : {
      "path" : "build/jvmci-service.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-service.src.zip",
      "dependencies" : ["jdk.internal.jvmci.service"],
    },

    "JVMCI_API" : {
      "path" : "build/jvmci-api.jar",
      "subDir" : "jvmci",
      "sourcesPath" : "build/jvmci-api.src.zip",
      "dependencies" : [
        "jdk.internal.jvmci.runtime",
        "jdk.internal.jvmci.options",
        "jdk.internal.jvmci.common",
        "jdk.internal.jvmci.debug",
      ],
      "distDependencies" : [
        "JVMCI_SERVICE",
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
        "JVMCI_API",
      ],
    },
  },
}
