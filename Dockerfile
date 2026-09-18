FROM devkitpro/devkitarm:20260221

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITARM=/opt/devkitpro/devkitARM

ENV PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitARM/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

ENV LANG=C.UTF-8
ENV TZ=UTC

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        git \
        python3 \
        ffmpeg \
        ca-certificates \
        file \
    && rm -rf /var/lib/apt/lists/*

RUN dkp-pacman -Syu --noconfirm

RUN dkp-pacman -S --needed --noconfirm \
    libnds \
    maxmod-nds \
    libfilesystem \
    libfat-nds \
    nds-dev

WORKDIR /spiderman-snake

CMD ["make", "-j4"]