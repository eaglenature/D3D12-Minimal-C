@echo off

set BuildDirectory=..\build

if not exist %BuildDirectory% mkdir %BuildDirectory%
pushd %BuildDirectory%
del *.pdb > nul 2> nul

set PrepFlags=
set CompFlags= -diagnostics:column -WL -nologo -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -FC -Zi
set LinkFlags= -incremental:no winmm.lib user32.lib gdi32.lib ole32.lib d3d12.lib dxguid.lib dxgi.lib d3dcompiler.lib

set BuildTarget=win32_d3d12_minimal.exe
set SourceFiles=..\code\win32_d3d12_minimal.c
cl %PrepFlags% %CompFlags% -MTd -Fe%BuildTarget% %SourceFiles% -I..\code /link %LinkFlags%

popd
