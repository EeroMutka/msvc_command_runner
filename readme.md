
# MSVC Command Runner

You're on your way to make some cool new programming project, and you need to build or link some C/C++ code on windows. How do you do this? Microsoft tells you to launch the X64 Native Tools Command Prompt for Visual Studio XXX, then cd your way into the correct directory, and call `cl.exe` on your source code files. It would be *sooo boring* if you could just call `cl.exe` from a normal command line...

This is very frustrating, especially if you just want to quickly open *cmd* at some directory by typing "cmd" to the address bar in your windows explorer to build some code, or if you want to setup some other tool like VSCode to build your code.

One solution to this problem would be to manually add everything necessary to your environment variables when installing a new version of visual studio:
https://stackoverflow.com/questions/84404/using-visual-studios-cl-from-a-normal-command-line

Another solution is to use *MSVC Command Runner*! This project generates executables with the same names as the MSVC tools that redirect your commands into the original executables, but with the correct paths setup automatically.

**Supported commands**:
 - `cl.exe`
 - `link.exe`
 - `lib.exe`
 - `dumpbin.exe`
 - `msbuild.exe`
 - `devenv.exe`
 - `ml64.exe`
 - Bonus: `where_is_my_msvc.exe`. This just prints out some directories showing where your windows SDK / visual studio installion is located.

Just build this project, then add the *build* folder to your PATH, and boom! You won't need *X64 Native Tools Command Prompt for Visual Studio 2022* ever again! Your boring old command prompt will *just work*.

## How to build
 1. Launch *X64 Native Tools Command Prompt for Visual Studio XXX* and cd your way to the downloaded directory :)
 2. run *build.bat*
