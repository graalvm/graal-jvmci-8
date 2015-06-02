suite = {
  "mxversion" : "1.0",
  "name" : "graal",
  "libraries" : {

      "JLINE" : {
      "path" : "lib/jline-2.11.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/jline-2.11.jar",
        "https://search.maven.org/remotecontent?filepath=jline/jline/2.11/jline-2.11.jar",
      ],
      "sha1" : "9504d5e2da5d78237239c5226e8200ec21182040",
    },

    "JUNIT" : {
      "path" : "lib/junit-4.11.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/junit-4.11.jar",
        "https://search.maven.org/remotecontent?filepath=junit/junit/4.11/junit-4.11.jar",
      ],
      "sha1" : "4e031bb61df09069aeb2bffb4019e7a5034a4ee0",
      "eclipse.container" : "org.eclipse.jdt.junit.JUNIT_CONTAINER/4",
      "sourcePath" : "lib/junit-4.11-sources.jar",
      "sourceUrls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/junit-4.11-sources.jar",
        "https://search.maven.org/remotecontent?filepath=junit/junit/4.11/junit-4.11-sources.jar",
      ],
      "sourceSha1" : "28e0ad201304e4a4abf999ca0570b7cffc352c3c",
      "dependencies" : ["HAMCREST"],
    },

    "CHECKSTYLE" : {
      "path" : "lib/checkstyle-6.0-all.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/checkstyle-6.0-all.jar",
        "jar:http://sourceforge.net/projects/checkstyle/files/checkstyle/6.0/checkstyle-6.0-bin.zip/download!/checkstyle-6.0/checkstyle-6.0-all.jar",
      ],
      "sha1" : "2bedc7feded58b5fd65595323bfaf7b9bb6a3c7a",
    },

    "HAMCREST" : {
      "path" : "lib/hamcrest-core-1.3.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/hamcrest-core-1.3.jar",
        "https://search.maven.org/remotecontent?filepath=org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
      ],
      "sha1" : "42a25dc3219429f0e5d060061f71acb49bf010a0",
      "sourcePath" : "lib/hamcrest-core-1.3-sources.jar",
      "sourceUrls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/hamcrest-core-1.3-sources.jar",
        "https://search.maven.org/remotecontent?filepath=org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3-sources.jar",
      ],
      "sourceSha1" : "1dc37250fbc78e23a65a67fbbaf71d2e9cbc3c0b",
    },

    "HCFDIS" : {
      "path" : "lib/hcfdis-2.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/hcfdis-2.jar"],
      "sha1" : "bc8b2253436485e9dbaf81771c259ccfa1a24c80",
    },

    "FINDBUGS_DIST" : {
      "path" : "lib/findbugs-dist-3.0.0.zip",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/findbugs-3.0.0.zip",
        "http://sourceforge.net/projects/findbugs/files/findbugs/3.0.0/findbugs-3.0.0.zip/download",
      ],
      "sha1" : "6e56d67f238dbcd60acb88a81655749aa6419c5b",
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

    "FINDBUGS" : {
      "path" : "graal/findbugs-SuppressFBWarnings.jar",
      "sha1" : "fb78822d27c68fabf2cb2e5e573b3cdb5f9cae2d",
    },

    "DACAPO" : {
      "path" : "lib/dacapo-9.12-bach.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/dacapo-9.12-bach.jar",
        "http://softlayer.dl.sourceforge.net/project/dacapobench/9.12-bach/dacapo-9.12-bach.jar",
      ],
      "sha1" : "2626a9546df09009f6da0df854e6dc1113ef7dd4",
    },

    "JACOCOAGENT" : {
      "path" : "lib/jacocoagent.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/jacoco/jacocoagent-0.7.1-1.jar"],
      "sha1" : "2f73a645b02e39290e577ce555f00b02004650b0",
    },

    "JACOCOREPORT" : {
      "path" : "lib/jacocoreport.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/jacoco/jacocoreport-0.7.1-2.jar"],
      "sha1" : "a630436391832d697a12c8f7daef8655d7a1efd2",
    },

    "DACAPO_SCALA" : {
      "path" : "lib/dacapo-scala-0.1.0-20120216.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/dacapo-scala-0.1.0-20120216.jar",
        "http://repo.scalabench.org/snapshots/org/scalabench/benchmarks/scala-benchmark-suite/0.1.0-SNAPSHOT/scala-benchmark-suite-0.1.0-20120216.103539-3.jar",
      ],
      "sha1" : "59b64c974662b5cf9dbd3cf9045d293853dd7a51",
    },

    "JAVA_ALLOCATION_INSTRUMENTER" : {
      "path" : "lib/java-allocation-instrumenter.jar",
      "sourcePath" : "lib/java-allocation-instrumenter.jar",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/java-allocation-instrumenter/java-allocation-instrumenter-8f0db117e64e.jar"],
      "sha1" : "476d9a44cd19d6b55f81571077dfa972a4f8a083",
      "bootClassPathAgent" : "true",
    },

    "VECMATH" : {
      "path" : "lib/vecmath-1.3.1.jar",
      "urls" : [
        "http://lafo.ssw.uni-linz.ac.at/graal-external-deps/vecmath-1.3.1.jar",
        "https://search.maven.org/remotecontent?filepath=java3d/vecmath/1.3.1/vecmath-1.3.1.jar",
      ],
      "sha1" : "a0ae4f51da409fa0c20fa0ca59e6bbc9413ae71d",
    },

    "JMH" : {
      "path" : "lib/jmh-runner-1.4.2.jar",
      "sha1" : "f44bffaf237305512002303a306fc5ce3fa63f76",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/jmh/jmh-runner-1.4.2.jar"],
      "annotationProcessor" : "true"
    },

    "BATIK" : {
      "path" : "lib/batik-all-1.7.jar",
      "sha1" : "122b87ca88e41a415cf8b523fd3d03b4325134a3",
      "urls" : ["http://lafo.ssw.uni-linz.ac.at/graal-external-deps/batik-all-1.7.jar"],
    }
},

  "jrelibraries" : {
    "JFR" : {
      "jar" : "jfr.jar",
    }
  },

  "projects" : {
                
    # ------------- JVMCI -------------
    
    "com.oracle.jvmci.service" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal",
    },

    "com.oracle.jvmci.service.processor" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.service"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Codegen,HotSpot",
    },

    "com.oracle.jvmci.common" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "com.oracle.jvmci.meta" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "com.oracle.jvmci.code" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.meta"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "com.oracle.jvmci.runtime" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.code"
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "com.oracle.jvmci.runtime.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "com.oracle.jvmci.common",
        "com.oracle.jvmci.runtime",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,JVMCI",
    },

    "com.oracle.jvmci.hotspot" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.hotspotvmconfig",
        "com.oracle.jvmci.options",
        "com.oracle.jvmci.debug",
        "com.oracle.jvmci.runtime",
        "com.oracle.jvmci.common",
        "FINDBUGS",
      ],
      "annotationProcessors" : [
        "com.oracle.jvmci.hotspotvmconfig.processor",
        "com.oracle.jvmci.options.processor",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "com.oracle.jvmci.hotspotvmconfig" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,HotSpot",
    },

    "com.oracle.jvmci.hotspotvmconfig.processor" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.hotspotvmconfig", "com.oracle.jvmci.common"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,HotSpot,Codegen",
    },

    "com.oracle.jvmci.debug" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "dependencies" : [
        "com.oracle.jvmci.service",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Debug",
    },

    "com.oracle.jvmci.debug.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "com.oracle.jvmci.debug",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Debug,Test",
    },

    "com.oracle.jvmci.options" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.service",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "com.oracle.jvmci.options.processor" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.options",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI,Codegen",
    },

    "com.oracle.jvmci.options.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.options",
        "JUNIT",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["com.oracle.jvmci.options.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "JVMCI",
    },

    "com.oracle.jvmci.hotspot.jfr" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.hotspot",
        "JFR",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "javaCompliance" : "1.8",
      "profile" : "",
      "workingSets" : "JVMCI,HotSpot",
    },

    # ------------- NFI -------------
    
    "com.oracle.nfi" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.service"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
    },

    "com.oracle.nfi.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["test"],
      "dependencies" : [
        "com.oracle.nfi",
        "com.oracle.jvmci.common",
        "JUNIT",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
    },

    # ------------- Graal -------------
    
    "com.oracle.graal.api.collections" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal",
    },

    "com.oracle.graal.api.directives" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal",
    },

    "com.oracle.graal.api.directives.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "dependencies" : [
        "com.oracle.graal.compiler.test",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal",
    },

    "com.oracle.graal.api.runtime" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.service",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal",
    },

    "com.oracle.graal.api.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "com.oracle.graal.api.runtime",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal,Test",
    },

    "com.oracle.graal.api.replacements" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.meta"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal,Replacements",
    },

    "com.oracle.graal.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.code"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,AMD64",
    },

    "com.oracle.graal.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.code"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,SPARC",
    },

    "com.oracle.graal.hotspot" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.hotspot",
        "com.oracle.graal.replacements",
        "com.oracle.graal.printer",
        "com.oracle.graal.runtime",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : [
        "com.oracle.graal.replacements.verifier",
        "com.oracle.jvmci.service.processor",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,HotSpot",
    },

    "com.oracle.graal.hotspot.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler.amd64",
        "com.oracle.graal.hotspot",
        "com.oracle.graal.replacements.amd64",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,HotSpot,AMD64",
    },

    "com.oracle.graal.hotspot.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler.sparc",
        "com.oracle.graal.replacements.sparc",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor", "com.oracle.graal.compiler.match.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,HotSpot,SPARC",
    },

    "com.oracle.graal.hotspot.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.replacements.test",
        "com.oracle.graal.hotspot",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,HotSpot,Test",
    },

    "com.oracle.graal.hotspot.amd64.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.asm.amd64",
        "com.oracle.graal.hotspot.test",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,HotSpot,AMD64,Test",
    },

    "com.oracle.graal.nodeinfo" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Graph",
    },

    "com.oracle.graal.nodeinfo.processor" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "dependencies" : [
        "com.oracle.graal.nodeinfo",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Graph",
    },

    "com.oracle.graal.graph" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.nodeinfo",
        "com.oracle.graal.compiler.common",
        "com.oracle.graal.api.collections",
        "com.oracle.graal.api.runtime",
        "FINDBUGS",
      ],
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.graal.nodeinfo.processor"],
      "workingSets" : "Graal,Graph",
    },

    "com.oracle.graal.graph.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "dependencies" : [
        "JUNIT",
        "com.oracle.graal.graph",
      ],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Graph,Test",
    },

    "com.oracle.graal.lir" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler.common",
        "com.oracle.graal.asm",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,LIR",
    },

    "com.oracle.graal.lir.jtt" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.jtt",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,LIR",
    },

    "com.oracle.graal.lir.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "com.oracle.graal.lir",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,LIR",
    },

    "com.oracle.graal.lir.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.lir",
        "com.oracle.graal.asm.amd64",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,LIR,AMD64",
    },

    "com.oracle.graal.lir.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.asm.sparc"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,LIR,SPARC",
    },

    "com.oracle.graal.word" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.nodes"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal",
    },

    "com.oracle.graal.replacements" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler",
        "com.oracle.graal.java",
        "com.oracle.graal.api.directives",
        "com.oracle.graal.word",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : [
        "com.oracle.graal.replacements.verifier",
        "com.oracle.jvmci.service.processor",
      ],
      "workingSets" : "Graal,Replacements",
    },

    "com.oracle.graal.replacements.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
          "com.oracle.graal.replacements",
          "com.oracle.graal.lir.amd64",
          ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "workingSets" : "Graal,Replacements,AMD64",
    },

    "com.oracle.graal.replacements.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
          "com.oracle.graal.replacements",
          ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Replacements,SPARC",
    },

    "com.oracle.graal.replacements.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler.test",
        "com.oracle.graal.replacements",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Replacements,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.graal.replacements.verifier" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.api.replacements",
        "com.oracle.graal.graph",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Replacements",
    },

    "com.oracle.graal.nodes" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.graph",
        "com.oracle.graal.api.replacements",
        "com.oracle.graal.lir",
        "com.oracle.graal.bytecode",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.graal.replacements.verifier"],
      "workingSets" : "Graal,Graph",
    },

    "com.oracle.graal.nodes.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.compiler.test"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Graph",
    },

    "com.oracle.graal.phases" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.nodes"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Phases",
    },

    "com.oracle.graal.phases.common" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.phases"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Phases",
    },

    "com.oracle.graal.phases.common.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.runtime",
        "JUNIT",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Test",
    },

    "com.oracle.graal.virtual" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.phases.common"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Phases",
    },

    "com.oracle.graal.virtual.bench" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["JMH"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Bench",
    },

    "com.oracle.graal.loop" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.phases.common"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Phases",
    },

    "com.oracle.graal.compiler" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.virtual",
        "com.oracle.graal.loop",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "workingSets" : "Graal",
    },

    "com.oracle.graal.compiler.match.processor" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Codegen",
    },

    "com.oracle.graal.compiler.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler",
        "com.oracle.graal.lir.amd64",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["com.oracle.graal.compiler.match.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,AMD64",
    },

    "com.oracle.graal.compiler.amd64.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.amd64",
        "com.oracle.graal.lir.jtt",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,AMD64,Test",
    },

    "com.oracle.graal.compiler.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.lir.sparc"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,SPARC",
    },

    "com.oracle.graal.compiler.sparc.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.sparc",
        "com.oracle.graal.compiler.test",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,SPARC,Test",
    },

    "com.oracle.graal.runtime" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.graal.compiler"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal",
    },

    "com.oracle.graal.bytecode" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Java",
    },

    "com.oracle.graal.java" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.phases",
        "com.oracle.graal.graphbuilderconf"
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Java",
    },

    "com.oracle.graal.java.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.runtime.test",
        "com.oracle.graal.java",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "API,Graal,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.graal.graphbuilderconf" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.nodes",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Java",
    },

    "com.oracle.graal.compiler.common" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.jvmci.options",
        "com.oracle.jvmci.debug",
        "com.oracle.jvmci.common",
        "com.oracle.jvmci.code",
      ],
      "annotationProcessors" : ["com.oracle.jvmci.options.processor"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Java",
    },

    "com.oracle.graal.printer" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.java",
        "com.oracle.graal.compiler",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Graph",
    },

    "com.oracle.graal.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "JUNIT",
        "com.oracle.jvmci.debug",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Test",
    },

    "com.oracle.graal.compiler.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.api.directives",
        "com.oracle.graal.test",
        "com.oracle.graal.printer",
        "com.oracle.graal.runtime",
        "JAVA_ALLOCATION_INSTRUMENTER",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.graal.jtt" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.compiler.test",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.graal.asm" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.code"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Assembler",
    },

    "com.oracle.graal.asm.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.test",
        "com.oracle.graal.runtime",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Assembler,Test",
    },

    "com.oracle.graal.asm.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.asm",
        "com.oracle.graal.amd64",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Assembler,AMD64",
    },

    "com.oracle.graal.asm.amd64.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.asm.test",
        "com.oracle.graal.asm.amd64",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Assembler,AMD64,Test",
    },

    "com.oracle.graal.asm.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.hotspot",
        "com.oracle.graal.sparc",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Assembler,SPARC",
    },

    # ------------- Truffle -------------
    
    "com.oracle.truffle.api" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.jvmci.service"],
      "javaCompliance" : "1.7",
      "workingSets" : "API,Truffle",
    },

    "com.oracle.truffle.api.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.truffle.api",
        "JUNIT",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
      "workingSets" : "API,Truffle,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.truffle.api.dsl" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api"],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.7",
      "workingSets" : "API,Truffle,Codegen",
    },

    "com.oracle.truffle.api.dsl.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.truffle.dsl.processor",
        "JUNIT",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
      "annotationProcessors" : ["com.oracle.truffle.dsl.processor"],
      "workingSets" : "API,Truffle,Codegen,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.truffle.dsl.processor" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api.dsl"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
      "workingSets" : "Truffle,Codegen",
    },

    "com.oracle.truffle.api.interop" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api"],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.7",
      "workingSets" : "API,Truffle",
    },

    "com.oracle.truffle.interop" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api.interop"],
      "javaCompliance" : "1.7",
      "workingSets" : "Truffle",
      "checkstyle" : "com.oracle.truffle.api",
    },

    "com.oracle.truffle.api.object" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api.interop"],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.7",
      "workingSets" : "API,Truffle",
    },

    "com.oracle.truffle.object" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api.object"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
      "workingSets" : "Truffle",
    },

    "com.oracle.truffle.object.basic" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.object"],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.7",
      "workingSets" : "Truffle",
    },

    "com.oracle.truffle.tools" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.api"],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.8",
      "workingSets" : "Truffle,Tools",
    },

    "com.oracle.truffle.tools.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
          "com.oracle.truffle.tools",
          "JUNIT"
          ],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.8",
      "workingSets" : "Truffle,Tools",
    },

    "com.oracle.truffle.tools.debug.engine" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.tools"],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.8",
      "workingSets" : "Truffle,Tools",
    },

    "com.oracle.truffle.tools.debug.shell" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : ["com.oracle.truffle.tools.debug.engine",
                        "JLINE"],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.8",
      "workingSets" : "Truffle,Tools",
    },

    "com.oracle.truffle.sl" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.truffle.api.dsl",
        "com.oracle.truffle.interop",
        "com.oracle.truffle.api.object",
        "com.oracle.truffle.tools",
        "FINDBUGS"
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.truffle.dsl.processor"],
      "workingSets" : "Truffle,SimpleLanguage",
    },

    "com.oracle.truffle.sl.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.truffle.api.test",
        "com.oracle.truffle.sl"
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Truffle,SimpleLanguage,Test",
    },

     "com.oracle.truffle.sl.tools" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.truffle.sl",
        "com.oracle.truffle.tools.debug.shell",
      ],
      "checkstyle" : "com.oracle.truffle.api",
      "javaCompliance" : "1.8",
      "workingSets" : "Truffle,SimpleLanguage,Tools",
    },

    # ------------- GraalTruffle -------------
    
    "com.oracle.graal.truffle" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.truffle.api",
        "com.oracle.graal.runtime",
        "com.oracle.graal.printer",
        "com.oracle.graal.replacements",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Truffle",
      "jacoco" : "exclude",
    },

    "com.oracle.graal.truffle.test" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.truffle",
        "com.oracle.graal.graph.test",
        "com.oracle.graal.compiler.test",
        "com.oracle.truffle.sl.test",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "workingSets" : "Graal,Truffle,Test",
      "jacoco" : "exclude",
    },

    "com.oracle.graal.truffle.hotspot" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.truffle",
        "com.oracle.graal.hotspot",
        "com.oracle.nfi",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "workingSets" : "Graal,Truffle",
    },

    "com.oracle.graal.truffle.hotspot.amd64" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.truffle.hotspot",
        "com.oracle.graal.hotspot.amd64",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "workingSets" : "Graal,Truffle",
    },

    "com.oracle.graal.truffle.hotspot.sparc" : {
      "subDir" : "graal",
      "sourceDirs" : ["src"],
      "dependencies" : [
        "com.oracle.graal.truffle.hotspot",
        "com.oracle.graal.asm.sparc",
      ],
      "checkstyle" : "com.oracle.graal.graph",
      "javaCompliance" : "1.8",
      "annotationProcessors" : ["com.oracle.jvmci.service.processor"],
      "workingSets" : "Graal,Truffle,SPARC",
    }
  },

  "distributions" : {

    "JVMCI_SERVICE" : {
      "path" : "build/jvmci-service.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/jvmci-service.src.zip",
      "dependencies" : ["com.oracle.jvmci.service"],
      "exclude" : ["FINDBUGS"],
    },

    "JVMCI_API" : {
      "path" : "build/jvmci-api.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/jvmci-api.src.zip",
      "dependencies" : [
        "com.oracle.jvmci.runtime",
        "com.oracle.jvmci.options",
        "com.oracle.jvmci.common",
        "com.oracle.jvmci.debug",
      ],
      "exclude" : ["FINDBUGS"],
      "distDependencies" : [
        "JVMCI_SERVICE",
      ],
    },

    "JVMCI_HOTSPOT" : {
      "path" : "build/jvmci-hotspot.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/jvmci-hotspot.src.zip",
      "dependencies" : [
        "com.oracle.jvmci.hotspot",
        "com.oracle.jvmci.hotspot.jfr",
      ],
      "distDependencies" : [
        "JVMCI_API",
      ],
    },

    "GRAAL" : {
      "path" : "build/graal.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/graal.src.zip",
      "dependencies" : [
        "com.oracle.graal.hotspot.amd64",
        "com.oracle.graal.hotspot.sparc",
        "com.oracle.graal.hotspot",
      ],
      "exclude" : ["FINDBUGS"],
      "distDependencies" : [
        "JVMCI_HOTSPOT",
        "TRUFFLE",
      ],
    },

    "TRUFFLE" : {
      "path" : "build/truffle.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/truffle.src.zip",
      "javaCompliance" : "1.7",
      "dependencies" : [
        "com.oracle.truffle.api.dsl",
        "com.oracle.nfi",
        "com.oracle.truffle.interop",
        "com.oracle.truffle.object.basic",
      ],
      "distDependencies" : [
        "JVMCI_SERVICE",
      ],
    },

    "GRAAL_TRUFFLE" : {
      "path" : "build/graal-truffle.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/graal-truffle.src.zip",
      "dependencies" : [
        "com.oracle.graal.truffle",
        "com.oracle.graal.truffle.hotspot.amd64",
        "com.oracle.graal.truffle.hotspot.sparc"
      ],
      "exclude" : ["FINDBUGS"],
      "distDependencies" : [
        "GRAAL",
        "TRUFFLE",
      ],
    },

    "TRUFFLE-DSL-PROCESSOR" : {
      "path" : "build/truffle-dsl-processor.jar",
      "subDir" : "graal",
      "sourcesPath" : "build/truffle-dsl-processor.src.zip",
      "javaCompliance" : "1.7",
      "dependencies" : ["com.oracle.truffle.dsl.processor"],
      "distDependencies" : ["TRUFFLE"],
    }
  },
}
