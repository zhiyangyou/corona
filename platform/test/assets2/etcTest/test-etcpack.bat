chcp 65001
set basedir=C:\_YzyDianchuSpace\_Gitlab\Solar2D-Moksha\platform\test\assets2\etcTest
set commonArgs=-v on -s fast -e perceptual -c etc2
set exeFile=etcpack.exe
REM "%exeFile%" %commonArgs% -f RGB  %basedir%\bg1-non4.jpg %basedir%\bg1-non4.jpg.pkm

REM %exeFile%    %commonArgs%  -f RGB  %basedir%\bg-4.jpg  %basedir%\bg-4.jpg.pkm

%exeFile%    %commonArgs%  -f RGBA8 %basedir%\minister-non4.png  %basedir%\minister-non4.png.pkm
 