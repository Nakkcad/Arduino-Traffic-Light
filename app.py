from flask import Flask, render_template, jsonify
import time
from threading import Thread

app = Flask(__name__)

# Pentagon directions (5-way intersection)
directions = ["NORTH", "NORTHEAST", "SOUTHEAST", "SOUTHWEST", "NORTHWEST"]

# Initialize all lights to RED
lights = {dir: {"RED": True, "YELLOW": False, "GREEN": False} for dir in directions}

# Timing settings (in seconds)
GREEN_TIME = 5
YELLOW_TIME = 2

def turn_all_red():
    for direction in lights:
        lights[direction]["RED"] = True
        lights[direction]["YELLOW"] = False
        lights[direction]["GREEN"] = False

def traffic_light_controller():
    while True:
        for i in range(5):
            current = directions[i]
            next_dir = directions[(i + 1) % 5]

            # Turn all RED
            turn_all_red()
            print(f"All lights RED - Preparing {current}")

            # Turn current GREEN
            lights[current]["RED"] = False
            lights[current]["GREEN"] = True
            print(f"{current} GREEN - Go!")
            time.sleep(GREEN_TIME)

            # Turn current YELLOW
            lights[current]["GREEN"] = False
            lights[current]["YELLOW"] = True
            print(f"{current} YELLOW - Prepare to stop")

            # Also turn next light YELLOW
            lights[next_dir]["RED"] = False
            lights[next_dir]["YELLOW"] = True
            print(f"{next_dir} YELLOW - Prepare to go")
            time.sleep(YELLOW_TIME)

            # Turn both YELLOWs OFF
            lights[current]["YELLOW"] = False
            lights[next_dir]["YELLOW"] = False
            print(f"Turned off YELLOW for {current} and {next_dir}")

@app.route('/')
def index():
    return render_template('index.html', directions=directions)

@app.route('/status')
def status():
    return jsonify(lights)

if __name__ == '__main__':
    controller_thread = Thread(target=traffic_light_controller)
    controller_thread.daemon = True
    controller_thread.start()
    app.run(debug=True, threaded=True)