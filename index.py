import argparse
import logging

# --- Logging Configuration ---
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

from config import (
    SERVER_HOST,
    SERVER_PORT,
    WEBSOCKET_PATH,
)

from instance_client import InstanceClient

# --- Main Execution ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Python MMO Instance Client with Plain WebSockets and Raylib."
    )
    parser.add_argument(
        "--host",
        type=str,
        default=SERVER_HOST,
        help=f"Server host address (default: {SERVER_HOST}).",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=SERVER_PORT,
        help=f"Server port (default: {SERVER_PORT}).",
    )

    args = parser.parse_args()

    client = InstanceClient(args.host, args.port, WEBSOCKET_PATH)
    client.run()
