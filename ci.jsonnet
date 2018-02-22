{
    Linux:: {
        packages+: {
            git: ">=1.8.3",
            mercurial: ">=2.2",
            make : ">=3.83",
            "gcc-build-essentials" : "==4.9.1"
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
            MACOSX_DEPLOYMENT_TARGET: "10.11"
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

    Build:: {
        packages+: {
            "pip:astroid" : "==1.1.0",
            "pip:pylint" : "==1.1.0",
        },
        name: "gate",
        timelimit: "1:00:00",
        logs: ["*.log"],
        targets: ["gate"],
        downloads: {
            JAVA_HOME: {
                name : "oraclejdk",
                version : "8u161",
                platformspecific: true
            }
        },
        run: [
            ["mx", "-v", "--kill-with-sigquit", "--strict-compliance", "gate"],

            # Test on graal
            ["git", "clone", ["mx", "urlrewrite", "https://github.com/graalvm/graal.git"]],

            # Look for a well known branch that fixes a downstream failure caused by a JDK change
            ["git", "-C", "graal", "checkout", "master", "||", "true"],

            ["mx", "-v", "-p", "graal/compiler",
                    "--java-home", ["mx", "--vm=server", "jdkhome"],
                    "gate", "--tags", "build,ctw,test"
            ]
        ],
    },

    builds: [
        self.Build + mach
        for mach in [
            # Only need to test formatting and building
            # with Eclipse on one platform.
            self.Linux + self.AMD64 + self.Eclipse + self.JDT,
            self.Darwin + self.AMD64,
            self.Solaris + self.SPARCv9,
        ]
    ]
}
