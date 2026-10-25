FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
COPY ubuntu.sources.list /etc/apt/sources.list
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates cmake g++ make pkg-config python3 git curl file \
    qt6-base-dev qt6-base-dev-tools libqt6svg6-dev \
    libgl1-mesa-dri mesa-utils xvfb xauth \
    fonts-liberation2 fonts-noto-cjk fontconfig cabextract \
    patchelf squashfs-tools desktop-file-utils dpkg-dev xz-utils

ENV QT_QPA_PLATFORM=xcb
RUN apt-get install -y --no-install-recommends libqt6opengl6-dev libgl-dev libopengl-dev
RUN apt-get install -y --only-upgrade libsystemd0 liblzma5
WORKDIR /out
