FROM ubuntu:22.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libpqxx-dev \
    librabbitmq-dev \
    libssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

# build SimpleAmqpClient in /usr/local
RUN git clone https://github.com/alanxz/SimpleAmqpClient.git && \
    cd SimpleAmqpClient && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_SSL_SUPPORT=OFF && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf SimpleAmqpClient

WORKDIR /app

# -----------------------------------------------------------------------------
# BUILDER
# -----------------------------------------------------------------------------
FROM base AS builder

COPY CMakeLists.txt .
COPY common/ common/
COPY services/ services/

WORKDIR /app/build
RUN cmake .. && make -j$(nproc)

# -----------------------------------------------------------------------------
# PAYMENT SERVICE
# -----------------------------------------------------------------------------
FROM ubuntu:22.04 AS payment-service

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libpqxx-6.4 \
    librabbitmq4 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    libboost-date-time1.74.0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=base /usr/local/lib/libSimpleAmqpClient* /usr/local/lib/

RUN ldconfig

COPY --from=builder /app/build/services/payment-service/payment_service_exe ./app

CMD ["./app"]

# -----------------------------------------------------------------------------
# ORDER SERVICE
# -----------------------------------------------------------------------------
FROM ubuntu:22.04 AS order-service

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libpqxx-6.4 \
    librabbitmq4 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    libboost-date-time1.74.0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=base /usr/local/lib/libSimpleAmqpClient* /usr/local/lib/

RUN ldconfig

COPY --from=builder /app/build/services/order-service/order_service_exe ./app

CMD ["sh", "-c", "ldd ./app && ./app"]