// Pin definitions red yellow green
const int lights[5][3] = {
  {A2, A1, A0},    // 0: Center
  {2, 3, 4},       // 1: North
  {5, 6, 7},       // 2: East
  {8, 9, 10},      // 3: South
  {11, 12, 13}     // 4: West
};

// System state
int lightOrder[5] = {0, 1, 2, 3, 4};
unsigned long lightDelays[5][3] = {
  {1000, 1000, 1000},  // Center
  {1000, 1000, 1000},  // North
  {1000, 1000, 1000},  // East
  {1000, 1000, 1000},  // South
  {1000, 1000, 1000}   // West
};


// Pause/resume state
struct PauseState {
  bool isPaused = false;
  unsigned long pauseStartTime = 0;
  unsigned long remainingDelay = 0;
  int currentPhase = 0;
  int currentLight = 0;
  int nextLight = 1;
} pauseState;

void setup() {
  Serial.begin(115200);
  Serial.println("\nTraffic Light Controller v2.1");
  Serial.println("Available commands:");
  Serial.println("!order 0,1,2,3,4 - Set light sequence");
  Serial.println("!delay 5000,2000,5000,... - Set all timings (15 values)");
  Serial.println("!pause - Freeze current state");
  Serial.println("!resume - Continue operation");
  Serial.println("!status - Show current settings");
  
  // Initialize pins
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 3; j++) {
      pinMode(lights[i][j], OUTPUT);
      digitalWrite(lights[i][j], LOW);
    }
  }
  turnAllRed();
}

void loop() {
  if (!pauseState.isPaused) {
    runTrafficCycle();
  }
  checkSerial();
}

void runTrafficCycle() {
  for (int i = pauseState.currentLight; i < 5; i++) {
    pauseState.currentLight = i;
    int current = lightOrder[i];
    int next = lightOrder[(i + 1) % 5];
    pauseState.nextLight = next;

    // Phase 1: Current GREEN
    if (pauseState.currentPhase <= 1) {
      // State update before transition
      sendLightStates();
      
      // Transition from RED to GREEN
      digitalWrite(lights[current][0], LOW);   // Turn off current RED
      sendLightStates();  // State update after RED off
      digitalWrite(lights[current][2], HIGH); // Turn on current GREEN
      sendLightStates();  // State update after GREEN on
      
      if (!smartDelay(lightDelays[current][2])) {
        sendLightStates();  // Final state update if paused
        return;
      }
      pauseState.currentPhase = 2;
      sendLightStates();  // State update after phase completion
    }
    
    // Phase 2: Current YELLOW (with RED) + Next YELLOW (with RED)
    if (pauseState.currentPhase <= 2) {
      // State update before transition
      sendLightStates();
      
      // Transition from GREEN to YELLOW (with RED)
      digitalWrite(lights[current][2], LOW);  // Turn off current GREEN
      sendLightStates();  // State update after GREEN off
      digitalWrite(lights[current][0], HIGH); // Turn on current RED (ensure it's on)
      digitalWrite(lights[current][1], HIGH); // Turn on current YELLOW
      sendLightStates();  // State update after YELLOW+RED on
      
      // Prepare next light (YELLOW with RED)
      digitalWrite(lights[next][0], HIGH);    // Ensure next RED is on
      digitalWrite(lights[next][1], HIGH);    // Turn on next YELLOW
      sendLightStates();  // State update after next YELLOW+RED on
      
      if (!smartDelay(lightDelays[current][1])) {
        sendLightStates();  // Final state update if paused
        return;
      }
      pauseState.currentPhase = 3;
      sendLightStates();  // State update after phase completion
    }

    // Phase 3: Cleanup and transition to next light
    sendLightStates();  // State update before cleanup
    
    // Turn off YELLOWs (REDs stay on)
    digitalWrite(lights[current][1], LOW);    // Turn off current YELLOW
    sendLightStates();  // State update after current YELLOW off
    digitalWrite(lights[next][1], LOW);       // Turn off next YELLOW
    sendLightStates();  // State update after next YELLOW off
    
    // Ensure REDs stay on (they should already be on)
    digitalWrite(lights[next][0], HIGH);      // Ensure next RED is on
    digitalWrite(lights[current][0], HIGH);   // Ensure current RED is on
    sendLightStates();  // Final state update after cleanup
    
    pauseState.currentPhase = 1; // Reset for next light
  }
  pauseState.currentLight = 0; // Reset cycle
  sendLightStates();  // Final state update after cycle complete
}

bool smartDelay(unsigned long duration) {
  unsigned long start = millis();
  while (millis() - start < duration) {
    if (checkSerial() && pauseState.isPaused) {
      pauseState.remainingDelay = duration - (millis() - start);
      return false;
    }
    delay(10);
  }
  return true;
}

bool checkSerial() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input == "!pause") {
      pauseState.isPaused = true;
      pauseState.pauseStartTime = millis();
      Serial.println("[System PAUSED]");
      printCurrentState();
      return true;
    }
    else if (input == "!resume") {
      pauseState.isPaused = false;
      Serial.print("[System RESUMED] Paused for ");
      Serial.print((millis() - pauseState.pauseStartTime) / 1000.0, 1);
      Serial.println(" seconds");
      printCurrentState();
      return true;
    }
    else if (input.startsWith("!order ")) {
      setLightOrder(input.substring(7));
    }
    else if (input.startsWith("!delay ")) {
      setLightDelays(input.substring(7));
    }
    else if (input == "!status") {
      printStatus();
    }
    else {
      Serial.println("Unknown command. Try: !order !delay !pause !resume !status");
    }
    return true;
  }
  return false;
}

