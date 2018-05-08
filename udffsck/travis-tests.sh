#!/bin/bash
set -e

if [ "$1" == 'basic' ]; then  
    cd ..
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/raw/master/decompress-samples.sh
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/releases/download/"$2"/udf-samples.tar.xz
    bash decompress-samples.sh
    cd udftools/udffsck
    $RUN ./test
    cd ../..
    rm udf-samples.tar.xz udf-samples -r
    cd udftools/udffsck
fi

if [ "$1" == 'extra1' ]; then  
    cd ..
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/releases/download/"$2"/udf-samples-extra-1.tar.xz
    tar -xJvf udf-samples-extra-1.tar.xz  
    cd udftools/udffsck
    $RUN ./testextra1
    cd ../..
    rm  -r udf-samples-extra-1 udf-samples-extra-1.tar.xz
    cd udftools/udffsck
fi

if [ "$1" == 'extra2' ]; then  
    cd ..
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/releases/download/"$2"/udf-samples-extra-2.tar.xz
    tar -xJvf udf-samples-extra-2.tar.xz  
    cd udftools/udffsck
    $RUN ./testextra2
    cd ../..  
    rm -r udf-samples-extra-2 udf-samples-extra-2.tar.xz
    cd udftools/udffsck
fi

if [ "$1" == 'extra3' ]; then  
    cd ..
    wget --no-check-certificate https://github.com/argorain/udffsck-test-samples/releases/download/"$2"/udf-samples-extra-3.tar.xz
    tar -xJvf udf-samples-extra-3.tar.xz 
    cd udftools/udffsck
    $RUN ./testextra3
    cd ../..
    rm -r udf-samples-extra-3 udf-samples-extra-3.tar.xz
    cd udftools/udffsck
fi

if [ "$1" == 'unit' ]; then
    cd udffsck
    $RUN ./unittest
fi
