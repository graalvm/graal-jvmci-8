{
    overlay: "f598ffb9d249c7f64cde615186d53b41019752a0",
    specVersion: "2",

    Windows:: {
        packages+: {
            "devkit:VS2017-15.9.16+1" : "==0"
        },
        capabilities+: ["windows"],
        name+: "-windows",
        environment+: {
            CI_OS: "windows"
        }
    },
    Linux:: {
        packages+: {
            git: ">=1.8.3",
            make : ">=3.83",
            "gcc-build-essentials" : "==4.9.2",

            # Deps for building GraalVM
            binutils: "==2.23.2",
            llvm: "==3.8",
            ruby: "==2.1.0"
        },
        capabilities+: ["linux"],
        name+: "-linux",
        environment+: {
            CI_OS: "linux"
        }
    },
    Darwin:: {
        packages+: {
            # No need to specify a "make" package as Mac OS X has make 3.81
            # available once Xcode has been installed.

            # Deps for building GraalVM
            llvm: "==4.0.1"
        },
        environment+: {
            CI_OS: "darwin",
            ac_cv_func_basename_r: "no",
            ac_cv_func_clock_getres: "no",
            ac_cv_func_clock_gettime: "no",
            ac_cv_func_clock_settime: "no",
            ac_cv_func_dirname_r: "no",
            ac_cv_func_getentropy: "no",
            ac_cv_func_mkostemp: "no",
            ac_cv_func_mkostemps: "no",
            MACOSX_DEPLOYMENT_TARGET: "10.11",

            # These 2 are needed for pylint on macOS
            LC_ALL: "en_US.UTF-8",
            LANG: "en_US.UTF-8",
        },
        capabilities+: ["darwin_sierra"],
        name+: "-darwin",
    },

    AMD64:: {
        capabilities+: ["amd64"],
        name+: "-amd64",
        environment+: {
            CI_ARCH: "amd64"
        }
    },

    Eclipse:: {
        downloads+: {
            ECLIPSE: {
                name: "eclipse",
                version: "4.5.2",
                platformspecific: true
            }
        },
        environment+: {
            ECLIPSE_EXE: "$ECLIPSE/eclipse"
        },
    },

    JDT:: {
        downloads+: {
            JDT: {
                name: "ecj",
                version: "4.5.1",
                platformspecific: false
            }
        }
    },

    OpenJDK:: {
        local jdk_version = "8u302",
        local jdk_build = "03",

        name+: "-openjdk",
        downloads+: {
            JAVA_HOME: {
                name : "openjdk",
                version : jdk_version + "+" + jdk_build,
                platformspecific: true
            },
            JAVA_HOME_OVERLAY: {
                name : "openjdk-overlay",
                version : jdk_version + "+" + jdk_build,
                platformspecific: true
            }
        }
    },

    # Downstream Graal branch to test against.
    local downstream_branch = "me/GR-30392", # ignore JVMCI in CheckGraalInvariants 

    # Only need to test formatting and building
    # with Eclipse on one platform.
    local eclipse_conf(conf) = if conf.environment["CI_OS"] == "linux" then (self.Eclipse + self.JDT) else {},

    Build(conf):: self.OpenJDK + eclipse_conf(conf) + {
        packages+: {
            "00:pip:logilab-common": "==1.4.4",
            "01:pip:astroid" : "==1.1.0",
            "pip:pylint" : "==1.1.0",
        },
        environment+: {
            MX_PYTHON: "python3",
        },
        publishArtifacts: [
            {
                name: 'build' + conf.name,
                dir: '.',
                patterns: ["java_home"]
            }
        ],
        name: "build-jvmci",
        timelimit: "1:00:00",
        diskspace_required: "10G",
        logs: ["*.log", "*.cmd"],
        targets: ["gate"],
        run+: [
            # To reduce load, the CI system does not fetch all tags so it must
            # be done explicitly as `mx jvmci-version` relies on it.
            # See GR-22662.
            ["git", "fetch", "--tags"],

            ["mx", "--kill-with-sigquit", "--strict-compliance", "gate", "--only-build-jvmci", "--dry-run"],
            ["mx", "--kill-with-sigquit", "--strict-compliance", "gate", "--only-build-jvmci"],
            ["mv", ["mx", "--vm=server", "jdkhome"], "java_home"],
            ["set-export", "JAVA_HOME", "${PWD}/java_home"],
            ["${JAVA_HOME}/bin/java", "-version"],

            # Overlay static libraries
            ["set-export", "OLD_PWD", "${PWD}"],
            ["cd", "${JAVA_HOME_OVERLAY}"],
            ["cd", "Contents/Home", "||", "true"], # macOS
            ["cp", "-r", ".", "${JAVA_HOME}"],
            ["cd", "${OLD_PWD}"],
        ],
    } + if conf.environment["CI_OS"] == "windows" then {
        environment+: {
            # Make `msbuild` and common *nix utilities in MKS_HOME available
            PATH : "C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319;$MKS_HOME;$PATH",
            # Tell `msbuild` to use the environment as is (i.e., the devkit)
            UseEnv: "true",
        },
    } else {},

    local clone_graal = {
        run+: [
            ["git", "--version"],
            ["git", "clone", ["mx", "urlrewrite", "https://github.com/graalvm/graal.git"]],
            ["git", "-C", "graal", "checkout", downstream_branch, "||", "true"],
        ]
    },

    local requireJVMCI(conf) = {
        requireArtifacts+: [
            {
                name: 'build' + conf.name,
                dir: '.'
            }
        ],
        catch_files+: [
            "Graal diagnostic output saved in (?P<filename>.+\\.zip)"
        ],
        run+: [
            ["set-export", "JAVA_HOME", "${PWD}/java_home"]
        ]
    },

    # Run Graal compiler tests
    CompilerTests(conf):: clone_graal + requireJVMCI(conf) + {
        name: "gate-compiler",
        timelimit: "1:00:00",
        logs: ["*.log", "*.cmd"],
        targets: ["gate"],
        run+: [
            ["mx", "-p", "graal/compiler", "gate", "--tags", "build,test"]
        ]
    },

    # Build and test JavaScript on GraalVM
    JavaScriptTests(conf):: clone_graal + requireJVMCI(conf) + {
        name: "gate-js",
        timelimit: "1:30:00",
        logs: ["*.log", "*.cmd"],
        targets: ["gate"],
        local os_path(path) = if conf.environment["CI_OS"] == "windows" then std.strReplace(path, "/", "\\") else path,
        local exe(path) = if conf.environment["CI_OS"] == "windows" then os_path(path) + ".exe" else os_path(path),

        local mx = [
            "mx",
            "-p", "graal/vm",
            "--dynamicimports", "/graal-js,/substratevm",
            "--disable-polyglot",
            "--disable-libpolyglot",
            "--force-bash-launchers=native-image",
        ],

        run+: [
            mx + ["build"],
            ["set-export", "GRAALVM_HOME", mx + ["graalvm-home"] ],
            [exe("${GRAALVM_HOME}/jre/languages/js/bin/js"),          os_path("mx.jvmci/test.js")],
            [exe("${GRAALVM_HOME}/jre/languages/js/bin/js"), "--jvm", os_path("mx.jvmci/test.js")],
        ],
    },

    # Build LibGraal
    BuildLibGraal(conf):: clone_graal + requireJVMCI(conf) + {
        name: "gate-libgraal-build",
        timelimit: "1:30:00",
        logs: ["*.log", "*.cmd"],
        targets: ["gate"],

        publishArtifacts: [
            {
                name: 'build-libgraal' + conf.name,
                dir: '.',
                patterns: ["graal/*/mxbuild"]
            }
        ],
        run+: [
            ["mx", "-p", "graal/vm",
                "--env", "libgraal",
                "--extra-image-builder-argument=-J-esa",
                "--extra-image-builder-argument=-H:+ReportExceptionStackTraces",
                "build"],
        ],
    },

    local requireLibGraal(conf) = {
        requireArtifacts+: [
            {
                name: 'build-libgraal' + conf.name,
                dir: '.',
                autoExtract: false
            }
        ],
    },

    # Test LibGraal
    TestLibGraal(conf):: clone_graal + requireJVMCI(conf) + requireLibGraal(conf) + {
        name: "gate-libgraal-test",
        timelimit: "1:30:00",
        logs: ["*.log", "*.cmd"],
        targets: ["gate"],

        run+: [
            ["unpack-artifact", "build-libgraal" + conf.name],
            ["mx", "-p", "graal/vm",
                "--env", "libgraal",
                "gate", "--task", "LibGraal"],
        ],
    },

    # GraalVM CE is not supported on Solaris-SPARC
    local graalvm_test_confs = [
        self.Linux + self.AMD64,
        # GR-29152 self.Darwin + self.AMD64,
        self.Windows + self.AMD64
    ],

    builds: [ self.Build(conf) + conf for conf in graalvm_test_confs ] +
            [ self.CompilerTests(conf) + conf for conf in graalvm_test_confs ] +
            [ self.JavaScriptTests(conf) + conf for conf in graalvm_test_confs ] +
            [ self.BuildLibGraal(conf) + conf for conf in graalvm_test_confs ] +
            [ self.TestLibGraal(conf) + conf for conf in graalvm_test_confs ]
}
