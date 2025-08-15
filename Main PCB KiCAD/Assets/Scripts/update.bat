@echo off

echo Purges old copies if they are here
del main.zip /F
rmdir common_kicad-main /Q /S

echo grab and unzip a new copy

bin\wget.exe https://github.com/PhilipMcGaw/common_kicad/archive/refs/heads/main.zip
tar -xf main.zip

echo place the new files in the right location
xcopy common_kicad-main\Assets\* ..\ /s /e /h /y

echo purge the downloads
del main.zip /F
rmdir common_kicad-main /Q /S
