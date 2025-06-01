ARG TYPE="release"
ARG SIZE="UINT16"

FROM ghcr.io/throttr/builder-alpine:1.87.0-${TYPE} AS builder

ARG TYPE
ARG SIZE

RUN --mount=type=secret,id=SENTRY_DSN \
    export SENTRY_DSN=$(cat /run/secrets/SENTRY_DSN) && \
    echo "$SENTRY_DSN" > /etc/sentry_dsn

COPY src/ src/
COPY tests/ tests/
COPY CMakeLists.txt .
COPY main.cpp .

RUN mkdir -p build && \
    cd build && \
    if [ "$TYPE" = "debug" ]; then BUILD_TYPE="Debug"; else BUILD_TYPE="Release"; fi && \
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_TESTS=ON -DRUNTIME_VALUE_SIZE="$SIZE" -DENABLE_STATIC_LINKING=ON && \
    curl -sL https://sentry.io/get-cli/ | bash && \
    make -j32 && \
    --mount=type=secret,id=SENTRY_TOKEN SENTRY_AUTH_TOKEN=$(cat /run/secrets/SENTRY_TOKEN) sentry-cli upload-dif --org throttr --project native . && \
    mv throttr /usr/bin/throttr && \
    ctest --output-on-failure -V --parallel 4 && \
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