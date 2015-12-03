@ECHO OFF

REM git describe --always

rmdir /S /Q bin\x64
rmdir /S /Q lib\x64
rmdir /S /Q win_build64

ECHO ===== CMake for 64-bit ======
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" amd64
mkdir win_build64 
cd win_build64
cmake -G "Visual Studio 12 Win64" -D__BUILD_VERSION__=%BUILD_VERSION% ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Debug /p:Platform=x64 /maxcpucount:12
msbuild ALL_BUILD.vcxproj /p:Configuration=Release /p:Platform=x64 /maxcpucount:12
cd ..

REM ECHO ===== CMake for 32-bit ======
REM mkdir win_build32 
REM cd win_build32
REM del CMakeCache.txt
REM cmake -G "Visual Studio 12" ..
REM cd ..


:errorexit
set returncode=%errorlevel%
echo There was an error during CMake (error code %returncode%).
exit /b %returncode%

:cleanexit

