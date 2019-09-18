FROM stronglytyped/arm-none-eabi-gcc as builder

RUN apt-get install -y ccache autoconf automake libtool sed python-minimal

COPY tools /work/tools
COPY build-tools.sh /work/build-tools.sh
RUN /work/build-tools.sh --verbose

COPY mono /work/mono
COPY build-mono.sh /work/build-mono.sh

COPY apps /work/apps
COPY nuttx /work/nuttx
COPY build.sh /work/build-nuttx.sh

RUN /work/build-nuttx.sh --verbose --clean --configure
RUN /work/build-mono.sh --verbose --force
RUN /work/build-nuttx.sh --verbose --clean

FROM scratch as mono
COPY --from=builder /work/mono/libs libs

FROM scratch as nuttx
COPY --from=builder /work/nuttx/nuttx.bin .
COPY --from=builder /work/nuttx/nuttx_user.bin .
