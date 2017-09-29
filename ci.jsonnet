{
    Linux:: {
        packages+: {
            git : ">=1.8.3",
            mercurial : ">=2.2",
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
            # Brew does not support versions
            git : "",
            mercurial : "",
            # No need to specify a "make" package as Mac OS X has make 3.81
            # available once Xcode has been installed.
        },
        environment+: {
            CI_OS: "darwin"
        },
        capabilities+: ["darwin"],
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

    Build:: {
        name: "gate",
        timelimit: "30:00",
        logs: ["*.log"],
        targets: ["gate"],
        downloads: {
            JAVA_HOME: {
                name : "oraclejdk",
                version : "8u141",
                platformspecific: true
            },
            JDT: {
                name: "ecj",
                version: "4.5.1",
                platformspecific: false
            },
        },
        run: [
            ["mx", "-v", "--kill-with-sigquit", "--strict-compliance", "gate"]
        ],
    },

    builds: [
        self.Build + mach
        for mach in [
            self.Linux + self.AMD64 + self.Eclipse,
            self.Darwin + self.AMD64,
            self.Solaris + self.SPARCv9,
        ]
    ]
}
