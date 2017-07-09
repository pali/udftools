#!/bin/bash
set -e

if [ "$1" == 'basic' ]; then  
    cd ..
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/raw/master/decompress-samples.sh
    wget --no-check-certificate https://pac.chapadla.cz/~rain/udffsck-test-samples/udf-samples.tar.xz 
    bash decompress-samples.sh
    cd udftools/udffsck
    ./test
    cd ../..
    rm udf-samples.tar.xz udf-samples -r
    cd udftools/udffsck
fi

if [ "$1" == 'extra' ]; then  
    cd ..
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/raw/master/decompress-samples-extra.sh
    wget --no-check-certificate https://pac.chapadla.cz/~rain/udffsck-test-samples/udf-samples-extra.tar.xz 
    bash decompress-samples-extra.sh
    cd udftools/udffsck
    ./testextra
    cd ../..
    rm udf-samples-extra.tar.xz udf-samples-extra -r
    cd udftools/udffsck
fi
