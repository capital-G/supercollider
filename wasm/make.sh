#!/bin/bash
make scsynth \
    -s STANDALONE_WASM=1
    # -s USE_PTHREADS=1 \
    # -s WASM=1
    # -s EXPORTED_RUNTIME_METHODS=_malloc


# emcmake make scsynth \
#     -s USE_PTHREADS=0 \
#     -s WASM=1 \
#     -s EXPORTED_FUNCTIONS=_malloc \
#     -s EXPORTED_RUNTIME_METHODS=_malloc

cp server/scsynth/*.js ../wasm/
cp server/scsynth/*.wasm ../wasm/


# CMake Error: Unknown argument --target


# emcmake cmake -DSC_EL=no -DSUPERNOVA=no ... -s USE_PTHREADS= -s WASM=1 ..
