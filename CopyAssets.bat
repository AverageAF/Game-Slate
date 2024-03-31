
del Assets.dat
del CopyAssets.log

REM  --bitmaps--



REM   -fonts-

MyMiniz.exe Assets.dat + .\Assets\PixelFont(6x7).bmpx >> CopyAssets.log

REM 	-sound-

MyMiniz.exe Assets.dat + .\Assets\item.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\itemshort.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\menu.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\ouch.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\ouchhurt.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\ouchlethal.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\SplashNoise.wav >> CopyAssets.log
MyMiniz.exe Assets.dat + .\Assets\SplashNoise2.wav >> CopyAssets.log

REM copy Assets.dat into whatever directory it needs to be in