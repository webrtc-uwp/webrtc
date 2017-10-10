@echo off
rem Converts all .s files in specified folder from GAS to MS ARM assembler

IF "%1"=="" (
 echo Converts all .s files in specified folder from GAS to MS ARM assembler
 echo usage:
 echo GasToArmAsmFolder folder
 goto END
)

for %%f in (%1\*.s) do (
  echo Processing: %%f ...
  call GasToArmAsm %%f
)

:END




