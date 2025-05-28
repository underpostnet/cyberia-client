# cyberia-client

Core application that connects to the **Cyberia online MMO instance**. It uses `websockets` for real-time communication and `raylib` for rendering dynamic game grid instances.

## Getting Started

To run `cyberia-client`, you'll need Python 3.8 or newer.

### Installation

It's recommended to use a **Python virtual environment** to manage dependencies.

1.  **Create and activate your virtual environment:**

    ```bash
    pip install virtualenv
    virtualenv myenv --python=python3 # Use 'python3' for Python 3.x
    # On Windows:
    .\myenv\Scripts\activate
    # On Linux/macOS:
    source myenv/bin/activate
    ```

2.  **Install the required libraries:**

    ```bash
    pip install raylib-py==5.0.0 # For rendering
    pip install websocket-client # For WebSocket communication
    ```

---

### Running the Client

Once you've installed the dependencies and activated your virtual environment:

```bash
python main_mmo_client.py
```
