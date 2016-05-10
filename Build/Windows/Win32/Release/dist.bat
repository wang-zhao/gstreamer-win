echo on
cd /d "%~dp0"
cd .\bin

copy libgio-2.0-x.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>  ../dist.log 2>../errort.log
copy libglib-2.0-x.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgmodule-2.0-x.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgobject-2.0-x.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgthread-2.0-x.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libpango-1.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libharfbuzz-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libpangowin32-1.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libpangoft2-1.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libpangocairo-1.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libatk-1.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy glib-genmarshal.exe ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy gtk-update-icon-cache.exe ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libgdk_pixbuf-2.0-0.dll  ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libpng15.dll   ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy glib-compile-resources.exe ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy gdk-pixbuf-pixdata.exe ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy glib-compile-schemas.exe ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy pixman.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin				>> ../dist.log 2>>../errort.log
copy libcairo-2.dll  ..\..\..\..\..\Shared\Build\Windows\Win32\bin				>> ../dist.log 2>>../errort.log

copy libgailutil-3.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libgdk-3.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgpac.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log

copy libgdk-win32-2.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgtk-win32-2.0-0.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy librtmp.dll ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log

cd ..

cd .\pdb

copy libgio-2.0-x.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libglib-2.0-x.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgmodule-2.0-x.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgobject-2.0-x.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgthread-2.0-x.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libpango-1.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libpangowin32-1.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libatk-1.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libharfbuzz-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libpangoft2-1.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libpangocairo-1.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libgdk_pixbuf-2.0-0.pdb  ..\..\..\..\..\Shared\Build\Windows\Win32\bin		>> ../dist.log 2>>../errort.log
copy libpng15.pdb   ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy pixman.pdb   ..\..\..\..\..\Shared\Build\Windows\Win32\bin				>> ../dist.log 2>>../errort.log
copy libcairo-2.pdb    ..\..\..\..\..\Shared\Build\Windows\Win32\bin				>> ../dist.log 2>>../errort.log
copy libgpac.pdb    ..\..\..\..\..\Shared\Build\Windows\Win32\bin				>> ../dist.log 2>>../errort.log
copy libgdk-win32-2.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy libgtk-win32-2.0-0.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log
copy librtmp.pdb ..\..\..\..\..\Shared\Build\Windows\Win32\bin			>> ../dist.log 2>>../errort.log


cd ..

cd .\lib

copy gio-2.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib				>> ../dist.log 2>>../errort.log
copy glib-2.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib				>> ../dist.log 2>>../errort.log
copy gmodule-2.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy gobject-2.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy gthread-2.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy pango-1.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy pangowin32-1.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy atk-1.0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib				>> ../dist.log 2>>../errort.log
copy libharfbuzz-0.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy pangocairo-1.0.lib  ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy pangoft2-1.0.lib  ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy gdk_pixbuf-2.0.lib   ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy libpng15.lib    ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy cairo.lib     ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy libgpac.lib     ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log


copy gailutil-3.0.lib     ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy libgail.lib     ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy gdk-3.0.lib    ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy gdk-win32.lib    ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy gtk-3.0.lib    ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log
copy pixman.lib   ..\..\..\..\..\Shared\Build\Windows\Win32\lib				>> ../dist.log 2>>../errort.log
copy librtmp.lib ..\..\..\..\..\Shared\Build\Windows\Win32\lib			>> ../dist.log 2>>../errort.log

cd ..



cd .\include 

xcopy /E /Y /I glib-2.0 ..\..\..\..\..\Shared\Build\Windows\Win32\include\glib-2.0		>> ../dist.log 2>>../errort.log
xcopy /E /Y /I pango-1.0 ..\..\..\..\..\Shared\Build\Windows\Win32\include\pango-1.0		>> ../dist.log 2>>../errort.log
xcopy /E /Y /I atk-1.0 ..\..\..\..\..\Shared\Build\Windows\Win32\include\atk-1.0		>> ../dist.log 2>>../errort.log
xcopy /E /Y /I libharfbuzz ..\..\..\..\..\Shared\Build\Windows\Win32\include\libharfbuzz	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I gdk-pixbuf-2.0  ..\..\..\..\..\Shared\Build\Windows\Win32\include\gdk-pixbuf-2.0	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I libpng15   ..\..\..\..\..\Shared\Build\Windows\Win32\include\			>> ../dist.log 2>>../errort.log
xcopy /E /Y /I libpng15   ..\..\..\..\..\Shared\Build\Windows\Win32\include\libpng15		>> ../dist.log 2>>../errort.log

xcopy /E /Y /I gail-3.0    ..\..\..\..\..\Shared\Build\Windows\Win32\include\gail-3.0	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I gtk-3.0     ..\..\..\..\..\Shared\Build\Windows\Win32\include\gtk-3.0	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I pixman-1  ..\..\..\..\..\Shared\Build\Windows\Win32\include\pixman-1	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I cairo   ..\..\..\..\..\Shared\Build\Windows\Win32\include\cairo	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I libgpac   ..\..\..\..\..\Shared\Build\Windows\Win32\include\libgpac	>> ../dist.log 2>>../errort.log
xcopy /E /Y /I gtk-2.0   ..\..\..\..\..\Shared\Build\Windows\Win32\include\gtk-2.0	>> ../dist.log 2>>../errort.log

cd .. 

xcopy /E /Y /I share ..\..\..\..\Shared\Build\Windows\Win32\share	>> ./dist.log 2>>./errort.log

pause

