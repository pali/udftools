#!/bin/bash

rm trace1a.log trace1b.log
rm trace1a.err trace1b.err
#!/usr/bin/env bash

unpack=

while getopts ":n" opt; do
    case $opt in 
        n) unpack="1" ;;
        ?) exit ;; 
    esac
done

if [ ! -z "$unpack" ]; then
    cd ../../udffsck-test-samples
    tar -xJvf udf-samples.tar.xz udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity.img
    cp udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity.img udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity-a.img
    mv udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity.img udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity-b.img
    cd ../udftools
else
    cd ..
fi

make clean
make CFLAGS=""
cd udffsck
./udffsck -vvvp ../../udffsck-test-samples/udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity-b.img > >(tee -a trace1b.log) 2> >(tee -a trace1b.err >&2)
#cd ../../udffsck-test-samples
#./decompress-samples.sh
#tar -xJvf udf-samples.tar.xz udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity.img
#cd ../udftools
cd ..
make clean
make CFLAGS="-m32"
cd udffsck
./udffsck -vvvp ../../udffsck-test-samples/udf-samples/bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity-a.img > >(tee -a trace1a.log) 2> >(tee -a trace1a.err >&2)
meld trace1a.log trace1b.log
meld trace1a.err trace1b.err
