FROM debian:jessie

RUN apt-get update && apt-get install -y \
    git \
    libxml2-dev \
    python \
    build-essential \
    make \
    gcc \
    python-dev \
    locales \
    python-pip \
    dh-autoreconf \
    libtool-bin \
    valgrind

RUN dpkg-reconfigure locales && \
    locale-gen C.UTF-8 && \
    /usr/sbin/update-locale LANG=C.UTF-8

ENV LC_ALL C.UTF-8

RUN mkdir -p /usr/src/app
WORKDIR /usr/src/app

RUN git clone https://github.com/robertpostill/libcsv.git 
WORKDIR /usr/src/app/libcsv
RUN git checkout a971ac0fff32afb4d57ff399e7e662057b5016e2
RUN /bin/sh autogen.sh
RUN ./configure
RUN make
RUN make check
RUN make install

ENV LD_LIBRARY_PATH /usr/local/lib

WORKDIR /usr/src/app
COPY . /usr/src/app
RUN ./autogen.sh
RUN ./configure
RUN make
RUN make check TESTS='test_readstat' || /bin/sh -c 'cat ./test-suite.log; exit 1'
RUN make check-valgrind TESTS='test_readstat' || /bin/sh -c 'cat ./test-suite.log; exit 1'
