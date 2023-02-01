## MSVC Command Runner

You're on your way to make some cool new programming project, and you need to build
or link some code on windows. How do you do this? Microsoft tells you to launch the X64 Native Tools
Command Prompt for Visual Studio XXX, then cd your way into the correct directory, and
call `cl.exe` on your source code files. It would be *sooo boring* if you could just call `cl.exe` from a normal
command line...

This is very frustrating, especially if you just want to quickly open *cmd* at some directory by typing
"cmd" to the address bar in your windows explorer to build some code, or if you want to setup some other
tool like VSCode to build your code.

One solution to this problem would be to manually add everything necessary to your environment variables
when installing a new version of visual studio:
https://stackoverflow.com/questions/84404/using-visual-studios-cl-from-a-normal-command-line

Another solution is to use `msvc_command_runner`! The purpose of this tool is to just redirect
your command to the actual MSVC command, but with the right paths setup automatically.
`msvc_command_runner` supports both `cl.exe`, `link.exe` and `dumpbin.exe`.

Just build this project, then add the directory that contains the built `msvc_command_runner` executables
to your PATH, and boom! You won't need *X64 Native Tools Command Prompt for Visual Studio 2022* ever again!
Your boring old command prompt will *just work*.

As a bonus, there's also `where_is_my_msvc`, which just prints out some directories showing
where your windows SDK / visual studio is located.

## How to build

1. Launch *X64 Native Tools Command Prompt for Visual Studio XXX* and cd your way to the downloaded directory :)
2. run *build.bat*
