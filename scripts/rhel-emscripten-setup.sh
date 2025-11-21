#!/bin/bash

# Update system
sudo dnf install -y epel-release

# Development tools and dependencies
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y cmake make gcc-c++ pkgconfig git wget unzip python3 python3-pip

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

git clone https://github.com/emscripten-core/emsdk.git .emsdk

# Add Emscripten environment to bashrc
echo 'source "$HOME/.emsdk/emsdk_env.sh" 2>/dev/null' >> ~/.bashrc
