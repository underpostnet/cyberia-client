# cyberia-client

`cyberia-client` is a Python application that connects to the **Cyberia online MMO instance**. It uses **plain WebSockets** for real-time communication with the server and `raylibpy` for **rendering the game world**.

---

## Features

- **Real-time Communication:** Connects via WebSockets to send player actions and receive immediate game state updates.
- **Vector Graphics Rendering:** Utilizes `raylibpy` for **real-time 2D vector graphics rendering**, drawing game entities like players, obstacles, and paths.
- **Dynamic World Visualization:** Renders a **dynamic instance world**, including updating object positions and states received from the server.
- **Camera System:** Implements a **smooth camera system** that follows the player character, enhancing the visual experience.
- **Input Handling:** Processes **real-time mouse input** to interact with the game world (e.g., sending movement commands).
- **Optimized Performance:** Network communication runs in a **separate thread**, ensuring the rendering loop remains uninterrupted for smooth visual updates.

---

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
    pip install websocket-client # For plain WebSocket communication
    ```

---

### Running the Client

Once you've installed the dependencies and activated your virtual environment:

```bash
python index.py
```
