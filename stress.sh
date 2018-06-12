#!/bin/bash
set -x

# Get the latest OpenJDK8
sudo add-apt-repository ppa:openjdk-r/ppa -y
sudo apt-get -qq update
sudo apt-cache search openjdk
sudo apt-get install -y openjdk-8-jdk openjdk-8-source openjdk-8-doc

which java
java -version

# Get mx
git clone https://github.com/graalvm/mx.git
export PATH=$PATH:`pwd`/mx

# Build the VM, install and test it
mx build -DFULL_DEBUG_SYMBOLS=0
javac -target 1.8 -source 1.8 Stress.java
i=2000
while [ $? -eq 0 -a $i -ge 0 ]; do
    echo "------ $i ------"
    date
    i=$(( $i - 1 ))
    mx vm -esa -Djvmci.class.path.append=graal.jar:graal-management.jar -Xbootclasspath/a:graal-sdk.jar:truffle-api.jar -Djvmci.PrintConfig=true -XX:+UseJVMCICompiler -XX:+BootstrapJVMCI -version >print_config.log
done
