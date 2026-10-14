@echo off
rem Build helper: MSVC x64 from the VS 2022 Professional install (the Community install on E: has a broken vcvarsall).
rem Everything that runs lands in bin\ (objects and the test binaries in build\).
rem   build.bat                      -> bin\fsvr_console.exe, the console (:console below)
rem   build.bat test                 -> builds and runs the self checks (engine, formant, presets)
rem   build.bat plugin               -> every plug-in format through CMake into bin\<format>: CLAP, VST3, VST2 and the
rem                                     standalone (64-bit, build\x64), then the 32-bit VST2 with the DXi in it (build\x86),
rem                                     then the plug-in's own checks (check_plugin, check_gui)
rem   build.bat bench                -> bin\vop3_bench.exe, the VOP3 chips' cost and bit-exact output (tools\vop3_bench.cpp),
rem                                     and bin\vop3_jit_check.exe, the compiled programs against the interpreter
rem   build.bat file.cpp [cl args]   -> compiles whatever you pass (paths relative to this folder)
rem   set FSVR_MATH=0..3           -> the sample loop's maths backend (src\fsvr\fastmath.h): 0 raw libm, 1 LUT (unset), 2 CORDIC, 3 hybrid
rem CMakeLists.txt builds the same targets for anything that is not MSVC-on-Windows.
pushd "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not exist build mkdir build
if not exist bin mkdir bin
set MATHDEF=
rem The chip, its two hosts and the sections over them are bit-exact only without FMA contraction, so they are compiled apart.
rem VS 2022 does not contract under /fp:precise (tools\vop3_bench.cpp writes the same bytes as under /fp:strict).
rem AsmJit (third_party\asmjit), which the chip's compiler emits its code with, is built once into build\asmjit. It needs
rem C++20, so it and yss236_jit.cpp, the one file that includes it, are built as C++20 and the rest as C++17.
set ASMJITDEF=/DASMJIT_STATIC /DASMJIT_NO_BUILDER /DASMJIT_NO_COMPILER /DASMJIT_NO_UJIT /DASMJIT_NO_FOREIGN /DASMJIT_NO_SHM_OPEN /I third_party\asmjit
set VOP3FLAGS=/nologo /O2 /EHsc /W3 /std:c++17 /fp:precise /I src %ASMJITDEF%
set VOP3OBJ=build\vop3host\*.obj build\asmjit\*.obj
if defined FSVR_MATH set MATHDEF=/DFSVR_MATH=%FSVR_MATH%
if "%~1"=="" (
  call :console
) else if "%~1"=="test" (
  call :console || goto :done
  "%~dp0bin\fsvr_console.exe" -selftest || goto :done
  python "%~dp0tools\check_formant.py" || goto :done
  python "%~dp0tools\check_presets.py"
) else if "%~1"=="plugin" (
  cmake -S . -B build\x64 -G "Visual Studio 17 2022" -A x64 -DFSVR_BUILD_PLUGIN=ON || goto :done
  cmake --build build\x64 --config Release -- -m || goto :done
  cmake -S . -B build\x86 -G "Visual Studio 17 2022" -A Win32 -DFSVR_BUILD_PLUGIN=ON || goto :done
  cmake --build build\x86 --config Release -- -m || goto :done
  build\x64\check_plugin.exe || goto :done
  build\x64\check_gui.exe
) else if "%~1"=="bench" (
  call :vop3host || goto :done
  cl %VOP3FLAGS% tools\vop3_bench.cpp %VOP3OBJ% /Fe:bin\vop3_bench.exe /Fobuild\ || goto :done
  cl %VOP3FLAGS% tools\vop3_jit_check.cpp %VOP3OBJ% /Fe:bin\vop3_jit_check.exe /Fobuild\
) else (
  cl /nologo /O2 /EHsc /W3 /std:c++17 %* /Fobuild\
)
goto :done

rem The console.
:console
call :vop3host || exit /b 1
cl /nologo /O2 /EHsc /W3 /std:c++17 %MATHDEF% /I src src\fs1r\chips\ymp706.cpp src\fs1r\firmware\controllers.cpp src\fs1r\firmware\fseq.cpp src\fs1r\firmware\midi.cpp src\fs1r\firmware\notes.cpp src\fs1r\firmware\patch.cpp src\fs1r\firmware\rom.cpp src\fsvr\device.cpp src\fsvr\fastmath.cpp src\fsvr\selftest.cpp %VOP3OBJ% src\console\main.cpp winmm.lib /Fe:bin\fsvr_console.exe /Fobuild\
exit /b %ERRORLEVEL%

:vop3host
if not exist build\asmjit\x86_assembler.obj (
  if not exist build\asmjit mkdir build\asmjit
  cl /nologo /c /MP /O2 /EHsc /W0 /std:c++20 %ASMJITDEF% third_party\asmjit\asmjit\core\*.cpp third_party\asmjit\asmjit\x86\*.cpp third_party\asmjit\asmjit\arm\*.cpp third_party\asmjit\asmjit\axl\*.cpp /Fobuild\asmjit\ || (rmdir /s /q build\asmjit & exit /b 1)
)
if exist build\vop3host rmdir /s /q build\vop3host
mkdir build\vop3host
cl /c /MP %VOP3FLAGS% src\fs1r\firmware\vop3host\*.cpp src\fs1r\effects.cpp src\fs1r\filter.cpp src\fs1r\chips\vop3.cpp /Fobuild\vop3host\ || exit /b 1
cl /c %VOP3FLAGS% /std:c++20 src\fs1r\chips\yss236_jit.cpp /Fobuild\vop3host\
exit /b %ERRORLEVEL%

:done
set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
