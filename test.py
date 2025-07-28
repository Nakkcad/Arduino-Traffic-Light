from flask import Flask, render_template, jsonify, request
import time
from threading import Thread, Lock
from dataclasses import dataclass

app = Flask(__name__)

@dataclass
class PauseState:
    is_paused: bool = False
    pause_start_time: float = 0
    remaining_delay: float = 0
    current_phase: int = 0
    current_light: int = 0
    next_light: int = 1

class TrafficLightSimulator:
    def __init__(self):
        self.light_names = ["NORTH", "NE", "SE", "SW", "NW"]
        self.light_order = [0, 1, 2, 3, 4]
        self.light_delays = [
            [5000, 2000, 5000],  # NORTH
            [5000, 2000, 5000],  # SW
            [5000, 2000, 5000],  # SE
            [5000, 2000, 5000],  # NW
            [5000, 2000, 5000]   # NE
        ]
        self.light_states = [[True, False, False] for _ in range(5)]
        self.pause_state = PauseState()
        self._running = True
        self._lock = Lock()
        self.serial_log = []
        self.thread = Thread(target=self.run_cycle, daemon=True)
        self.thread.start()

    def stop(self):
        self._running = False
        self.thread.join()

    def set_order(self, new_order):
        with self._lock:
            if len(new_order) != 5 or sorted(new_order) != list(range(5)):
                return False
            self.light_order = new_order.copy()
            self.serial_log.append(f"ORDER SET: {','.join(self.light_names[i] for i in new_order)}")
            return True

    def set_delays(self, delays):
        with self._lock:
            if len(delays) != 15:
                return False
            for i in range(5):
                self.light_delays[i][0] = max(delays[i*3], 100)
                self.light_delays[i][1] = max(delays[i*3+1], 100)
                self.light_delays[i][2] = max(delays[i*3+2], 100)
            self.serial_log.append("DELAYS SET: " + str(self.light_delays))
            return True

    def pause(self):
        with self._lock:
            if not self.pause_state.is_paused:
                self.pause_state.is_paused = True
                self.pause_state.pause_start_time = time.time()
                self.serial_log.append("[System PAUSED]")

    def resume(self):
        with self._lock:
            if self.pause_state.is_paused:
                self.pause_state.is_paused = False
                paused_time = time.time() - self.pause_state.pause_start_time
                self.serial_log.append(f"[System RESUMED] Paused for {paused_time:.1f} seconds")

    def run_cycle(self):
        while self._running:
            if not self.pause_state.is_paused:
                self.run_traffic_cycle()
            else:
                time.sleep(0.1)

    def run_traffic_cycle(self):
        for i in range(self.pause_state.current_light, 5):
            with self._lock:
                self.pause_state.current_light = i
                current = self.light_order[i]
                next_light = self.light_order[(i + 1) % 5]
                self.pause_state.next_light = next_light

            # Phase 1: All RED
            if self.pause_state.current_phase <= 0:
                self.turn_all_red()
                self.send_light_states()
                if not self.smart_delay(0.01):
                    return
                self.pause_state.current_phase = 1

            # Phase 2: Current GREEN
            if self.pause_state.current_phase <= 1:
                with self._lock:
                    self.light_states[current][0] = False  # Red off
                    self.light_states[current][2] = True   # Green on
                self.send_light_states()
                if not self.smart_delay(self.light_delays[current][2] / 1000):
                    return
                self.pause_state.current_phase = 2

            # Phase 3: Current YELLOW + Next YELLOW
            if self.pause_state.current_phase <= 2:
                with self._lock:
                    self.light_states[current][2] = False  # Green off
                    self.light_states[current][1] = True   # Yellow on
                    self.light_states[next_light][0] = False  # Next red off
                    self.light_states[next_light][1] = True   # Next yellow on
                self.send_light_states()
                if not self.smart_delay(self.light_delays[current][1] / 1000):
                    return
                self.pause_state.current_phase = 3

            # Cleanup
            with self._lock:
                self.light_states[current][1] = False
                self.light_states[next_light][1] = False
            self.send_light_states()
            self.pause_state.current_phase = 0

        with self._lock:
            self.pause_state.current_light = 0

    def smart_delay(self, duration):
        start = time.time()
        while time.time() - start < duration:
            if self.pause_state.is_paused:
                with self._lock:
                    self.pause_state.remaining_delay = duration - (time.time() - start)
                return False
            time.sleep(0.01)
        return True

    def turn_all_red(self):
        with self._lock:
            for i in range(5):
                self.light_states[i][0] = True
                self.light_states[i][1] = False
                self.light_states[i][2] = False
        if not self.pause_state.is_paused:
            self.send_light_states()

    def send_light_states(self):
        with self._lock:
            if self.pause_state.is_paused:
                return  # Don't log anything when paused
            state_str = "STATE:" + ",".join(
                f"{self.light_names[i]},{int(self.light_states[i][0])},{int(self.light_states[i][1])},{int(self.light_states[i][2])}"
                for i in range(5)
            )
            self.serial_log.append(state_str)

    def get_lights_json(self):
        with self._lock:
            data = {
                self.light_names[i]: {
                    "RED": self.light_states[i][0],
                    "YELLOW": self.light_states[i][1],
                    "GREEN": self.light_states[i][2]
                } for i in range(5)
            }
            # Find the latest STATE log entry
            state_entry = next((log for log in reversed(self.serial_log) if log.startswith("STATE:")), None)
            if state_entry:
                data["STATE"] = state_entry
            return data

    def get_serial_log(self):
        with self._lock:
            # Exclude STATE log entries from serial output
            return [log for log in self.serial_log[-100:] if not log.startswith("STATE:")]

simulator = TrafficLightSimulator()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    return jsonify(simulator.get_lights_json())

@app.route('/command', methods=['POST'])
def handle_command():
    cmd = request.json.get('command')
    if cmd:
        if cmd == "!pause":
            simulator.pause()
        elif cmd == "!resume":
            simulator.resume()
        elif cmd.startswith("!order "):
            try:
                order = [int(x) for x in cmd[7:].split(",")]
                simulator.set_order(order)
            except Exception:
                pass
        elif cmd.startswith("!delay "):
            try:
                delays = [int(x) for x in cmd[7:].split(",")]
                simulator.set_delays(delays)
            except Exception:
                pass
        elif cmd == "!status":
            simulator.serial_log.append("STATUS REQUESTED")
        else:
            simulator.serial_log.append(f"Unknown command: {cmd}")
        return jsonify({"status": "success"})
    return jsonify({"status": "error"})

@app.route('/serial')
def serial_view():
    return '\n'.join(simulator.get_serial_log()), 200, {'Content-Type': 'text/plain; charset=utf-8'}

@app.route('/order', methods=['POST'])
def set_order():
    data = request.json.get('order')
    if isinstance(data, list) and len(data) == 5 and sorted(data) == list(range(5)):
        simulator.set_order(data)
        return jsonify({"status": "success"})
    return jsonify({"status": "error", "message": "Invalid order"})

@app.route('/delay', methods=['POST'])
def set_delay():
    data = request.json.get('delay')
    if isinstance(data, list) and len(data) == 15:
        simulator.set_delays(data)
        return jsonify({"status": "success"})
    return jsonify({"status": "error", "message": "Invalid delays"})

if __name__ == '__main__':
    app.run(debug=True, threaded=True)
