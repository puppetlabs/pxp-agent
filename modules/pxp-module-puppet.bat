@echo off
SETLOCAL

call "%~dp0..\..\bin\environment.bat" %0 %*

"%PUPPET_DIR%\bin\ruby.exe" -S -- "%~dp0\pxp-module-puppet" %*
