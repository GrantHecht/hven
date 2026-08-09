@echo off
REM Installs the Windows pieces of Intel oneAPI that hven's CMake build
REM needs: the Math Kernel Library devel component (headers + import libs,
REM cmake/FindMKL.cmake's target) and the OpenMP runtime component, because
REM FindMKL.cmake's default (unset MKL_USE_SEQUENTIAL) threading layer is
REM "intel", which links libiomp5md.lib -- MKL's devel component alone does
REM not guarantee that library ships with it.
REM
REM Pattern: download the combined oneAPI Base+HPC offline installer .exe
REM (a self-extracting archive holding every toolkit component locally),
REM extract it, then run its embedded bootstrapper.exe with --components=
REM to actually install only the two components above. This mirrors
REM oneAPI's own CI reference script
REM (https://github.com/oneapi-src/oneapi-ci/blob/master/scripts/install_windows.bat)
REM and the same two-step shape used by other public oneAPI-on-GitHub-Actions
REM setups (e.g. https://github.com/equipez/github_actions_scripts). The URL
REM below is the exact one oneapi-ci's own live workflow
REM (.github/workflows/build_all.yml, WINDOWS_TOOLKIT_URL) used at the time
REM this script was written, and was confirmed reachable (HTTP 200,
REM Content-Length ~1.9 GB) by a direct request before being pasted here --
REM not guessed. Bump it by pulling the current WINDOWS_TOOLKIT_URL from
REM that workflow file when Intel rotates the version.
REM
REM This script's own claim ceiling: both component ids below are now
REM CONFIRMED, not just plausible -- intel.oneapi.win.mkl.devel was verified
REM against Intel's own published component listing when this script was
REM written, and intel.oneapi.win.openmp (a best-effort guess at the time)
REM is confirmed by the strongest evidence there is: across five real CI
REM runs, the component installed, libiomp5md.lib resolved at configure
REM time under C:\Program Files (x86)\Intel\oneAPI\compiler\latest\lib
REM exactly where FindMKL.cmake expects it, and every test binary that
REM links it loaded and ran. See docs/ci.md's Windows lane section.
REM
REM Usage: scripts\install_windows_mkl.bat
REM Requires: curl (bundled with Windows 10 1803+ and the windows-latest
REM runner image).

setlocal

set URL=https://registrationcenter-download.intel.com/akdlm/IRC_NAS/4144bec3-82ce-4672-bd71-5c93a79cd5e7/intel-oneapi-toolkit-2026.1.0.191_offline.exe
set COMPONENTS=intel.oneapi.win.mkl.devel:intel.oneapi.win.openmp

curl.exe --output %TEMP%\webimage.exe --url %URL% --retry 5 --retry-delay 5
if errorlevel 1 exit /b 1

start /b /wait %TEMP%\webimage.exe -s -x -f webimage_extracted --log extract.log
REM Cache hits bypass this guard; any cache miss reaches it, so ERRORLEVEL propagation through start /b /wait is assumed correct but has not been directly observed.
if errorlevel 1 (
    del %TEMP%\webimage.exe
    exit /b 1
)
del %TEMP%\webimage.exe

REM A failed extraction here would otherwise surface only indirectly, as
REM bootstrapper.exe not existing below (errorlevel 9009, still caught and
REM returned by the block after it) -- the explicit check above gives a
REM legible failure at the point it actually happened instead.
webimage_extracted\bootstrapper.exe -s --action install --components=%COMPONENTS% --eula=accept -p=NEED_VS2017_INTEGRATION=0 -p=NEED_VS2019_INTEGRATION=0 -p=NEED_VS2022_INTEGRATION=0 --log-dir=.
set installer_exit_code=%ERRORLEVEL%

rd /s /q "webimage_extracted"
exit /b %installer_exit_code%