void setLightOrder(const String &data) {
  int newOrder[5] = {-1, -1, -1, -1, -1};
  int count = 0;
  
  int start = 0;
  int end = data.indexOf(',');
  
  while (end != -1 && count < 5) {
    newOrder[count++] = data.substring(start, end).toInt();
    start = end + 1;
    end = data.indexOf(',', start);
  }
  
  if (count < 5 && start < data.length()) {
    newOrder[count++] = data.substring(start).toInt();
  }

  // Validate
  bool valid = (count == 5);
  bool used[5] = {false};
  for (int i = 0; i < 5; i++) {
    if (newOrder[i] < 0 || newOrder[i] > 4 || used[newOrder[i]]) {
      valid = false;
      break;
    }
    used[newOrder[i]] = true;
  }

  if (valid) {
    memcpy(lightOrder, newOrder, sizeof(lightOrder));
    Serial.println("Light order updated:");
    printCurrentOrder();
  } else {
    Serial.println("Error: Need 5 unique numbers 0-4 separated by commas");
    Serial.println("Example: !order 1,2,3,4,0");
  }
}

void setLightDelays(const String &data) {
  int values[15];
  int count = 0;
  
  int start = 0;
  int end = data.indexOf(',');
  
  while (end != -1 && count < 15) {
    values[count++] = data.substring(start, end).toInt();
    start = end + 1;
    end = data.indexOf(',', start);
  }
  
  if (count < 15 && start < data.length()) {
    values[count++] = data.substring(start).toInt();
  }

  if (count == 15) {
    for (int i = 0; i < 5; i++) {
      lightDelays[i][0] = max(values[i*3], 100);     // Min 100ms for Red
      lightDelays[i][1] = max(values[i*3+1], 100);   // Min 100ms for Yellow
      lightDelays[i][2] = max(values[i*3+2], 100);   // Min 100ms for Green
    }
    Serial.println("Delays updated:");
    printCurrentDelays();
  } else {
    Serial.println("Error: Need 15 delay values (R,Y,G for each light)");
    Serial.println("Example: !delay 5000,2000,5000,5000,2000,5000,...");
  }
}

void printStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("State: ");
  Serial.println(pauseState.isPaused ? "PAUSED" : "RUNNING");
  printCurrentState();
  printCurrentOrder();
  printCurrentDelays();
}

void printCurrentState() {
  const char* lightNames[5] = {"NORTH", "SW", "SE", "NW", "NE"};
  Serial.print("Current light: ");
  Serial.println(lightNames[lightOrder[pauseState.currentLight]]);
  Serial.print("Next light: ");
  Serial.println(lightNames[pauseState.nextLight]);
  Serial.print("Phase: ");
  switch(pauseState.currentPhase) {
    case 0: Serial.println("All RED"); break;
    case 1: Serial.println("GREEN active"); break;
    case 2: Serial.println("YELLOW transition"); break;
  }
  if (pauseState.remainingDelay > 0) {
    Serial.print("Remaining delay: ");
    Serial.print(pauseState.remainingDelay);
    Serial.println("ms");
  }
}

void printCurrentOrder() {
  const char* lightNames[5] = {"NORTH", "SW", "SE", "NW", "NE"};
  Serial.print("Sequence: ");
  for (int i = 0; i < 5; i++) {
    Serial.print(lightNames[lightOrder[i]]);
    if (i < 4) Serial.print(" -> ");
  }
  Serial.println();
}

void printCurrentDelays() {
  const char* lightNames[5] = {"NORTH", "SW", "SE", "NW", "NE"};
  Serial.println("Timings (ms):");
  for (int i = 0; i < 5; i++) {
    Serial.print("  ");
    Serial.print(lightNames[i]);
    Serial.print(": R=");
    Serial.print(lightDelays[i][0]);
    Serial.print(" Y=");
    Serial.print(lightDelays[i][1]);
    Serial.print(" G=");
    Serial.println(lightDelays[i][2]);
  }
}

// Update direction names to match: "NORTH", "SW", "SE", "NW", "NE"
const char* lightNames[5] = {"NORTH", "SW", "SE", "NW", "NE"};

// Replace sendLightStates() with single-line output
void sendLightStates() {
  Serial.print("STATE:");
  for (int i = 0; i < 5; i++) {
    Serial.print(lightNames[i]);
    Serial.print(",");
    Serial.print(digitalRead(lights[i][0]) ? "1" : "0"); // RED
    Serial.print(",");
    Serial.print(digitalRead(lights[i][1]) ? "1" : "0"); // YELLOW
    Serial.print(",");
    Serial.print(digitalRead(lights[i][2]) ? "1" : "0"); // GREEN
    if (i < 4) Serial.print(",");
  }
  Serial.println();
}

void turnAllRed() {
  // First turn on all red lights
  for (int i = 0; i < 5; i++) {
    digitalWrite(lights[i][0], HIGH);  // Turn on RED
  }
  
  // Then turn off all other lights (YELLOW and GREEN)
  for (int i = 0; i < 5; i++) {
    digitalWrite(lights[i][1], LOW);   // Turn off YELLOW
    digitalWrite(lights[i][2], LOW);   // Turn off GREEN
  }
  
  sendLightStates();
}