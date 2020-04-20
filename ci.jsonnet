{
    overlay: "7ae71ed606bd7a3c225ac70aa5c93561b67050ac",

    Windows:: {
        capabilities+: ["windows"],
        name+: "-windows",
        environment+: {
            PATH : "$MKS_HOME;$PATH",  # Makes the `test` utility available
            CI_OS: "windows"
        },
        packages+: {
            msvc: "==10.0",
        },
    },
    Linux:: {
        packages+: {
            git: ">=1.8.3",
            mercurial: ">=2.2",
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
    Solaris:: {
        packages+: {
            git: ">=1.8.3",
            mercurial: ">=2.2",
            make : ">=3.83",
            solarisstudio: "==12.3"
        },
        capabilities+: ["solaris"],
        name+: "-solaris",
        environment+: {
            CI_OS: "solaris"
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
    SPARCv9:: {
        capabilities+: ["sparcv9"],
        name+: "-sparcv9",
        timelimit: "1:30:00",
        environment+: {
            CI_ARCH: "sparcv9"
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
        name+: "-openjdk",
        downloads: {
            JAVA_HOME: {
                name : "openjdk",
                version : "8u252+09",
                platformspecific: true
            },
            JAVA_HOME_OVERLAY: {
                name : "openjdk-overlay",
                version : "8u252+09",
                platformspecific: true
            }
        }
    },

    # Downstream Graal branch to test against. If not master, then
    # the branch must exist on both graal and graal-enterprise to
    # ensure a consistent downstream code base is tested against.
    local downstream_branch = "master",

    Build:: {
        packages+: {
            "00:pip:logilab-common": "==1.4.4",
            "01:pip:astroid" : "==1.1.0",
            "pip:pylint" : "==1.1.0",
        },
        environment+: {
            MX_PYTHON: "python3",
        },
        name: "gate-jvmci",
        timelimit: "1:00:00",
        diskspace_required: "10G",
        logs: ["*.log", "*.cmd"],
        targets: ["gate"],
        run+: [
            # To reduce load, the CI system does not fetch all tags so it must
            # be done explicitly as `mx jvmci-version` relies on it.
            # See GR-22662.
            ["git", "fetch", "origin", "--tags"],

            # Clone graal for testing
            ["git", "--version"],
            ["git", "clone", ["mx", "urlrewrite", "https://github.com/graalvm/graal.git"]],
            ["git", "-C", "graal", "checkout", downstream_branch, "||", "true"],

            ["mx", "--kill-with-sigquit", "--strict-compliance", "gate", "--dry-run"],
            ["mx", "--kill-with-sigquit", "--strict-compliance", "gate"],
            ["mv", ["mx", "--vm=server", "jdkhome"], "java_home"],
            ["set-export", "JAVA_HOME", "${PWD}/java_home"],
            ["${JAVA_HOME}/bin/java", "-version"],

            # Free up disk space for space tight CI slaves
            ["rm", "-rf", "build", "mxbuild", "*jdk1.8.0*"],

            # Overlay static libraries
            ["set-export", "OLD_PWD", "${PWD}"],
            ["cd", "${JAVA_HOME_OVERLAY}"],
            ["cp", "-r", ".", "${JAVA_HOME}"],
            ["cd", "${OLD_PWD}"],
        ],
    },

    GraalTest:: {
        name+: "-graal",
        run+: [
            ["mx", "-v", "-p", "graal/compiler", "gate", "--tags", "build,test,bootstraplite"]
        ]
    },

    # Build a basic GraalVM and run some simple tests.
    GraalVMTest:: {
        name+: "-graalvm",
        timelimit: "1:30:00",
        run+: [
            # Build and test JavaScript on GraalVM
            ["mx", "-p", "graal/vm", "--dynamicimports", "/graal-js,/substratevm", "--disable-polyglot", "--disable-libpolyglot", "--force-bash-launchers=native-image", "build"],
            ["./graal/vm/latest_graalvm_home/bin/js",          "mx.jvmci/test.js"],
            ["./graal/vm/latest_graalvm_home/bin/js", "--jvm", "mx.jvmci/test.js"],

             # Build and test LibGraal
            ["mx", "-p", "graal/vm", "--env", "libgraal", "--extra-image-builder-argument=-J-esa", "--extra-image-builder-argument=-H:+ReportExceptionStackTraces", "build"],
            ["mx", "-p", "graal/vm", "--env", "libgraal", "gate", "--task", "LibGraal"],
        ]
    },

    builds: [
        self.Build + self.GraalTest + mach
        for mach in [
            # Only need to test formatting and building
            # with Eclipse on one platform.
            self.GraalVMTest + self.Linux + self.AMD64 + self.OpenJDK + self.Eclipse + self.JDT,
            self.GraalVMTest + self.Darwin + self.AMD64 + self.OpenJDK,
            # GraalVM not (yet) supported on these platforms
            self.Windows + self.AMD64 + self.OpenJDK,
        ]
    ]
}
