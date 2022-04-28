========================================
Clang 13.0.0 Release Notes
========================================

.. contents::
   :local:
   :depth: 2

Written by the `LLVM Team <https://llvm.org/>`_

Introduction
============

This document contains the release notes for the Clang C/C++/Objective-C
frontend, part of the LLVM Compiler Infrastructure, release 14.0.0. Here we
describe the status of Clang in some detail, including major
improvements from the previous release and new feature work. For the
general LLVM release notes, see `the LLVM
documentation <https://llvm.org/docs/ReleaseNotes.html>`_. All LLVM
releases may be downloaded from the `LLVM releases web
site <https://llvm.org/releases/>`_.

For more information about Clang or LLVM, including information about the
latest release, please see the `Clang Web Site <https://clang.llvm.org>`_ or the
`LLVM Web Site <https://llvm.org>`_.

Note that if you are reading this file from a Git checkout or the
main Clang web page, this document applies to the *next* release, not
the current one. To see the release notes for a specific release, please
see the `releases page <https://llvm.org/releases/>`_.

What's New in Clang 14.0.0?
===========================

Some of the major new features and improvements to Clang are listed
here. Generic improvements to Clang as a whole or to its underlying
infrastructure are described first, followed by language-specific
sections with improvements to Clang's support for those languages.

Major New Features
------------------

-  ...

Improvements to Clang's diagnostics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ...

Non-comprehensive list of changes in this release
-------------------------------------------------

- The default value of _MSC_VER was raised from 1911 to 1914. MSVC 19.14 has the
  support to overaligned objects on x86_32 which is required for some LLVM
  passes.

New Compiler Flags
------------------

- ``-Wreserved-identifier`` emits warning when user code uses reserved
  identifiers.

- ``Wunused-but-set-parameter`` and ``-Wunused-but-set-variable`` emit warnings
  when a parameter or a variable is set but not used.

- ``-fstack-usage`` generates an extra .su file per input source file. The .su
  file contains frame size information for each function defined in the source
  file.

- ``-Wnull-pointer-subtraction`` emits warning when user code may have
  undefined behaviour due to subtraction involving a null pointer.

Deprecated Compiler Flags
-------------------------

- ...

Modified Compiler Flags
-----------------------

- ...

Removed Compiler Flags
-------------------------

- ...

New Pragmas in Clang
--------------------

- ...

Attribute Changes in Clang
--------------------------

- ...

Windows Support
---------------

- Fixed reading ``long double`` arguments with ``va_arg`` on x86_64 MinGW
  targets.

C Language Changes in Clang
---------------------------

- ...

C++ Language Changes in Clang
-----------------------------

- ...

C++20 Feature Support
^^^^^^^^^^^^^^^^^^^^^
...

C++2b Feature Support
^^^^^^^^^^^^^^^^^^^^^
...

Objective-C Language Changes in Clang
-------------------------------------

OpenCL Kernel Language Changes in Clang
---------------------------------------


Command-line interface changes:

- All builtin types, macros and function declarations are now added by default
  without any command-line flags. A flag is provided ``-cl-no-stdinc`` to
  suppress the default declarations non-native to the compiler.

- Clang now compiles using OpenCL C version 1.2 by default if no version is
  specified explicitly from the command line.

- Clang now supports ``.clcpp`` file extension for sources written in
  C++ for OpenCL.

- Clang now accepts ``-cl-std=clc++1.0`` that sets C++ for OpenCL to
  the version 1.0 explicitly.

Misc common changes:

- Added ``NULL`` definition in internal headers for standards prior to the
  version 2.0.

- Simplified use of pragma in extensions for ``double``, images, atomics,
  subgroups, Arm dot product extension. There are less cases where extension
  pragma is now required by clang to compile kernel sources.

- Added missing ``as_size``/``as_ptrdiff``/``as_intptr``/``as_uintptr_t``
  operators to internal headers.

- Added new builtin function for ndrange, ``cl_khr_subgroup_extended_types``,
  ``cl_khr_subgroup_non_uniform_vote``, ``cl_khr_subgroup_ballot``,
  ``cl_khr_subgroup_non_uniform_arithmetic``, ``cl_khr_subgroup_shuffle``,
  ``cl_khr_subgroup_shuffle_relative``, ``cl_khr_subgroup_clustered_reduce``
  into the default Tablegen-based header.

- Added online documentation for Tablegen-based header, OpenCL 3.0 support,
  new clang extensions.

- Fixed OpenCL C language version and SPIR address space reporting in DWARF.

New extensions:

- ``cl_khr_integer_dot_product`` for dedicated support of dot product.

- ``cl_khr_extended_bit_ops`` for dedicated support of extra binary operations.

- ``__cl_clang_bitfields`` for use of bit-fields in the kernel code.

- ``__cl_clang_non_portable_kernel_param_types`` for relaxing some restrictions
  to types of kernel parameters.

OpenCL C 3.0 related changes:

- Added parsing support for the optionality of generic address space, images 
  (including 3d writes and ``read_write`` access qualifier), pipes, program
  scope variables, double-precision floating-point support. 

- Added optionality support for builtin functions (in ``opencl-c.h`` header)
  for generic address space, C11 atomics.  

- Added ``memory_scope_all_devices`` enum for the atomics in internal headers.

- Enabled use of ``.rgba`` vector components.

C++ for OpenCL related changes:

