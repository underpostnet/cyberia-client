import pyray as pr


class Util:
    def __init__(self):
        pass

    def color_from_payload(self, cdict):
        try:
            r = int(cdict.get("r", 255))
            g = int(cdict.get("g", 255))
            b = int(cdict.get("b", 255))
            a = int(cdict.get("a", 255))
            return pr.Color(r, g, b, a)
        except Exception:
            return pr.Color(255, 255, 255, 255)
