ARG TYPE="release"

FROM ghcr.io/throttr/builder-alpine:1.87.0-${TYPE}

ARG TYPE

COPY src/ src/
COPY tests/ tests/
COPY CMakeLists.txt .
COPY main.cpp .

EXPOSE 9000

ENV THREADS=1

RUN mkdir -p build && \
    cd build && \
    if [ "$TYPE" = "debug" ]; then BUILD_TYPE="Debug"; else BUILD_TYPE="Release"; fi && \
    if [ "$TYPE" = "debug" ]; then BUILD_TESTS="ON"; else BUILD_TESTS="OFF"; fi && \
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_TESTS="$BUILD_TESTS" && \
    make -j4 && \
    mv throttr /usr/bin/throttr && \
    if [ "$TYPE" = "debug" ]; then mv tests /usr/bin/tests; fi && \
    cd /srv && \
    rm -rf ./* && \
    adduser --system --no-create-home --shell /bin/false gatekeeper && \
    ldd /usr/bin/throttr || echo "Binary is statically linked"

COPY LICENSE .

USER gatekeeper

CMD ["sh", "-c", "throttr --port=9000 --threads=$THREADS"]