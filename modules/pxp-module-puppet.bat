@echo off
SETLOCAL

call "%~dp0..\..\bin\environment.bat" %0 %*

ruby -S -- "%~dp0%SCRIPT_NAME%" %*
