# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    cmake \
    libasio-dev \
    nlohmann-json3-dev \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Crow C++ header
RUN git clone --branch v1.3.4 https://github.com/CrowCpp/Crow.git /tmp/crow \
    && cp -r /tmp/crow/include/* /usr/local/include/ \
    && rm -rf /tmp/crow

WORKDIR /app
COPY . .

# Compile taskflow for Linux (passing -I/usr/local/include and -lpthread)
RUN clang++ -std=c++17 -O2 -Wall -Wextra \
    -I/usr/include -I/usr/local/include \
    -o taskflow main.cpp -lpthread

# Stage 2: Minimal runtime environment
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary and web assets from the builder stage
COPY --from=builder /app/taskflow /app/taskflow
COPY --from=builder /app/index.html /app/index.html
COPY --from=builder /app/style.css /app/style.css
COPY --from=builder /app/data /app/data

# Ensure data directory exists
RUN mkdir -p /app/data

EXPOSE 8080

# Render provides the port in the $PORT environment variable; fallback to 8080
CMD ["sh", "-c", "./taskflow ${PORT:-8080}"]