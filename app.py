# app.py
from flask import Flask, render_template, jsonify, request
import serial
import time
from threading import Thread, Lock

app = Flask(__name__)

# Light names matching the Arduino code
directions = ["NORTH", "SW", "SE", "NW", "NE"]

# Initialize all lights to RED (matches Arduino initial state)
lights = {dir: {"RED": True, "YELLOW": False, "GREEN": False} for dir in directions}

# Serial communication setup
ser = None
try:
    ser = serial.Serial('COM4', 115200, timeout=1)  # Update COM port as needed
    time.sleep(2)  # Wait for Arduino to initialize>comm
except Exception as e:
    print(f"Error opening serial port: {e}")

# Lock for thread-safe serial communication
serial_lock = Lock()

# Buffer for serial log
serial_log = []

def update_lights_from_arduino():
    global lights, serial_log
    while True:
        if ser and ser.in_waiting:
            with serial_lock:
                line = ser.readline().decode('utf-8').strip()
                if line:
                    serial_log.append(line)
                    if len(serial_log) > 100:
                        serial_log = serial_log[-100:]
                # Expect format: STATE:NORTH,1,0,0,SW,1,0,0,SE,1,0,0,NW,1,0,0,NE,1,0,0
                if line.startswith("STATE:"):
                    try:
                        state_data = line.split(':')[1].split(',')
                        # state_data: [dir1,red1,yellow1,green1,dir2,red2,...]
                        for i in range(0, len(state_data), 4):
                            direction = state_data[i]
                            red = state_data[i+1] == '1'
                            yellow = state_data[i+2] == '1'
                            green = state_data[i+3] == '1'
                            if direction in lights:
                                lights[direction]["RED"] = red
                                lights[direction]["YELLOW"] = yellow
                                lights[direction]["GREEN"] = green
                    except Exception as e:
                        print(f"Error parsing state line: {e}")

        time.sleep(0.1)

def send_command(cmd):
    if ser:
        with serial_lock:
            ser.write((cmd + '\n').encode('utf-8'))

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    return jsonify(lights)

@app.route('/command', methods=['POST'])
def handle_command():
    cmd = request.json.get('command')
    if cmd:
        send_command(cmd)
        return jsonify({"status": "success"})
    return jsonify({"status": "error"})

@app.route('/serial')
def serial_view():
    # Return the last lines as plain text
    return '\n'.join(serial_log[-100:]), 200, {'Content-Type': 'text/plain; charset=utf-8'}

if __name__ == '__main__':
    if ser:
        update_thread = Thread(target=update_lights_from_arduino)
        update_thread.daemon = True
        update_thread.start()
    
    app.run(debug=True, threaded=True)