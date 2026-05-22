FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    upx-ucl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY . .

RUN mkdir build_release && cd build_release && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_C_FLAGS="-O2 -DNDEBUG" \
          -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG -static-libgcc -static-libstdc++" \
          -DCMAKE_EXE_LINKER_FLAGS="-s" \
          .. && \
    make -j$(nproc)

RUN cd build_release && \
    strip TextExtractionCLI/TextExtraction && \
    upx --best TextExtractionCLI/TextExtraction || true && \
    cp TextExtractionCLI/TextExtraction /TextExtraction-linux-x64

FROM scratch
COPY --from=builder /TextExtraction-linux-x64 /TextExtraction-linux-x64
