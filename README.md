# cyberia-client

Core application that connects to the **Cyberia online MMO instance**. It uses `websockets` for real-time communication and `raylib` for rendering dynamic game grid instances.

### Installation

Create env:

```bash
conda create --name raylib_env python=3.9
```

Activate env and install:

```bash
conda activate raylib_env
pip install -r requirements.txt
```

### Running the Client

```bash
python main_network_state_client.py
```
