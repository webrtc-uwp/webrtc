@echo off
rem Compile file using armasm

IF "%1"=="" (
 echo Compile file using armasm
 echo usage:
 echo CompileArmAsm filename
 goto END
)

armasm -oldit %1 -o %~p1/%~n1.obj
rem for /f %%i in ('armasm %1 -o %~p1/%~n1.obj') do set VAR=%%i
rem echo %VAR

:END




