#!/bin/bash
set -e

cd ..
wget --no-check-certificate https://pac.chapadla.cz/~rain/udffsck-test-samples/udf-samples.tar.xz 
bash decompress-samples.sh
cd udftools/udffsck
./test
cd ../..
rm udf-samples.tar.xz udf-samples -r
wget --no-check-certificate https://pac.chapadla.cz/~rain/udffsck-test-samples/udf-samples-extra.tar.xz 
bash decompress-samples-extra.sh
cd udftools/udffsck
./testextra
