REM Dopasuj poniższe ścieżki:
set MINGW_PATH=c:\AM2023\tools\mingw-w64\bin
set CMAKE_PATH=c:\AM2023\tools\cmake\bin


set PATH=%PATH%;%MINGW_PATH%;%CMAKE_PATH%;

cmake -G "MinGW Makefiles" -B build
mingw32-make.exe -C build