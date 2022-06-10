# Testing Function Merging on OSX with -Os and -Oz

This is based on Rodrigo Rocha's work and one of his implementation,
in https://github.com/rcorcs/llvm-project/blob/func-merging-dot.
I've built Clang on OSX, and use this compiler as the bootstrap tool,
to build another LLVM components (e.g., `opt`, and `libLTO.dylib`) with LTO.
This `func-merging` pass is not wired with any default pass manager, 
so I manually applied to run `opt` and `llc` to the merged bitcode,
and compared the text section size of the resulting object file.

## Result (arm64)

The results with `-Os` showed the size reduction (negative value) with
the function coverage, which was expected.

| -Os| Baseline | FunctionMerge  | % |
| :-----: | :-: | :-: |  :-: |
| opt | 22676580 | 22089828 | -2.58% |
| libLTO.dylib | 20205336 | 19882476 | -1.60% |

However, the results with `-Oz` (minsize opt) seemed the size regression
(positive value). These trends are not much different with `x86_64`.

| -Oz| Baseline | FunctionMerge  | % |
| :-----: | :-: | :-: |  :-: |
| opt | 15427104 | 15684144 | 1.67% |
| libLTO.dylib | 13618004 | 14057512 | 3.23% |

## Methodology (Repro Step)

Basically there are only a few steps to build this same branch with two different build directories.
The first step is probably once, and you may repeat the remaining steps with different configurations.

1. Build the bootstrap compiler. 

   * ``mkdir -p $HOME/build-bootstrap && pushd $HOME/build-bootstrap``
   *  
    ```
       cmake -G Ninja \
       -DLLVM_ENABLE_DUMP=ON \
       -DCMAKE_BUILD_TYPE=RelWithDebInfo \
       -DLLVM_ENABLE_PROJECTS='clang;compiler-rt' \
       -DLLVM_TARGETS_TO_BUILD='X86;AArch64' \
       $HOME/llvm-project/llvm
    ```
    * ``ninja``
 
2. Build the cross-component that you want to test. Example with `arm64`, `-Os` for `libLTO.dylib`.
  
     * ``mkdir -p $HOME/build-arm64-Os && pushd $HOME/build-arm64-Os``
     * 
      ```
         cmake -G Ninja  \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CROSSCOMPILING=True \
         -DCMAKE_LIBTOOL="$HOME/build-bootstrap/bin/llvm-libtool-darwin" \
         -DCMAKE_C_COMPILER="$HOME/build-bootstrap/bin/clang" \
         -DCMAKE_CXX_COMPILER="$HOME/build-bootstrap/bin/clang++" \
         -DLLVM_DEFAULT_TARGET_TRIPLE=arm64-apple-macos11 \
         -DLLVM_TABLEGEN=$HOME/build-bootstrap/bin/llvm-tblgen \
         -DLLVM_TARGET_ARCH='AArch64' \
         -DLLVM_TARGETS_TO_BUILD='AArch64' \
         -DCMAKE_CXX_FLAGS_RELEASE='-Os' \
         -DCMAKE_C_FLAGS_RELEASE='-Os' \
         -DCMAKE_CXX_FLAGS='-target arm64-apple-macos11 -flto -Wl,-save-temps -Wno-unused-command-line-argument' \
         -DCMAKE_C_FLAGS='-target arm64-apple-macos11 -flto -Wl,-save-temps -Wno-unused-command-line-argument' \
         $HOME/llvm-project/llvm
      ```
      * ``ninja libLTO.dylib``
   
3. From the above 2, you will have intermediate results as the baseline (without function merging).
   They are ``lib/libLTO.dylib.lto.bc`` (prestine bitcode just merged),
   ``lib/libLTO.dylib.lto.opt.bc`` (optimized bitcode),
   and ``lib/libLTO.dylib.o`` (resulting object file).
   To get the similar output with function merging:
      * `` $HOME/build-bootstrap/bin/opt -func-merging lib/libLTO.dylib.lto.bc -o lib/libLTO.dylib.lto.funcmerging.bc``
      * `` $HOME/build-bootstrap/bin/opt -Os lib/libLTO.dylib.lto.funcmerging.bc -o lib/libLTO.dylib.lto.funcmerging.Os.bc``
      * `` $HOME/build-bootstrap/bin/llc -filetype=obj lib/libLTO.dylib.lto.funcmerging.Os.bc -o lib/libLTO.dylib.lto.funcmerging.Os.o``

4. Compare the text section size.

      * ``size -m lib/libLTO.dylib.o | grep text `` for baseline
      * ``size -m lib/libLTO.dylib.lto.funcmerging.Os.o | grep text `` for function merging
