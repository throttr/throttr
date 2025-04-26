ARG TYPE="release"

FROM ghcr.io/throttr/builder:1.87.0-${TYPE}

ARG TYPE

COPY src/ src/
COPY tests/ tests/
COPY CMakeLists.txt .
COPY main.cpp .
COPY LICENSE .

EXPOSE 9000

ENV THREADS=1

RUN mkdir -p build && \
    cd build && \
    if [ "$TYPE" = "debug" ]; then BUILD_TYPE="Debug";  else BUILD_TYPE="Release"; fi && \
    if [ "$TYPE" = "debug" ]; then BUILD_TESTS="ON";  else BUILD_TESTS="OFF"; fi && \
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTS=$BUILD_TESTS  && \
    make -j4 && \
    mv /srv/build/throttr /usr/bin/throttr && \
    if [ "$TYPE" = "debug" ]; then mv /srv/build/tests /usr/bin/tests; fi && \
    cd .. && \
    rm -rf /srv/*

RUN adduser --system --no-create-home --shell /bin/false gatekeeper
USER gatekeeper

CMD ["sh", "-c", "throttr --port=9000 --threads=$THREADS"]