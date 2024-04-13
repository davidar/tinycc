FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y build-essential
COPY . /src
WORKDIR /src
RUN ./configure --extra-ldflags=-static
RUN make -j$(nproc)
RUN make DESTDIR=/src/out install

FROM scratch
ADD https://github.com/davidar/dietlibc.git#tcc /src/dietlibc
# COPY --from=build /src/out/usr /usr
COPY --from=build /src/tcc /usr/local/bin/tcc
COPY lib/libtcc1.c /usr/lib/libtcc1.c
COPY syscall.h /usr/include/syscall.h
COPY hello.c /usr/bin/hello
COPY hello2.c /usr/bin/hello2
CMD ["/usr/bin/hello2"]
