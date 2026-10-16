Set WshShell = CreateObject("WScript.Shell")
WshShell.CurrentDirectory = "C:\Users\shaan\OneDrive\Documents\C++\esp\CompanionOS\python"
WshShell.Run """C:\Users\shaan\AppData\Local\Programs\Python\Python311\pythonw.exe"" companion_controller.py", 0, False
