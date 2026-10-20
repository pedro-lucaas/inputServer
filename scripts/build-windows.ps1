# Builds InputServer on Windows: configure, compile, test, install and
# (optionally) package the Inno Setup installer.
#
# Usage, from any PowerShell window in the repository root:
#   .\scripts\build-windows.ps1                 # Release build + tests + install
#   .\scripts\build-windows.ps1 -Installer      # ...and InputServerSetup-<versão>.exe
#   .\scripts\build-windows.ps1 -SkipTests
#
# Requirements (see README): Visual Studio 2022 Build Tools (C++), CMake,
# Ninja, Qt 6 for MSVC 64-bit, OpenSSL (ShiningLight) and, for -Installer,
# Inno Setup 6.

param(
    [string]$QtDir = $env:INPUTSERVER_QT_DIR,
    [string]$OpenSslDir = "C:\Program Files\OpenSSL-Win64",
    [string]$BuildDir = "build",
    [string]$InstallDir = "dist-inputserver",
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",
    [switch]$SkipTests,
    [switch]$Installer
)

# Native tools write progress to stderr; failures are detected via $LASTEXITCODE.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail($message) {
    Write-Host "ERRO: $message" -ForegroundColor Red
    exit 1
}

# --- Locate Qt ---------------------------------------------------------------
if (-not $QtDir) {
    $QtDir = Get-ChildItem "C:\Qt\6.*\msvc*_64" -Directory -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $QtDir -or -not (Test-Path "$QtDir\bin\qmake.exe")) {
    Fail "Qt 6 (MSVC 64-bit) não encontrado. Informe -QtDir C:\Qt\6.x.y\msvc2019_64."
}
if (-not (Test-Path "$OpenSslDir\include\openssl\ssl.h")) {
    Fail "OpenSSL não encontrado em '$OpenSslDir'. Informe -OpenSslDir."
}

# --- Load the Visual Studio x64 developer environment ------------------------
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { Fail "Visual Studio Build Tools não encontrado." }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { Fail "Componente C++ (VC Tools x64) do Visual Studio não instalado." }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

# winget installs CMake and Ninja here; a new shell may not have it on PATH yet.
$wingetLinks = "$env:LOCALAPPDATA\Microsoft\WinGet\Links"
if ((Test-Path $wingetLinks) -and ($env:PATH -notlike "*$wingetLinks*")) {
    $env:PATH = "$wingetLinks;$env:PATH"
}
foreach ($tool in "cmake", "ninja") {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { Fail "$tool não encontrado no PATH." }
}

Write-Host "Qt:      $QtDir"
Write-Host "OpenSSL: $OpenSslDir"

# --- Configure, build, test, install ------------------------------------------
# OpenSSL is linked statically so the installed app needs no OpenSSL DLLs.
cmake -S . -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    "-DOPENSSL_ROOT_DIR=$OpenSslDir" `
    "-DOPENSSL_USE_STATIC_LIBS=TRUE" `
    "-DCMAKE_INSTALL_PREFIX=$(Join-Path $root $InstallDir)"
if ($LASTEXITCODE -ne 0) { Fail "configuração do CMake falhou." }

cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { Fail "compilação falhou." }

if (-not $SkipTests) {
    # The GUI tests link Qt dynamically and need its DLLs on PATH.
    $env:PATH = "$QtDir\bin;$env:PATH"
    ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { Fail "testes falharam." }
}

cmake --install $BuildDir
if ($LASTEXITCODE -ne 0) { Fail "instalação na pasta '$InstallDir' falhou." }
Write-Host "Aplicativo pronto em $InstallDir\InputServer.exe" -ForegroundColor Green

# --- Installer ----------------------------------------------------------------
if ($Installer) {
    $iscc = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $iscc) { Fail "Inno Setup 6 não encontrado (winget install JRSoftware.InnoSetup)." }

    & $iscc /Q "$BuildDir\installer-inno\inputserver.iss"
    if ($LASTEXITCODE -ne 0) { Fail "geração do instalador falhou." }
    Get-ChildItem "$BuildDir\installer-inno\bin\InputServerSetup-*.exe" |
        ForEach-Object { Write-Host "Instalador: $($_.FullName)" -ForegroundColor Green }
}
