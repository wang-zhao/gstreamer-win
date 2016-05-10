

Requirements
-------------------------------------

  Windows Build

    1. MSys/MinGW
       a. This step is typically not necessary if you just want to compile GStreamer itself. Only do this if you're comfortable with an MSys/MinGW environment or have special needs. Otherwise skip step 1 entirely.
       b. Run "msys-env-x86.bat" or "msys-env-x86_64.bat" in the Tools/ directory to automatically setup a build environment for all the dependencies.
       c. Double click on the "msys.bat" shortcut in the top level directory to open a shell.
       d. To build dependencies, from the shell:
              cd <OSSBuild Home>
              cd Libraries/
              ./Clean-All-Windows-x86.sh
              ./Build-All-Windows-x86.sh
    2. Visual Studio 2008 or later
    3. Windows Installer XML (WiX) 3.5 or later (e.g. http://wix.sourceforge.net/releases/3.5.2430.0/Wix35.msi)
    4. Perl 5.10+ (32-bit, x86-compatible)
       a. You can skip this step if you're only interested in building GStreamer itself and not the dependencies.
       b. Recommended ActiveState Perl (http://www.activestate.com/downloads/) on Windows
          1. Download the LibXML package (these steps may be different in future releases)
             a. Load the perl package manager ("ppm" command)
             b. Select the Edit > Preferences menu
             c. Under "Add Repository" select "University of Winnipeg" from the "suggested" list and select "Add"
             d. Select "OK"
             e. Scroll down the list and right click on "XML-LibXML" and "XML-LibXML-Common" both and select install
             f. Select File > Run Marked Actions
    5. DirectX SDK (November 2008 or later)
       a. Download this, for example, from http://www.microsoft.com/downloads/info.aspx?na=90&p=&SrcDisplayLang=en&SrcCategoryId=&SrcFamilyId=5493f76a-6d37-478d-ba17-28b1cca4865a&u=http%3a%2f%2fdownload.microsoft.com%2fdownload%2f5%2f8%2f2%2f58223f79-689d-47ae-bdd0-056116ee8d16%2fDXSDK_Nov08.exe
    6. Python 2.6  (32-bit, x86-compatible)
       b. Download, for example, from http://www.python.org/download/releases/2.6.6/
       c. Install to C:\Python26
    7. PyGobject (2.14)
       a. Download, for example, from http://ftp.gnome.org/pub/GNOME/binaries/win32/pygobject/2.14/
    

  Linux Build

    1. See directions for your individual distribution below.




Setup Visual Studio 2008+
-------------------------------------

  Add Custom Rules Path

    1. Select the Tools > Options... menu.
    2. Expand "Projects and Solutions" and select "VC++ Project Settings"
    3. In "Rule File Search Paths", add the top-level directory for this code (e.g. C:\OSSBuild\)
       a. If needed, separate other search paths with a semicolon (;).



Setup Linux
-------------------------------------

  Ubuntu 8.04
    1. sudo su
    2. ./Shared/Scripts/Init-Ubuntu-8.04.sh

  Ubuntu 9.10

    1. sudo apt-get install openjdk-6-jdk git-core subversion perl sed pkg-config build-essential autoconf bison flex libtool tofrodos vim gettext yasm nasm zlib1g-dev mesa-common-dev libglu1-mesa-dev libxmu-dev libx11-dev libxi-dev libcurl4-gnutls-dev libxrender-dev autoconf libxv-dev libasound2-dev libv4l-dev libpulse-dev python2.6-dev python-gobject-dev mono-devel

  Debian 5.0.4

    1. /bin/su -
    2. apt-get install openjdk-6-jdk git-core subversion perl sed pkg-config build-essential autoconf bison flex libtool tofrodos vim gettext yasm nasm zlib1g-dev mesa-common-dev libglu1-mesa-dev libxmu-dev libx11-dev libxi-dev libcurl4-gnutls-dev libxrender-dev autoconf libxv-dev libasound2-dev v4l-conf libpulse-dev mono-tools-devel python-gobject-dev



Acknowledgments
-------------------------------------

OAH Build
https://launchpad.net/oah

GStreamer WinBuilds
http://www.gstreamer-winbuild.ylatuya.es/doku.php?id=start

GStreamer
http://gstreamer.freedesktop.org/

7-Zip
http://www.7-zip.org/

Msys+Mingw
http://www.mingw.org/

Yasm
http://www.tortall.net/projects/yasm/

For all the dependencies, please see ./Libraries/Packages/ReadMe.txt


Licenses
-------------------------------------

Please see the licenses in the Licenses/ folder for details on each library/application used 
or in the ./Libraries/Packages/ folder in each individual package.

For copyright information, please see COPYING.

For legal purposes, you must inspect the individual licenses of all packages to determine if 
they fit your legal constraints. OSSBuild or any of its developers are NOT legally responsible 
for any failure to ensure compliance or any legal action resulting from the use of this 
software. It is solely the responsibility of the recipient and user to determine legal 
eligibility for use.
