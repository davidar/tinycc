FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y build-essential

COPY . /src/tcc
WORKDIR /src/tcc
RUN ./configure --extra-ldflags=-static
RUN make -j$(nproc)
RUN make install
RUN make DESTDIR=/dest install

ADD https://github.com/davidar/musl.git#tcc /src/musl
WORKDIR /src/musl
RUN ./configure --target=x86_64 CC='tcc' AR='tcc -ar' RANLIB='echo' LIBCC='/usr/local/lib/tcc/libtcc1.a'
RUN make -j$(nproc)
RUN make DESTDIR=/dest install

FROM scratch
COPY --from=build /dest/usr /usr
COPY ldscript.ld /usr/lib/libc.so
COPY hello4.c /usr/bin/hello
CMD ["/usr/bin/hello"]
