@echo off

echo Compiling VSTR (graphics mode stretch utility)...
@bcc -mt -Os -f- -N- -G- vstr.cpp
if ERRORLEVEL NEQ 0 goto :ERROR

echo Compiling TESTVLR (vertical line replicaton test utility)...
@bcc -ml testvlr.c
if ERRORLEVEL NEQ 0 goto :ERROR

echo Done.
goto :END

:ERROR
echo Failed to compile!

:END
