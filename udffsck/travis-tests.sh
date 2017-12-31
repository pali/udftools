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

if [ "$1" == 'extra1' ]; then  
    cd ..
    wget --no-check-certificate https://pac.chapadla.cz/~rain/udffsck-test-samples/udf-samples-extra.tar.xz 
    tar -xJvf udf-samples-extra.tar.xz udf-samples-extra/bs512_windows7_udf0201.img udf-samples-extra/bs512_windows7_udf0201_broken_file_tree.img udf-samples-extra/bs512_windows7_udf0201_chkdsk.img  
    cd udftools/udffsck
    ./testextra1
    cd ../..
    rm udf-samples-extra/bs512_windows7_udf0201.img udf-samples-extra/bs512_windows7_udf0201_broken_file_tree.img udf-samples-extra/bs512_windows7_udf0201_chkdsk.img  
    rm udf-samples-extra.tar.xz
    cd udftools/udffsck
fi

if [ "$1" == 'extra2' ]; then  
    cd ..
    wget --no-check-certificate https://pac.chapadla.cz/~rain/udffsck-test-samples/udf-samples-extra.tar.xz 
    tar -xJvf udf-samples-extra.tar.xz udf-samples-extra/bs512_windows7_udf0201-linux-before-fix.img udf-samples-extra/bs512_windows7_udf0201-serial-broken-linux-written.img udf-samples-extra/bs512_windows7_udf0201-serial-broken-linux-written-afterfix-win-write.img  
    cd udftools/udffsck
    ./testextra2
    cd ../..
    rm udf-samples-extra/bs512_windows7_udf0201-linux-before-fix.img udf-samples-extra/bs512_windows7_udf0201-serial-broken-linux-written.img udf-samples-extra/bs512_windows7_udf0201-serial-broken-linux-written-afterfix-win-write.img  
    rm udf-samples-extra.tar.xz
    cd udftools/udffsck
fi

if [ "$1" == 'unit' ]; then
    cd udffsck
    ./unittest
fi
