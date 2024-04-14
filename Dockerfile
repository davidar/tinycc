FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y build-essential

FROM build AS tcc
COPY . /src
WORKDIR /src
RUN ./configure --extra-ldflags=-static
RUN make -j$(nproc)
RUN make DESTDIR=/src/out install

FROM build as musl
ADD https://github.com/davidar/musl.git#header-only /src/musl
WORKDIR /src/musl
RUN ./configure
RUN make obj/include/bits/alltypes.h obj/include/bits/syscall.h


FROM scratch
ADD https://github.com/davidar/dietlibc.git#tcc /src/dietlibc
# COPY --from=build /src/out/usr /usr
COPY --from=tcc /src/tcc /usr/local/bin/tcc
COPY --from=musl /src/musl /src/musl
COPY lib/*.c /usr/lib/
COPY syscall.h /usr/include/syscall.h
COPY hello.c /usr/bin/hello
COPY hello2.c /usr/bin/hello2
COPY hello3.c /usr/bin/hello3
CMD ["/usr/bin/hello3"]
