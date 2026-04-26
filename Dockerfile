# ═══════════════════════════════════════════════════════════════════════════════
# Cyberia Client — build-environment image
#
# This image is intentionally abstract: it ships the full toolchain (EMSDK,
# raylib dependencies, Node.js, underpost CLI, Python) and the project source
# but does NOT compile anything at build time.
#
# The correct BUILD_MODE (RELEASE for production, DEBUG for development) is
# determined at container startup by the CMD in the Kubernetes manifest
# (conf.instances.json / deployment.yaml).  This keeps a single image tag
# usable for both environments without any distortion of the compiled output.
# ═══════════════════════════════════════════════════════════════════════════════
FROM rockylinux:9

# ── System packages (build tools + raylib web dependencies) ───────────────────
RUN dnf install -y epel-release && \
    curl -fsSL https://rpm.nodesource.com/setup_24.x | bash - && \
    dnf groupinstall -y "Development Tools" && \
    dnf install -y \
        cmake git wget unzip \
        python3 python3.11 \
        nodejs \
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
        libatomic.x86_64 && \
    dnf clean all

# ── underpost CLI ──────────────────────────────────────────────────────────────
RUN npm install -g underpost

# ── Emscripten SDK ─────────────────────────────────────────────────────────────
ENV EMSDK=/root/.emsdk
ENV EMSDK_QUIET=1
ENV EMSDK_PYTHON=/usr/bin/python3.11
ENV PATH="${EMSDK}:${EMSDK}/upstream/emscripten:${PATH}"

RUN git clone --depth=1 https://github.com/emscripten-core/emsdk.git ${EMSDK} && \
    cd ${EMSDK} && \
    ./emsdk install latest && \
    ./emsdk activate latest

# ── Project source ─────────────────────────────────────────────────────────────
# libs/raylib and libs/cJSON are git-subrepos already present in the tree.
WORKDIR /app
COPY . .

EXPOSE 8081 8082

# Default CMD: production build then serve.
# The Kubernetes manifest command overrides this for each environment.
CMD ["/bin/sh", "-c", "make -f Web.mk all BUILD_MODE=RELEASE && python3 server.py 8081 bin/web/release production"]
