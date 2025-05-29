import argparse
import logging

from config import SERVER_HOST, SERVER_PORT, WEBSOCKET_PATH
from network_state.network_state_client import NetworkStateClient

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Python NetworkState Client with WebSockets and Raylib."
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

    client = NetworkStateClient(args.host, args.port, WEBSOCKET_PATH)

    client.run()
