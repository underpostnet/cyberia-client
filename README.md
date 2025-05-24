## cyberia mmorpg raylib client

### venv installation

```bash
pip install virtualenv
virtualenv myenv --python=3
myenv/bin/activate # windows
source myenv/bin/activate # linux
pip install raylib-py==5.0.0
pip install python-socketio eventlet requests
```

### pyenv installation

```bash
sudo yum groupinstall "Development Tools"

sudo yum install \
  libffi-devel \
  zlib-devel \
  bzip2-devel \
  readline-devel \
  sqlite-devel \
  openssl-devel \
  xz-devel \
  tk-devel

pyenv install 3.13.3
pyenv rehash
pyenv local 3.13.3
pip install raylib-py==5.0.0
pip install python-socketio eventlet requests
```

### usage

```bash
python index.py
```
