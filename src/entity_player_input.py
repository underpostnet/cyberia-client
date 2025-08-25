import json
import websocket


class EntityPlayerInput:
    def __init__(self, game_state):
        self.game_state = game_state

    def send_player_action(self, ws, target_x, target_y):
        if ws and ws.sock and ws.sock.connected:
            try:
                action_message = {
                    "type": "player_action",
                    "payload": {"targetX": target_x, "targetY": target_y},
                }
                ws.send(json.dumps(action_message))
                self.game_state.upload_size_bytes += len(json.dumps(action_message))
            except websocket.WebSocketConnectionClosedException:
                print("Cannot send message, connection is closed.")
            except Exception as e:
                print(f"Error sending message: {e}")
