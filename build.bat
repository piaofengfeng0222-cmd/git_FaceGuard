@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d C:\FaceGuard
"C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSBuild.exe" FaceGuard.sln -p:Configuration=Release -p:Platform=x64 -t:Build -v:minimal
