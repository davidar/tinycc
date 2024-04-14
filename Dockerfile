FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y build-essential

FROM build AS tcc
COPY . /src
WORKDIR /src
RUN ./configure --extra-ldflags=-static
RUN make -j$(nproc)
RUN make DESTDIR=/src/out install

FROM build as musl
COPY --from=tcc /src/out/usr /usr
ADD https://github.com/davidar/musl.git#tcc /src/musl
WORKDIR /src/musl
RUN ./configure --target=x86_64 CC='tcc' AR='tcc -ar' RANLIB='echo' LIBCC='/usr/local/lib/tcc/libtcc1.a'
RUN make -j$(nproc)
RUN make DESTDIR=/src/out install


FROM scratch
# ADD https://github.com/davidar/dietlibc.git#tcc /src/dietlibc
COPY --from=tcc /src/out/usr /usr
COPY --from=musl /src/out/usr /usr
COPY ldscript.ld /usr/lib/libc.so
COPY hello4.c /usr/bin/hello
CMD ["/usr/bin/hello"]
