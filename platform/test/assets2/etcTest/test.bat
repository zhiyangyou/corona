chcp 65001
set basedir=C:\_YzyDianchuSpace\_Gitlab\Solar2D-Moksha\platform\test\assets2\etcTest
set commonArgs=-j 16 -verbose -effort 0
set suffix=ktx
 

set filename2=bg-4.jpg
set filename3=bg-4.png
magick %filename2% %filename3%
EtcTool.exe  %basedir%\%filename3%  %commonArgs%  -format RGB8 -output %basedir%\%filename2%.%suffix%

del /Q %filename3%

set pngArgs=  -format  RGBA8   
EtcTool.exe  %basedir%\minister-non4.png   %commonArgs% %pngArgs% -output %basedir%\minister-non4.png.%suffix%

EtcTool.exe  %basedir%\king_2_5.png   %commonArgs% %pngArgs% -output %basedir%\king_2_5.png.%suffix%
