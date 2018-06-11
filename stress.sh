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
i=200
while [ $? -eq 0 -a $i -ge 0 ]; do
    echo "------ $i ------"
    i=$(( $i - 1 ))
    mx -v vm -Djvmci.PrintConfig=true -XX:+EagerJVMCI -version
done
