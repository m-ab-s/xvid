command = "powershell.exe -nologo -WindowStyle hidden -Noninteractive -NoProfile -ExecutionPolicy Bypass -File " & WScript.Arguments.Item(0)

set shell = CreateObject("WScript.Shell")
shell.Run command,0