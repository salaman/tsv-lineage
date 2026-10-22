FROM ubuntu:24.04

ARG BUILDER_UID=1000
ARG BUILDER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       bc bison build-essential ca-certificates ccache curl dpkg-dev \
       e2fsprogs erofs-utils file flex g++-multilib gcc-multilib git git-lfs gnupg \
       gperf imagemagick kmod lib32readline-dev lib32z1-dev libdw-dev \
       libelf-dev libgnutls28-dev libsdl1.2-dev libssl-dev libxml2 \
       libxml2-utils lz4 lzop mtools ninja-build patch pngcrush procps protobuf-compiler \
       python3 python3-mako python3-packaging python3-protobuf python3-yaml \
       quilt repo rsync schedtool shellcheck squashfs-tools time unzip \
       xz-utils xsltproc xxd zip zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN test "${BUILDER_UID}" -gt 0 \
    && test "${BUILDER_GID}" -gt 0 \
    && (getent group "${BUILDER_GID}" || groupadd --gid "${BUILDER_GID}" builder) \
    && if getent passwd "${BUILDER_UID}" >/dev/null; then \
         existing=$(getent passwd "${BUILDER_UID}" | cut -d: -f1); \
         usermod --login builder --home /home/builder --shell /bin/bash "$existing"; \
       else \
         useradd --uid "${BUILDER_UID}" --gid "${BUILDER_GID}" \
             --home-dir /home/builder --shell /bin/bash builder; \
       fi \
    && usermod --gid "${BUILDER_GID}" builder \
    && mkdir -p /android /cache /home/builder \
    && chown builder:"${BUILDER_GID}" /android /cache /home/builder

ENV HOME=/home/builder \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    CCACHE_DIR=/cache/ccache \
    CCACHE_MAXSIZE=10G \
    GIT_TERMINAL_PROMPT=0

USER builder
WORKDIR /android
CMD ["/bin/bash"]
