#!/bin/bash

# Update system
sudo dnf install -y epel-release

# Development tools and dependencies
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y cmake make gcc-c++ pkgconfig git wget unzip python3 python3-pip

# Install Python 3.11+ (required by latest Emscripten SDK)
sudo dnf install -y python3.11

# Graphics and audio libraries
sudo dnf install -y \
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
  glfw-devel

# Additional dependencies
sudo dnf install -y libatomic.x86_64

cd $HOME

if [ -d ".emsdk" ]; then
  echo "Directory '.emsdk' already exists, pulling latest changes..."
  cd .emsdk && git pull && cd $HOME
else
  git clone https://github.com/emscripten-core/emsdk.git .emsdk
fi

# Add Emscripten environment and EMSDK_PYTHON to bashrc (if not already present)
grep -qxF 'export EMSDK_PYTHON=$(which python3.11)' ~/.bashrc || echo 'export EMSDK_PYTHON=$(which python3.11)' >> ~/.bashrc
grep -qF 'source "$HOME/.emsdk/emsdk_env.sh"' ~/.bashrc || echo 'source "$HOME/.emsdk/emsdk_env.sh" 2>/dev/null' >> ~/.bashrc
