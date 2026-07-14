# Third-Party Notices

## Stable Cosserat Rods

This project implements the method described in:

Jerry Hsu, Tongtong Wang, Kui Wu, and Cem Yuksel. *Stable Cosserat Rods*.
SIGGRAPH Conference Papers 2025. DOI: 10.1145/3721238.3730618.

Project page: <https://graphics.cs.utah.edu/research/projects/stable-cosserat-rods/>

The authors' CPU reference implementation is available under the MIT License:
<https://github.com/jerry060599/StableCosseratRods>.

The native implementation in this repository is an independent implementation
of the published equations. If source is copied from the reference repository
later, its MIT copyright and permission notice must be added here and retained
with the copied files.

The authors' YarnBall GPU implementation is GPL-licensed and is not copied into
the first CPU milestone.

## Microsoft Visual C++ OpenMP Runtime

The Windows x64 package includes `bin/vcomp140.dll`, the Microsoft Visual C++
OpenMP runtime used by the native solver. It is copied unmodified from the
Visual Studio 2022 redistributable directory and is distributed under the
Microsoft Visual Studio license terms. It is not covered by this project's GPL
license.

Microsoft deployment and licensing guidance:
<https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files>

Microsoft OpenMP runtime reference:
<https://learn.microsoft.com/en-us/cpp/parallel/openmp/reference/openmp-library-reference>
