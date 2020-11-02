#!/bin/bash
cmake --version
FACTOR=$1
set -ex
cd Polyhedron/demo
LIST_OF_PLUGINS=$(cmake --build . -t help | egrep 'plugin$' |& cut -d\  -f2)
PLUGINS_ARRAY=(${LIST_OF_PLUGINS});
NB_OF_PLUGINS=${#PLUGINS_ARRAY[@]}
DEL=$(($NB_OF_PLUGINS / 4))
mkdir build
cd build
cmake -DCGAL_DIR=$2 ../Polyhedron
make -j2 ${PLUGINS_ARRAY[@]:$(($FACTOR * $DEL)):$((($FACTOR + 1) * $DEL))}
