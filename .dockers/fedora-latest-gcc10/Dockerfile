FROM fedora:32

RUN dnf update -y
RUN dnf -y groupinstall "Development Tools"
RUN dnf install -y cmake openssl-devel clang libcxx-devel libcxxabi-devel bzip2-devel which zlib-devel

WORKDIR /devel
RUN git clone -b develop https://github.com/boostorg/boost.git
WORKDIR /devel/boost
RUN git submodule update --init --recursive
RUN cd libs/beast && git checkout develop && git pull origin develop
RUN ./bootstrap.sh
RUN ./b2 toolset=gcc variant=release cxxstd=2a headers
RUN ./b2 toolset=gcc,clang variant=release cxxstd=2a,17,14,11 -j`grep processor /proc/cpuinfo | wc -l` libs/beast//test
