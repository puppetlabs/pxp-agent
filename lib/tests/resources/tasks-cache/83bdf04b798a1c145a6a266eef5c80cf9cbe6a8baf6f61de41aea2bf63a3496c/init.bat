@echo off
set installdir=%PT__installdir:/=\%
type %installdir%\file1.txt %installdir%\dir\file2.txt %installdir%\dir\sub_dir\file3.txt 2>nul
