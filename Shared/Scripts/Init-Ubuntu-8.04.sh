#!/bin/sh

if [ $(/usr/bin/id -u) -ne 0 ]; then
    echo "Not running as root or super user. Try using 'sudo su' first and then run this script."
    exit 1
fi



BUILD_DIR=/tmp/ossbuild_init

mkdir -p $BUILD_DIR
cd $BUILD_DIR

apt-get install openjdk-6-jdk git-core subversion perl sed pkg-config build-essential bison flex tofrodos vim gettext nasm zlib1g-dev mesa-common-dev libglu1-mesa-dev libxmu-dev libx11-dev libxi-dev libcurl4-gnutls-dev libxrender-dev libxv-dev libasound2-dev v4l-conf libpulse-dev python-all python-gobject-dev python-gtk2-dev mono-common
apt-get remove yasm libtool autoconf automake mono-tools-devel mono-gmcs mono-mcs

cd $BUILD_DIR
wget http://ftp.gnu.org/gnu/libtool/libtool-2.2.6b.tar.gz
tar xzvf libtool-2.2.6b.tar.gz
cd libtool-2.2.6b/
./configure --prefix=/usr/local
make && make install

cd $BUILD_DIR
wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz
tar xzvf autoconf-2.65.tar.gz
cd autoconf-2.65/
./configure --prefix=/usr/local
make && make install

cd $BUILD_DIR
wget http://ftp.gnu.org/gnu/automake/automake-1.11.1.tar.gz
tar xzvf automake-1.11.1.tar.gz
cd automake-1.11.1/
./configure --prefix=/usr/local
make && make install
echo "/usr/share/aclocal" | sudo tee -a /usr/local/share/aclocal/dirlist

cd $BUILD_DIR
wget http://www.tortall.net/projects/yasm/releases/yasm-0.8.0.tar.gz
tar xvfz yasm-0.8.0.tar.gz
cd yasm-0.8.0/
./configure --prefix=/usr/local
make && make install

wget http://ftp.gnome.org/pub/GNOME/sources/pygobject/2.16/pygobject-2.16.1.tar.gz
tar xzvf pygobject-2.16.1.tar.gz
cd pygobject-2.16.1/
./configure
make && make install

cd $BUILD_DIR
wget http://ftp.novell.com/pub/mono/sources/mono/mono-2.6.3.tar.bz2
tar xf mono-2.6.3.tar.bz2
cd mono-2.6.3/
./configure --with-libgdiplus=no
make && make install
sudo ln -sf /usr/local/bin/mono /usr/bin/cli

apt-get remove libpixman-1-dev libcairo2-dev

