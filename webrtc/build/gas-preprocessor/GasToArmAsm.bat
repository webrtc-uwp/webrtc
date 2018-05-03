@echo off
rem Converts specified file from GAS to MS ARM assembler
rem Requirements: perl and GNU Cpp compiler in path (cpp.exe)

IF "%1"=="" (
 echo converts specified file form GAS to MS ARM assembler
 echo usage:
 echo GasToArmAsm filename
 goto END
)

if NOT EXIST %~p1\generated MKDIR %~p1\generated

SET GASPP_DEBUG=1
perl gas-preprocessor.pl -as-type armasm -force-thumb -- armasm -I../../ -c %1 -o %~p1/generated/%~n1.obj >  %~p1/generated/%~n1.asm
SET GASPP_DEBUG=

:END




