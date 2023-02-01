
:: delete the build folder first to make sure we aren't trying to build the
:: project using the msvc_command_runner executables themselves
if EXIST build RMDIR /S /Q build
mkdir build
cd build

:: cl "../where_is_my_msvc.cpp" "../foundation/foundation.c" /link Advapi32.lib Ole32.lib OleAut32.lib
:: cl "../dumpbin.cpp" "../foundation/foundation.c" /link Advapi32.lib Ole32.lib OleAut32.lib
:: cl "../link.cpp" "../foundation/foundation.c" /link Advapi32.lib Ole32.lib OleAut32.lib
:: cl "../lib.cpp" "../foundation/foundation.c" /link Advapi32.lib Ole32.lib OleAut32.lib
:: cl "../ml64.cpp" "../foundation/foundation.c" /link Advapi32.lib Ole32.lib OleAut32.lib
cl "../cl.cpp" "../foundation/foundation.c" /link Advapi32.lib Ole32.lib OleAut32.lib

del *.obj
cd ..

@echo Done!