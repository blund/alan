@echo off

IF NOT EXIST w:\build mkdir w:\build

pushd w:\build

set CompilerFlags=-MT -nologo -GR- -EHa- -Od -Oi -W4 -wd4201 -wd4100 -wd4189 -wd4456 -Zi
set IgnoreWarn=-wd4201 -wd4100 -wd4189 -wd4456
set ProgramVariables=
set LinkerFlags=-opt:ref -incremental:no
set Libraries=

:::: Her er en rekke kompilatorflagg som brukes
:: -GR- er typeinformasjon i c++
:: -EHa er exception handling i c++
:: -Oi er bruk av exepction-handling
:: -WX er for å behandle warnings som errors
:: -W4 er warning nivå, unngår masse windows.h-drit
:: -Zi generer debug-informasjon
:: -opt:ref sørger for at så like som mulig bygges inn i vår executable
del *.pdb > NUL 2> NUL
cl %CompilerFlags% %ProgramVariables% w:\turing\turing.cpp -link %LinkerFlags% %Libraries%

popd
