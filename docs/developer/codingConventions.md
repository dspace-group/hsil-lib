# Coding Conventions

## SPDX identifiers

Every first-party source or build file starts with these two comment lines,
using the comment syntax for its file type:

```c
// SPDX-FileCopyrightText: <Your Copyright>
// SPDX-License-Identifier: Apache-2.0
```

For Python and CMake files, use `#` instead of `//`. Preserve a Python
script's shebang as line 1 and put the SPDX comments immediately after it.
Keep CMake's `cmake_minimum_required()` as the first command, after the SPDX
comments.

## C and C++ file headers

Keep the SPDX block separate from the Doxygen description. The Doxygen block
should describe the file and must not contain duplicate copyright text or an
`@copyright` tag:

```cpp
// SPDX-FileCopyrightText: <Your Copyright>
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file <filename>
*
*   @brief <short description>
*
*   @author
*       <author name(s)>
*
*   @description
*       <what the file does>
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/
```

Apply this format to C and C++ implementation files, headers, and tests. IDL
files use the same SPDX lines with `//` comments and retain their interface
description below them.

## Scope

These rules apply to first-party files. Do not modify license headers in
`third_party/`. Keep generated files under build directories out of source
control and do not add SPDX text to generated output.
