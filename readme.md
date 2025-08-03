# Arduino Traffic Light Controller

This project is a web-based controller and monitor for a 5-way Arduino traffic light system. It allows you to control the light sequence, timings, and view real-time status and logs via a modern web interface.

## Features

- **Real-time traffic light visualization** (pentagon layout)
- **Pause/Resume/Status** controls
- **Set light delays** (Red, Yellow, Green for each direction)
- **Set light order** (drag & drop)
- **Serial monitor** (view and send commands)
- **State log** (recent traffic light states)
- **Dark mode** toggle

## Directory Structure

```
Arduino Traffic Light/
├── app.py                # Flask web server (connects to Arduino via serial)
├── test.py               # Traffic light simulator (no Arduino required)
├── code_arudino/
│   └── code_arudino.ino  # Arduino sketch for traffic light logic
├── templates/
│   └── index.html        # Web UI template
```

## Getting Started

### 1. Arduino Setup

- Upload [`code_arudino/code_arudino.ino`](code_arudino/code_arudino.ino) to your Arduino.
- Connect the traffic light LEDs to the specified pins.

### 2. Python Web Server

- Install dependencies:
  ```
  pip install flask pyserial
  ```
- Edit `app.py` and set the correct serial port (default: `COM4`).
- Run the server:
  ```
  python app.py
  ```
- Open [http://localhost:5000](http://localhost:5000) in your browser.

### 3. Simulator Mode (No Arduino)

- Run the simulator:
  ```
  python test.py
  ```
- Open [http://localhost:5000](http://localhost:5000) in your browser.

### 4. Arduino Simulator (Tinkercad)

- Try the traffic light circuit online using [Tinkercad Arduino Simulator](https://www.tinkercad.com/things/iLUYqUoRydo-copy-of-traffic-light/editel?returnTo=https%3A%2F%2Fwww.tinkercad.com%2Fdashboard%2Fdesigns%2Fcircuits&sharecode=RRDW1kqaKbthhYnAWL8rVBS-IlItc0Hz0e9z_RcdsGc).

## Web Interface

- **Traffic lights**: Shows current state for each direction.
- **Controls**: Pause, resume, request status.
- **Delays**: Set Red/Yellow/Green durations for each direction.
- **Order**: Drag blocks to set the traffic light sequence.
- **Serial Monitor**: View and send raw commands.
- **State Log**: Recent state changes.

## Commands

Send via serial monitor or API:

- `!pause` — Pause the cycle
- `!resume` — Resume operation
- `!status` — Print current state
- `!order 0,1,2,3,4` — Set light sequence (indices)
- `!delay 5000,2000,5000,...` — Set delays (15 values: R,Y,G for each light)

## Notes

- **Directions**: The order and naming must match between Arduino and Python code.
- **Serial Port**: Update `COM4` to match your Arduino's port.
- **Simulator**: Use `test.py` for