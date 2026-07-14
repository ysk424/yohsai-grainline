# SPDX-License-Identifier: GPL-3.0-or-later

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root "build"

cmake -S $Root -B $Build -G "Visual Studio 17 2022" -A x64
cmake --build $Build --config $Configuration --parallel
ctest --test-dir $Build -C $Configuration --output-on-failure
cmake --install $Build --config $Configuration --prefix $Root
