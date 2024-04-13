FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y build-essential
COPY . /src
WORKDIR /src
RUN ./configure --extra-ldflags=-static
RUN make -j$(nproc)
RUN make DESTDIR=/src/out install

FROM scratch
COPY --from=build /src/out/usr /usr
COPY syscall.h /usr/include/syscall.h
COPY hello.c /usr/bin/hello
CMD ["/usr/bin/hello"]
