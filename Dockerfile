ARG TYPE="release"
ARG SIZE="UINT16"

FROM ghcr.io/throttr/builder-alpine:1.87.0-${TYPE} AS builder

ARG TYPE
ARG SIZE

COPY src/ src/
COPY tests/ tests/
COPY CMakeLists.txt .
COPY main.cpp .

RUN mkdir -p build && \
    cd build && \
    if [ "$TYPE" = "debug" ]; then BUILD_TYPE="Debug"; else BUILD_TYPE="Release"; fi && \
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_TESTS=ON -DRUNTIME_VALUE_SIZE="$SIZE" && \
    make -j4 && \
    strip --strip-all throttr  && \
    mv throttr /usr/bin/throttr && \
    ctest && \
    adduser --system --no-create-home --shell /bin/false throttr

FROM scratch

COPY --from=builder /usr/bin/throttr /usr/bin/throttr
COPY /LICENSE /LICENSE
COPY --from=builder /etc/passwd /etc/passwd
COPY --from=builder /etc/group /etc/group

USER throttr

EXPOSE 9000

ENV THREADS=1

CMD ["throttr"]