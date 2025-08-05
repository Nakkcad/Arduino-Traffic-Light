# app.py
from flask import Flask, render_template, jsonify, request
import serial
import time
from threading import Thread, Lock
from collections import deque
import serial.tools.list_ports

app = Flask(__name__)

# Light names matching the Arduino code
directions = ["NORTH", "NE", "SE", "SW", "NW"]

# Initialize all lights to RED (matches Arduino initial state)
lights = {dir: {"RED": True, "YELLOW": False, "GREEN": False} for dir in directions}

# State history tracking (last 100 states)
state_history = deque(maxlen=100)

# Serial communication setup
ser = None
serial_lock = Lock()

def find_serial_port():
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        return port.device
    return None

BAUDRATE = 115200

def connect_serial():
    global ser
    while True:
        with serial_lock:
            if ser is None:
                port = find_serial_port()
                if port:
                    try:
                        ser = serial.Serial(port, BAUDRATE, timeout=1)
                        print(f"Connected to {port}")
                        time.sleep(2)  # Wait for Arduino to initialize
                    except Exception as e:
                        print(f"Failed to connect: {e}")
                else:
                    print("No serial port found. Retrying in 2s...")
        time.sleep(2)

# Buffer for serial log
serial_log = deque(maxlen=100)  # Changed to deque for better performance

def get_serial_log():
    """Return serial log entries excluding STATE messages, similar to test.py"""
    return [log for log in serial_log if not log.startswith("STATE ")]

def get_state_log():
    return [log for log in serial_log if log.startswith("STATE ")]
    

def update_lights_from_arduino():
    global lights, serial_log, state_history
    while True:
        with serial_lock:
            if ser and ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)  # Print serial output to terminal
                    serial_log.append(line)
                    if line.startswith("STATE "):
                        try:
                            # Example: STATE 100100100100100
                            state_bits = line.split("STATE ")[1].strip()
                            # Each direction has 3 bits: RED, YELLOW, GREEN
                            # directions = ["NORTH", "NE", "SE", "SW", "NW"]
                            if len(state_bits) == len(directions) * 3:
                                current_state = {}
                                for idx, direction in enumerate(directions):
                                    base = idx * 3
                                    red = state_bits[base] == '1'
                                    yellow = state_bits[base + 1] == '1'
                                    green = state_bits[base + 2] == '1'
                                    lights[direction]["RED"] = red
                                    lights[direction]["YELLOW"] = yellow
                                    lights[direction]["GREEN"] = green  
                                    current_state[direction] = {
                                        "RED": red,
                                        "YELLOW": yellow,
                                        "GREEN": green
                                    }
                                state_history.append({
                                    "state": current_state,
                                    "raw": line
                                })
                            else:
                                print(f"Unexpected state length: {state_bits}")
                        except Exception as e:
                            print(f"Error parsing state line: {e}")
                    else:
                        serial_log.append(line)
        time.sleep(0.05)

def send_command(cmd):
    with serial_lock:
        if ser:
            ser.write((cmd + '\n').encode('utf-8'))
            serial_log.append(f"CMD: {cmd}")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    return jsonify({
        "current": lights,
        "history": list(state_history)[-10:]  # Return last 10 states
    })

@app.route('/command', methods=['POST'])
def handle_command():
    cmd = request.json.get('command')
    if cmd:
        send_command(cmd)
        return jsonify({"status": "success"})
    return jsonify({"status": "error"})

@app.route('/serial')
def serial_view():
    # Return the filtered serial log (excluding STATE messages) as plain text
    return '\n'.join(get_serial_log()), 200, {'Content-Type': 'text/plain; charset=utf-8'}

if __name__ == '__main__':
    Thread(target=connect_serial, daemon=True).start()
    Thread(target=update_lights_from_arduino, daemon=True).start()
    app.run(debug=True, threaded=True)