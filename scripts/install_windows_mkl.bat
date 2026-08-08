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
REM This script's own claim ceiling: it has been reviewed against a live,
REM reachable download URL and a real, currently-published component id
REM (intel.oneapi.win.mkl.devel, per https://oneapi-src.github.io/oneapi-ci/)
REM plus a best-effort second id for the OpenMP runtime
REM (intel.oneapi.win.openmp) that was NOT independently confirmed the same
REM way -- if the bootstrapper rejects that id, or the installed MKL layout
REM under C:\Program Files (x86)\Intel\oneAPI\mkl\latest does not match what
REM FindMKL.cmake expects, that is exactly the kind of first-contact finding
REM the windows-clang-release CI job exists to surface. See docs/ci.md.
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
del %TEMP%\webimage.exe

webimage_extracted\bootstrapper.exe -s --action install --components=%COMPONENTS% --eula=accept -p=NEED_VS2017_INTEGRATION=0 -p=NEED_VS2019_INTEGRATION=0 -p=NEED_VS2022_INTEGRATION=0 --log-dir=.
set installer_exit_code=%ERRORLEVEL%

rd /s /q "webimage_extracted"
exit /b %installer_exit_code%
