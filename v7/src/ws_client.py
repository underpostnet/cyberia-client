



class WSClient:
    def __init__(self):
        self.ws = None
        self.ws_thread = None
        self.is_running = True