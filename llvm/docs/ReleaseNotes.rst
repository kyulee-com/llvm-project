=========================
LLVM 14.0.0 Release Notes
=========================

.. contents::
    :local:


Introduction
============

This document contains the release notes for the LLVM Compiler Infrastructure,
release 14.0.0.  Here we describe the status of LLVM, including major improvements
from the previous release, improvements in various subprojects of LLVM, and
some of the current users of the code.  All LLVM releases may be downloaded
from the `LLVM releases web site <https://llvm.org/releases/>`_.

For more information about LLVM, including information about the latest
release, please check out the `main LLVM web site <https://llvm.org/>`_.  If you
have questions or comments, the `LLVM Developer's Mailing List
<https://lists.llvm.org/mailman/listinfo/llvm-dev>`_ is a good place to send
them.

Note that if you are reading this file from a Git checkout or the main
LLVM web page, this document applies to the *next* release, not the current
one.  To see the release notes for a specific release, please see the `releases
page <https://llvm.org/releases/>`_.

Non-comprehensive list of changes in this release
=================================================
.. NOTE
   For small 1-3 sentence descriptions, just add an entry at the end of
   this list. If your description won't fit comfortably in one bullet
   point (e.g. maybe you would like to give an example of the
   functionality, or simply have a lot to talk about), see the `NOTE` below
   for adding a new subsection.


.. NOTE
   If you would like to document a larger change, then you can add a
   subsection about it right here. You can copy the following boilerplate
   and un-indent it (the indentation causes it to be inside this comment).

   Special New Feature
   -------------------

   Makes programs 10x faster by doing Special New Thing.

* ...

* Flang is now included in the binary packages released by LLVM.

* The debuginfo-test project has been renamed cross-project-tests and is now
  intended for testing components from multiple projects, not just debug
  information. The new "cross-project-tests" name replaces "debuginfo-test" in
  LLVM_ENABLE_PROJECTS, and a new check-cross-project-tests target has been
  added for running all tests in the project. The pre-existing check-debuginfo-
  test target remains for running just the debug information tests.
  (`D95339 <https://reviews.llvm.org/D95339>`_ and
  `D96513 <https://reviews.llvm.org/D96513>`_)

Changes to the LLVM IR
----------------------

* ...

* Using the legacy pass manager for the optimization pipeline is deprecated and
  will be removed after LLVM 14. In the meantime, only minimal effort will be
  made to maintain the legacy pass manager for the optimization pipeline.

Changes to building LLVM
------------------------

* ...

Changes to TableGen
-------------------

Changes to the AArch64 Backend
------------------------------

* Introduced assembly support for Armv9-A's Realm Management Extension (RME)
  and Scalable Matrix Extension (SME).

* Produce proper cross-section relative relocations on COFF

* Fixed the calling convention on Windows for variadic functions involving
  floats in the fixed arguments

Changes to the ARM Backend
--------------------------

* Produce proper cross-section relative relocations on COFF

Changes to the MIPS Target
--------------------------

During this release ...

Changes to the Hexagon Target
-----------------------------

* ...

Changes to the PowerPC Target
-----------------------------

During this release ...

Changes to the X86 Target
-------------------------

During this release ...

Changes to the AMDGPU Target
-----------------------------

During this release ...

Changes to the AVR Target
-----------------------------

During this release ...

Changes to the WebAssembly Target
---------------------------------

During this release ...

Changes to the OCaml bindings
-----------------------------


Changes to the C API
--------------------

* ...

Changes to the Go bindings
--------------------------


Changes to the FastISel infrastructure
--------------------------------------

* ...

Changes to the DAG infrastructure
---------------------------------


Changes to the Debug Info
---------------------------------

During this release ...

Changes to the LLVM tools
---------------------------------

* ...

* llvm-rc got support for invoking Clang to preprocess its input.
  (`D100755 <https://reviews.llvm.org/D100755>`_)

* llvm-rc got a GNU windres compatible frontend, llvm-windres.
  (`D100756 <https://reviews.llvm.org/D100756>`_)

* llvm-ml has improved compatibility with MS ml.exe, managing to assemble
  more asm files.

Changes to LLDB
---------------------------------

* LLDB executable is now included in pre-built LLVM binaries.

* LLDB now includes full featured support for AArch64 SVE register access.

* LLDB now supports AArch64 Pointer Authentication, allowing stack unwind with signed return address.

* LLDB now supports debugging programs on AArch64 Linux that use memory tagging (MTE).
* Added ``memory tag read`` and ``memory tag write`` commands.
* The ``memory region`` command will note when a region has memory tagging enabled.
* Synchronous and asynchronous tag faults are recognised.
* Synchronous tag faults have memory tag annotations in addition to the usual fault address.

Changes to Sanitizers
---------------------

External Open Source Projects Using LLVM 14
===========================================

* A project...

Additional Information
======================

A wide variety of additional information is available on the `LLVM web page
<https://llvm.org/>`_, in particular in the `documentation
<https://llvm.org/docs/>`_ section.  The web page also contains versions of the
API documentation which is up-to-date with the Git version of the source
code.  You can access versions of these documents specific to this release by
going into the ``llvm/docs/`` directory in the LLVM tree.

If you have any questions or comments about LLVM, please feel free to contact
us via the `mailing lists <https://llvm.org/docs/#mailing-lists>`_.
