@echo off
rem compiles all .asm files in specified folder 

IF "%1"=="" (
 echo  compiles all asm .asm files in specified folder from GAS to MS ARM assembler
 echo usage:
 echo CompileArmAsmFolder folder
 goto END
)

for %%f in (%1\*.asm) do (
  echo Processing: %%f ...
  call CompileArmAsm %%f
)

:END




