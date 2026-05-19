# Default to linux/amd64; override at build time with --build-arg BUILD_PLATFORM=...
ARG BUILD_PLATFORM=linux/amd64
FROM --platform=${BUILD_PLATFORM} ubuntu:22.04

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# DEBIAN_FRONTEND prevents apt-get from prompting; TZ prevents tzdata from prompting.
ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Brussels

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    bison flex gettext texinfo libncurses5-dev locales \
    libncursesw5-dev gperf automake libtool pkg-config \
    build-essential genromfs libgmp-dev libmpc-dev \
    libmpfr-dev libisl-dev binutils-dev libelf-dev git \
    libexpat-dev gcc-multilib g++-multilib picocom \
    u-boot-tools util-linux kconfig-frontends sudo \
    gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    python3 python3-distutils python3-pip python3-apt \
    xxd srecord sed clang-15 zip \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

# Create dev user with sudo access (password "dev") and set up build system symlinks.
# /home/dev is made world-writable so mapped host UIDs (via --user flag) can write to it.
RUN groupadd -g 1000 dev \
    && useradd -u 1000 -g dev -d /home/dev dev \
    && mkdir /home/dev \
    && chown -R dev:dev /home/dev \
    && chmod 777 /home/dev \
    && usermod -aG sudo dev \
    && echo 'dev:dev' | chpasswd \
    && ln -s /usr/bin/clang-15 /usr/bin/clang \
    && ln -s /usr/bin/sed /usr/local/bin/gsed \
    && ln -s /usr/bin/python3 /usr/bin/python

RUN pip3 install --no-cache-dir jinja2

ENV LANG=en_US.UTF-8

USER dev

WORKDIR /home/dev
