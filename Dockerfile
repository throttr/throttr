ARG TYPE="release"

FROM ghcr.io/throttr/builder-alpine:1.87.0-${TYPE} AS builder

ARG TYPE

COPY src/ src/
COPY tests/ tests/
COPY CMakeLists.txt .
COPY main.cpp .

RUN mkdir -p build && \
    cd build && \
    if [ "$TYPE" = "debug" ]; then BUILD_TYPE="Debug"; else BUILD_TYPE="Release"; fi && \
    if [ "$TYPE" = "debug" ]; then BUILD_TESTS="ON"; else BUILD_TESTS="OFF"; fi && \
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_TESTS="$BUILD_TESTS" && \
    make -j4 && \
    strip --strip-all throttr  && \
    mv throttr /usr/bin/throttr && \
    if [ "$TYPE" = "debug" ]; then mv tests /usr/bin/tests; fi

FROM scratch

COPY --from=builder /usr/bin/throttr /usr/bin/throttr
COPY /LICENSE /LICENSE

EXPOSE 9000

USER gatekeeper

ENV THREADS=1

CMD ["throttr", "--port=9000", "--threads=1"]