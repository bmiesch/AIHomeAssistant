# Use the latest Debian arm64 image as base
FROM arm64v8/debian:bookworm

# Install dependencies
RUN apt-get update && apt-get install -y \
    ccache \
    g++ \
    make \
    libpocketsphinx-dev \
    libsphinxbase-dev \
    libasound2-dev \
    git \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# It needs to be built from source
RUN git clone https://github.com/simpleble/simpleble.git
WORKDIR /app
RUN cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release --target all -v
RUN cmake --install build --config Release

# Set up working directory
WORKDIR /app

# Copy your source files
COPY src/ /app/src/
COPY inc/ /app/inc/
COPY Makefile /app/

# Set up ccache
ENV CCACHE_DIR=/ccache
RUN mkdir -p /ccache

# Set compiler to use ccache
ENV CXX="ccache g++"

# Compile the project
RUN make

# Set the default command to run your application
CMD ["./start"]