- Added ``__remove_address_space`` metaprogramming utility in internal headers
  to allow removing address spaces from types.

- Improved overloads resolution logic for constructors wrt address spaces.

- Improved diagnostics of OpenCL specific types and address space qualified
  types in ``reinterpret_cast`` and template functions.

- Fixed ``NULL`` macro in internal headers to be compatible with C++.

- Fixed use of ``half`` type.

ABI Changes in Clang
--------------------

OpenMP Support in Clang
-----------------------

- Support for loop transformation directives from OpenMP 5.1 have been added.
  ``#pragma omp unroll`` is a standardized alternative to ``#pragma unroll``
  (or ``#pragma clang loop unroll(enable)``) but also allows composition with
  other OpenMP loop associated constructs as in

  .. code-block:: c

    #pragma omp parallel for
    #pragma omp unroll partial(4)
    for (int i = 0; i < n; ++i)

  ``#pragma omp tile`` applies tiling to a perfect loop nest using a
  user-defined tile size.

  .. code-block:: c

    #pragma omp tile sizes(8,8)
    for (int i = 0; i < m; ++i)
      for (int j = 0; j < n; ++j)

- ...

CUDA Support in Clang
---------------------

- ...

X86 Support in Clang
--------------------

- ...

Internal API Changes
--------------------

- ...

Build System Changes
--------------------

- ...

AST Matchers
------------

- ...

clang-format
------------

- Option ``AllowShortEnumsOnASingleLine: false`` has been improved, it now
  correctly places the opening brace according to ``BraceWrapping.AfterEnum``.

libclang
--------

- Make libclang SONAME independent from LLVM version. It will be updated only when
  needed. Defined in CLANG_SONAME (clang/tools/libclang/CMakeLists.txt).
  `More details <https://lists.llvm.org/pipermail/cfe-dev/2021-June/068423.html>`_

Static Analyzer
---------------

.. 2407eb08a574 [analyzer] Update static analyzer to be support sarif-html

- Add a new analyzer output type, ``sarif-html``, that outputs both HTML and
  Sarif files.

.. 90377308de6c [analyzer] Support allocClassWithName in OSObjectCStyleCast checker

- Add support for ``allocClassWithName`` in OSObjectCStyleCast checker.

.. cad9b7f708e2b2d19d7890494980c5e427d6d4ea: Print time taken to analyze each function

- The option ``-analyzer-display-progress`` now also outputs analysis time for
  each function.

.. 9e02f58780ab8734e5d27a0138bd477d18ae64a1 [analyzer] Highlight arrows for currently selected event

- For bug reports in HTML format, arrows are now highlighted for the currently
  selected event.

.. Deep Majumder's GSoC'21
.. 80068ca6232b [analyzer] Fix for faulty namespace test in SmartPtrModelling
.. d825309352b4 [analyzer] Handle std::make_unique
.. 0cd98bef1b6f [analyzer] Handle std::swap for std::unique_ptr
.. 13fe78212fe7 [analyzer] Handle << operator for std::unique_ptr
.. 48688257c52d [analyzer] Model comparision methods of std::unique_ptr
.. f8d3f47e1fd0 [analyzer] Updated comments to reflect D85817
.. 21daada95079 [analyzer] Fix static_cast on pointer-to-member handling

- While still in alpha, ``alpha.cplusplus.SmartPtr`` received numerous
  improvements and nears production quality.

.. 21daada95079 [analyzer] Fix static_cast on pointer-to-member handling
.. 170c67d5b8cc [analyzer] Use the MacroExpansionContext for macro expansions in plists
.. 02b51e5316cd [analyzer][solver] Redesign constraint ranges data structure
.. 3085bda2b348 [analyzer][solver] Fix infeasible constraints (PR49642)
.. 015c39882ebc [Analyzer] Infer 0 value when the divisible is 0 (bug fix)
.. 90377308de6c [analyzer] Support allocClassWithName in OSObjectCStyleCast checker
.. df64f471d1e2 [analyzer] DynamicSize: Store the dynamic size
.. e273918038a7 [analyzer] Track leaking object through stores
.. 61ae2db2d7a9 [analyzer] Adjust the reported variable name in retain count checker
.. 50f17e9d3139 [analyzer] RetainCountChecker: Disable reference counting for OSMetaClass.

- Various fixes and improvements, including modeling of casts (such as 
  ``std::bit_cast<>``), constraint solving, explaining bug-causing variable
  values, macro expansion notes, modeling the size of dynamic objects and the
  modeling and reporting of Objective C/C++ retain count related bugs. These
  should reduce false positives and make the remaining reports more readable.

.. _release-notes-ubsan:

Undefined Behavior Sanitizer (UBSan)
------------------------------------

Core Analysis Improvements
==========================

- ...

New Issues Found
================

- ...

Python Binding Changes
----------------------

The following methods have been added:

-  ...

Significant Known Problems
==========================

Additional Information
======================

A wide variety of additional information is available on the `Clang web
page <https://clang.llvm.org/>`_. The web page contains versions of the
API documentation which are up-to-date with the Git version of
the source code. You can access versions of these documents specific to
this release by going into the "``clang/docs/``" directory in the Clang
tree.

If you have any questions or comments about Clang, please feel free to
contact us via the `mailing
list <https://lists.llvm.org/mailman/listinfo/cfe-dev>`_.
