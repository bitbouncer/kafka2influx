Platforms: Windows / Linux 

Imports graphite encoded metrics (from collectd) from kafka (v0.82+) to a database in influxdb. v0.9+
```
kafka-graphite2influx --topic collectd.graphite --broker f013-520-kafka --influxdb 10.1.47.16:8086 --template "hostgroup.host...resource.measurement*" --database metrics
```

Imports influxdb encoded metrics from kafka influxdb. REQUIRES kafka v0.10.1+ 
```
kspp-metrics2influx --topic kspp_metrics --broker f013-520-kafka --influxdb 10.1.47.16:8086 --database kspp_metrics
```

## Ubuntu 16 x64:

Install build tools
```
sudo apt-get install -y automake autogen shtool libtool git wget cmake unzip build-essential libboost-all-dev g++ python-dev autotools-dev libicu-dev zlib1g-dev openssl libssl-dev libbz2-dev libsnappy-dev libcurl-dev

```
Get the source
```
git clone https://github.com/bitbouncer/csi-kafka.git
git clone https://github.com/bitbouncer/csi-async.git
git clone https://github.com/bitbouncer/csi-hcl-asio.git
git clone https://github.com/bitbouncer/kafka2influx.git
```

Build
```
cd csi-kafka
bash rebuild_linux.sh
cd ..

cd kafka2influx
bash rebuild_linux.sh
cd ..

```

## Centos 7

Install build tools
```
yum -y update
yum -y groupinstall 'Development Tools'
yum -y install automake autogen libtool git wget cmake unzip openssl redhat-lsb-core postgresql-devel openssl-devel bzip2-devel openldap  openldap-clients openldap-devel libidn-devel curl curl-devel
```
Get the source
```
export BOOST_VERSION=1_62_0
export BOOST_VERSION_DOTTED=1.62.0

wget http://sourceforge.net/projects/boost/files/boost/$BOOST_VERSION_DOTTED/boost_$BOOST_VERSION.tar.gz/download -Oboost_$BOOST_VERSION.tar.gz
tar xf boost_$BOOST_VERSION.tar.gz
rm -f boost_$BOOST_VERSION.tar.gz
mv boost_$BOOST_VERSION boost

git clone https://github.com/bitbouncer/csi-kafka.git
git clone https://github.com/bitbouncer/csi-async.git
git clone https://github.com/bitbouncer/csi-hcl-asio.git
git clone https://github.com/bitbouncer/kafka2influx.git
```

Build
```
cd boost
./bootstrap.sh
./b2 -j 4
cd ..

cd csi-kafka
bash rebuild_linux.sh
cd ..

cd kafka2influx
bash rebuild_linux.sh
cd ..
```


## MacOS X

Install build tools (using Homebrew)
```
# Install Xcode
xcode-select --install
brew install cmake
brew install boost
brew install boost-python
```

Check out source code
```
git clone https://github.com/bitbouncer/csi-kafka.git
git clone https://github.com/bitbouncer/csi-async.git
git clone https://github.com/bitbouncer/csi-hcl-asio.git
git clone https://github.com/bitbouncer/kafka2influx.git
```

Run the build
```
./rebuild_macos.sh
```

## Windows x64:

Install build tools
```
- CMake (https://cmake.org/)
- Visual Studio 14 (https://www.visualstudio.com/downloads/)
- nasm (https://sourceforge.net/projects/nasm/)
- perl (http://www.activestate.com/activeperl)
```
Build
```
wget --no-check-certificate http://downloads.sourceforge.net/project/boost/boost/1.62.0/boost_1_62_0.zip
unzip boost_1_62_0.zip
rename boost_1_62_0 boost

git clone https://github.com/madler/zlib.git
git clone https://github.com/lz4/lz4.git
git clone https://github.com/curl/curl.git
git clone https://github.com/bitbouncer/csi-kafka.git
git clone https://github.com/bitbouncer/csi-async.git
git clone https://github.com/bitbouncer/csi-hcl-asio.git
git clone https://github.com/bitbouncer/kafka2influx.git


set VISUALSTUDIO_VERSION_MAJOR=14
call "C:\Program Files (x86)\Microsoft Visual Studio %VISUALSTUDIO_VERSION_MAJOR%.0\VC\vcvarsall.bat" amd64

cd zlib
mkdir build & cd build
cmake -G "Visual Studio 14 Win64" ..
msbuild zlib.sln
msbuild zlib.sln /p:Configuration=Release
cd ../..

cd boost
call bootstrap.bat
.\b2.exe -toolset=msvc-%VisualStudioVersion% variant=release,debug link=static address-model=64 architecture=x86 --stagedir=stage\lib\x64 stage -s ZLIB_SOURCE=%CD%\..\zlib headers log_setup log date_time timer thread system program_options filesystem regex chrono
cd ..

cd curl
git checkout curl-7_51_0
rmdir /s /q builds
rm  lib\x64\Debug\libcurl.lib
rm  lib\x64\Release\libcurl.lib
cd winbuild
nmake /f makefile.vc mode=static VC=%VISUALSTUDIO_VERSION_MAJOR% ENABLE_SSPI=yes ENABLE_WINSSL=yes ENABLE_IDN=no DEBUG=yes MACHINE=x64
nmake /f makefile.vc mode=static VC=%VISUALSTUDIO_VERSION_MAJOR% ENABLE_SSPI=yes ENABLE_WINSSL=yes ENABLE_IDN=no DEBUG=no MACHINE=x64
cd ..
echo CURL COPYING LIBS
mkdir lib\x64
mkdir lib\x64\Debug
mkdir lib\x64\Release
copy builds\libcurl-vc%VISUALSTUDIO_VERSION_MAJOR%-x64-debug-static-ipv6-sspi-winssl\lib\libcurl_a_debug.lib  lib\x64\Debug\libcurl.lib
copy builds\libcurl-vc%VISUALSTUDIO_VERSION_MAJOR%-x64-release-static-ipv6-sspi-winssl\lib\libcurl_a.lib lib\x64\Release\libcurl.lib
cd ..

cd csi-kafka
call rebuild_win64_vc14.bat
cd ..

cd kafka2influx
call rebuild_windows_vs14.bat
cd ..

```

