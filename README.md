<p align="center">
  <img src="https://www.cyberiaonline.com/assets/splash/apple-touch-icon-precomposed.png" alt="CYBERIA online"/>
</p>

<div align="center">

<h1>cyberia client</h1>

</div>

Core application that connects to the **Cyberia online MMO instance**. It uses `websockets` for real-time communication and `raylib` for rendering dynamic game grid instances.

### Key Features

- **Real-time Multiplayer**: Connects to the Cyberia game server for a live multiplayer experience.
- **Dynamic 2D Rendering**: Utilizes `pyray` (Python bindings for raylib) for efficient, hardware-accelerated graphics.
- **Object Layer System**: A flexible system for rendering complex, multi-layered entities (players, bots, items).
- **Interactive HUD**: In-game Heads-Up Display for managing inventory and character stats.
- **Built-in Asset Development Suite**:
  - **Object Layer Viewer**: An interactive tool to inspect, edit, and test object layer animations in real-time. Features include live pixel editing, frame-by-frame analysis, and color palette manipulation.
  - **Synthetic Data Generator**: A powerful script-based tool for programmatically generating complex sprites and animations using parametric curves and procedural patterns.

### Prerequisites

- Python 3.9 or higher
- `pip` (Python package installer)
- A virtual environment tool (e.g., `venv`) is highly recommended.

### Installation

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/underpostnet/cyberia-client.git
    cd cyberia-client
    ```

2.  **Create and activate a virtual environment:**

    ```bash
    # For Unix/macOS
    python -m venv env
    source env/bin/activate

    # For Windows
    python -m venv env
    .\env\Scripts\activate
    ```

3.  **Install the required dependencies:**
    ```bash
    pip install -r requirements.txt
    ```

### Running the Client

```bash
python main.py
```
