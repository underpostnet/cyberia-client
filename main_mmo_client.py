import argparse
import logging

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# Import configuration settings
from config import (
    SERVER_HOST,
    SERVER_PORT,
    WEBSOCKET_PATH,
)

# Import the refactored MMO client
from client.mmo_client import MmoClient


# --- Main Execution ---
if __name__ == "__main__":
    # Set up argument parser for command-line arguments
    parser = argparse.ArgumentParser(
        description="Python MMO Instance Client with Plain WebSockets and Raylib."
    )
    # Add argument for server host, with a default value from config
    parser.add_argument(
        "--host",
        type=str,
        default=SERVER_HOST,
        help=f"Server host address (default: {SERVER_HOST}).",
    )
    # Add argument for server port, with a default value from config
    parser.add_argument(
        "--port",
        type=int,
        default=SERVER_PORT,
        help=f"Server port (default: {SERVER_PORT}).",
    )

    # Parse the command-line arguments
    args = parser.parse_args()

    # Create an instance of the MmoClient
    # The client handles its own Raylib initialization and WebSocket connection
    client = MmoClient(args.host, args.port, WEBSOCKET_PATH)

    # Run the client's main loop
    client.run()
