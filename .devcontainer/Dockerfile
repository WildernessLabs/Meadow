FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    bison flex gettext texinfo libncurses5-dev locales \
    libncursesw5-dev gperf automake libtool pkg-config \
    build-essential genromfs libgmp-dev libmpc-dev \
    libmpfr-dev libisl-dev binutils-dev libelf-dev git \
    libexpat-dev gcc-multilib g++-multilib picocom \
    u-boot-tools util-linux kconfig-frontends sudo \
    gcc-arm-none-eabi binutils-arm-none-eabi \
    python3.10 python3-distutils python3-pip python3-apt \
    xxd srecord sed clang-15 zip ccache autoconf libtool \
    cmake libusb-1.0-0-dev automake dfu-util srecord

# VS Code needs these
RUN apt-get install -y curl wget ca-certificates \
    openssh-client gnupg2

RUN apt-get install -y bash-completion
ENV TERM=xterm-256color

# Create a non-root user (VS Code strongly prefers this)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER $USERNAME

SHELL ["/bin/bash", "-c"]
