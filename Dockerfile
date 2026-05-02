# --- Build Image
FROM rockylinux/rockylinux:10 AS builder

# System packages (build prerequisites)
RUN dnf -y update && \
    dnf -y install epel-release && \
    dnf -y install --allowerasing \
        bzip2 \
        curl \
        git \
        wget \
        openssl-devel \
        perl \
        unzip

# System packages (raylib gfx + build dependencies)
RUN dnf groupinstall -y "Development Tools" && \
    dnf install -y \
        cmake \
        python3 \
        alsa-lib-devel \
        mesa-libGL-devel \
        mesa-libGLU-devel \
        libX11-devel \
        libXrandr-devel \
        libXi-devel \
        libXcursor-devel \
        libXinerama-devel \
        libXfixes-devel \
        freeglut-devel \
        glfw-devel \
        libatomic.x86_64

ENV EMSDK=/opt/emsdk
ENV PATH="${EMSDK}:${EMSDK}/upstream/emscripten:${PATH}"

WORKDIR /
RUN git clone https://github.com/emscripten-core/emsdk.git ${EMSDK}
WORKDIR ${EMSDK}
RUN ./emsdk install latest && \
    ./emsdk activate latest

WORKDIR /
COPY . /cyberia-client/

# Compile both build modes at image build-time to minimize service startup time.
WORKDIR /cyberia-client
RUN make -f Web.mk all BUILD_MODE=RELEASE
RUN make -f Web.mk all BUILD_MODE=DEBUG

# --- Runtime Image
FROM rockylinux/rockylinux:9-minimal AS runtime

RUN microdnf -y install python3 && microdnf clean all

WORKDIR /home/dd/engine/cyberia-client

COPY --from=builder /cyberia-client/server.py          ./server.py
COPY --from=builder /cyberia-client/bin/web/release    ./bin/web/release
COPY --from=builder /cyberia-client/bin/web/debug      ./bin/web/debug

ENV CYBERIA_PORT=8081
ENV CYBERIA_WEB_ROOT=/home/dd/engine/cyberia-client/bin/web/release
ENV CYBERIA_MODE=production

EXPOSE 8081 8082

CMD ["sh", "-c", "python3 /home/dd/engine/cyberia-client/server.py ${CYBERIA_PORT} ${CYBERIA_WEB_ROOT} ${CYBERIA_MODE}"]
