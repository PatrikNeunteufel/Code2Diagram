@echo off
rem Release-clang konfigurieren und bauen - in der Umgebung von Visual Studio 18 (2026).
rem
rem   tools\bau_clang.bat [Ziel]
rem
rem Ohne Ziel alles, sonst etwa: tools\bau_clang.bat C2dKern_UnitTests
rem
rem Warum so (Erfahrung aus LumiViz, dort tools\bau_clang.bat): Konfigurieren und
rem Bauen gehoeren in dieselbe Umgebung; ausserhalb scheitern Ressourcen-Dateien
rem ohne Meldung, mit der Umgebung von Visual Studio 2022 das Linken.
rem Konfiguriert wird jedes Mal: eine geaenderte Solution.json loest von selbst
rem kein neues Konfigurieren aus.
rem Ein offener Tester oder ein laufendes Code2Diagram sperrt Dateien neben der
rem Exe - dann wird nicht gebaut (Rueckgabe 3).
setlocal
tasklist /FI "IMAGENAME eq Sichttest.exe" | find /I "Sichttest.exe" >nul && (echo LAEUFT: Sichttest - nicht gebaut & exit /b 3)
tasklist /FI "IMAGENAME eq Code2Diagram.exe" | find /I "Code2Diagram.exe" >nul && (echo LAEUFT: Code2Diagram - nicht gebaut & exit /b 3)
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (echo Nicht gefunden: %VCVARS% & exit /b 2)
call "%VCVARS%" >nul || exit /b 2
cd /d "%~dp0.." || exit /b 2
cmake --preset windows-ninja-release-clang >"%TEMP%\c2d_konfig.log" 2>&1 || (type "%TEMP%\c2d_konfig.log" & exit /b 1)
if "%~1"=="" (
    cmake --build --preset build-ninja-release-clang --parallel 4
) else (
    cmake --build --preset build-ninja-release-clang --parallel 4 --target %1
)
exit /b %ERRORLEVEL%
