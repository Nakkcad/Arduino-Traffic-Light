// ========== Configuration ==========
const char* VERSION = "2.4";
const unsigned long MIN_DELAY = 100; // Minimum 100ms for any light state

// ========== Type Definitions ==========
enum LightPhase {
  ALL_RED,
  GREEN_ACTIVE,
  YELLOW_TRANSITION
};

// ========== Pin Definitions ==========
const struct {
  uint8_t red;
  uint8_t yellow;
  uint8_t green;
} LIGHT_PINS[5] = {
  {A2, A1, A0},		// 0: NORTH
  {2, 3, 4},		// 1: NE
  {5, 6, 7},		// 2: SE
  {8, 9, 10},		// 3: SW
  {11, 12, 13}		// 4: NW  
       
};

const char* LIGHT_NAMES[5] = {"NORTH", "NE", "SE", "SW", "NW"};

// ========== System State ==========
struct {
  int8_t order[5] = {0, 1, 2, 3, 4};  // Default order matches NORTH, NE, SE, SW, NW
  uint16_t delays[5][3] = {  // [light][phase]
    {5000, 2000, 5000},  // NORTH
    {5000, 2000, 5000},  // NE
    {5000, 2000, 5000},  // SE
    {5000, 2000, 5000},  // SW
    {5000, 2000, 5000}   // NW
  };
} config;

struct {
  bool isPaused = false;
  unsigned long pauseStartTime = 0;
  unsigned long remainingDelay = 0;
  LightPhase currentPhase = ALL_RED;
  int8_t currentLight = 0;
  int8_t nextLight = 1;
  
  // For state preservation during manual mode
  struct {
    LightPhase savedPhase;
    int8_t savedCurrentLight;
    int8_t savedNextLight;
    unsigned long savedRemainingDelay;
  } preservedState;
} systemState;

struct {
  bool isActive = false;
  unsigned long endTime = 0;
  String pattern = "";
} manualControl;

// ========== Function Prototypes ==========
void initializeSystem();
void runTrafficCycle();
void handleManualControl();
bool processDelay(unsigned long duration);
bool checkSerialInput();
void processCommand(const String& input);
void setLightOrder(const String& data);
void setLightDelays(const String& data);
void setManualControl(const String& data);
void printStatus();
void printCurrentState();
void printLightOrder();
void printLightDelays();
void sendLightStates();
void turnAllRed();
void safeLightTransition(uint8_t light, uint8_t fromState, uint8_t toState);

// ========== Main Functions ==========
void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection
  
  Serial.println("\nTraffic Light Controller v" + String(VERSION));
  Serial.println("Light Order: NORTH -> NE -> SE -> SW -> NW");
  Serial.println("Available commands:");
  Serial.println("!order 0,1,2,3,4 - Set light sequence (0=NORTH, 1=NE, 2=SE, 3=SW, 4=NW)");
  Serial.println("!delay 5000,2000,5000,... - Set all timings (15 values: R,Y,G for each light)");
  Serial.println("!pause - Freeze current state");
  Serial.println("!resume - Continue operation");
  Serial.println("!manual <duration_ms> <pattern> - Manual control (15-digit pattern)");
  Serial.println("!status - Show current settings");
  
  initializeSystem();
}

void loop() {
  if (manualControl.isActive) {
    handleManualControl();
  } else if (!systemState.isPaused) {
    runTrafficCycle();
  }
  checkSerialInput();
}

// ========== Core Logic ==========
void initializeSystem() {
  // Initialize all pins
  for (int i = 0; i < 5; i++) {
    pinMode(LIGHT_PINS[i].red, OUTPUT);
    pinMode(LIGHT_PINS[i].yellow, OUTPUT);
    pinMode(LIGHT_PINS[i].green, OUTPUT);
    digitalWrite(LIGHT_PINS[i].red, LOW);
    digitalWrite(LIGHT_PINS[i].yellow, LOW);
    digitalWrite(LIGHT_PINS[i].green, LOW);
  }
  turnAllRed();
}

void runTrafficCycle() {
  for (int i = systemState.currentLight; i < 5; i++) {
    systemState.currentLight = i;
    int current = config.order[i];
    int next = config.order[(i + 1) % 5];
    systemState.nextLight = next;

    // Phase 1: Current GREEN
    if (systemState.currentPhase <= GREEN_ACTIVE) {
      sendLightStates();
      safeLightTransition(current, 0, 2); // RED to GREEN
      
      if (!processDelay(config.delays[current][2])) {
        sendLightStates();
        return;
      }
      systemState.currentPhase = YELLOW_TRANSITION;
      sendLightStates();
    }
    
    // Phase 2: Current YELLOW + Next YELLOW (both with RED)
    if (systemState.currentPhase <= YELLOW_TRANSITION) {
      sendLightStates();
      safeLightTransition(current, 2, 1); // GREEN to YELLOW
      digitalWrite(LIGHT_PINS[current].red, HIGH);
      
      // Prepare next light
      digitalWrite(LIGHT_PINS[next].red, HIGH);
      digitalWrite(LIGHT_PINS[next].yellow, HIGH);
      sendLightStates();
      
      if (!processDelay(config.delays[current][1])) {
        sendLightStates();
        return;
      }
      systemState.currentPhase = ALL_RED;
      sendLightStates();
    }

    // Phase 3: Cleanup
    sendLightStates();
    digitalWrite(LIGHT_PINS[current].yellow, LOW);
    digitalWrite(LIGHT_PINS[next].yellow, LOW);
    digitalWrite(LIGHT_PINS[next].red, HIGH);
    digitalWrite(LIGHT_PINS[current].red, HIGH);
    sendLightStates();
    
    systemState.currentPhase = GREEN_ACTIVE; // Reset for next light
  }
  systemState.currentLight = 0; // Reset cycle
  sendLightStates();
}

// ========== Helper Functions ==========
void safeLightTransition(uint8_t light, uint8_t fromState, uint8_t toState) {
  // Turn off previous state
  switch (fromState) {
    case 0: digitalWrite(LIGHT_PINS[light].red, LOW); break;
    case 1: digitalWrite(LIGHT_PINS[light].yellow, LOW); break;
    case 2: digitalWrite(LIGHT_PINS[light].green, LOW); break;
  }
  
  // Turn on new state
  switch (toState) {
    case 0: digitalWrite(LIGHT_PINS[light].red, HIGH); break;
    case 1: digitalWrite(LIGHT_PINS[light].yellow, HIGH); break;
    case 2: digitalWrite(LIGHT_PINS[light].green, HIGH); break;
  }
}

bool processDelay(unsigned long duration) {
  unsigned long start = millis();
  while (millis() - start < duration) {
    if (checkSerialInput()) {
      if (systemState.isPaused || manualControl.isActive) {
        systemState.remainingDelay = duration - (millis() - start);
        return false;
      }
    }
    delay(10);
  }
  return true;
}

bool checkSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    processCommand(input);
    return true;
  }
  return false;
}

void processCommand(const String& input) {
  if (input == "!pause") {
    systemState.isPaused = true;
    systemState.pauseStartTime = millis();
    Serial.println("[System PAUSED]");
    printCurrentState();
  }
  else if (input == "!resume") {
    systemState.isPaused = false;
    Serial.print("[System RESUMED] Paused for ");
    Serial.print((millis() - systemState.pauseStartTime) / 1000.0, 1);
    Serial.println(" seconds");
    printCurrentState();
  }
  else if (input.startsWith("!order ")) {
    setLightOrder(input.substring(7));
  }
  else if (input.startsWith("!delay ")) {
    setLightDelays(input.substring(7));
  }
  else if (input.startsWith("!manual ")) {
    setManualControl(input.substring(8));
  }
  else if (input == "!status") {
    printStatus();
  }
  else {
    Serial.println("Unknown command. Try: !order !delay !pause !resume !status !manual");
  }
}

void setManualControl(const String& data) {
  int spaceIndex = data.indexOf(' ');
  if (spaceIndex == -1) {
    Serial.println("Error: Invalid manual command format. Use: !manual <duration_ms> <pattern>");
    return;
  }
  
  unsigned long duration = data.substring(0, spaceIndex).toInt();
  String pattern = data.substring(spaceIndex + 1);
  
  // Validate pattern (should be 15 characters of 0s and 1s)
  if (pattern.length() != 15) {
    Serial.println("Error: Pattern must be 15 characters (5 lights × 3 states each)");
    return;
  }
  
  for (int i = 0; i < pattern.length(); i++) {
    if (pattern.charAt(i) != '0' && pattern.charAt(i) != '1') {
      Serial.println("Error: Pattern must contain only 0s and 1s");
      return;
    }
  }
  
  manualControl.isActive = true;
  manualControl.endTime = millis() + duration;
  manualControl.pattern = pattern;
  
  Serial.print("[Manual control activated for ");
  Serial.print(duration);
  Serial.println("ms]");
}

void setLightOrder(const String& data) {
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
    for (int i = 0; i < 5; i++) config.order[i] = newOrder[i];
    Serial.println("Light order updated:");
    printLightOrder();
  } else {
    Serial.println("Error: Need 5 unique numbers 0-4 separated by commas");
    Serial.println("Example: !order 1,2,3,4,0");
  }
}

void setLightDelays(const String& data) {
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
      config.delays[i][0] = max(values[i*3], MIN_DELAY);     // Red
      config.delays[i][1] = max(values[i*3+1], MIN_DELAY);   // Yellow
      config.delays[i][2] = max(values[i*3+2], MIN_DELAY);   // Green
    }
    Serial.println("Delays updated:");
    printLightDelays();
  } else {
    Serial.println("Error: Need 15 delay values (R,Y,G for each light)");
    Serial.println("Example: !delay 5000,2000,5000,...");
  }
}

void handleManualControl() {
  // Check if manual control duration has expired
  if (millis() >= manualControl.endTime) {
    manualControl.isActive = false;
    turnAllRed(); // Reset all lights to red before returning to automatic
    Serial.println("[Manual control ended]");
    return;
  }
  
  // Apply the manual pattern (15 characters representing R,Y,G for 5 lights)
  if (manualControl.pattern.length() == 15) {
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 3; j++) {
        int index = i * 3 + j;
        if (index < manualControl.pattern.length()) {
          digitalWrite(LIGHT_PINS[i].red, manualControl.pattern.charAt(i*3+0) == '1' ? HIGH : LOW);
          digitalWrite(LIGHT_PINS[i].yellow, manualControl.pattern.charAt(i*3+1) == '1' ? HIGH : LOW);
          digitalWrite(LIGHT_PINS[i].green, manualControl.pattern.charAt(i*3+2) == '1' ? HIGH : LOW);
          break;
        }
      }
    }
  }
}

void printStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("State: ");
  if (manualControl.isActive) {
    Serial.print("MANUAL CONTROL (ends in ");
    Serial.print(max(0, (long)(manualControl.endTime - millis())));
    Serial.println("ms)");
  } else {
    Serial.println(systemState.isPaused ? "PAUSED" : "RUNNING");
  }
  printCurrentState();
  printLightOrder();
  printLightDelays();
}

void printCurrentState() {
  Serial.print("Current light: ");
  Serial.println(LIGHT_NAMES[config.order[systemState.currentLight]]);
  Serial.print("Next light: ");
  Serial.println(LIGHT_NAMES[systemState.nextLight]);
  Serial.print("Phase: ");
  switch(systemState.currentPhase) {
    case ALL_RED: Serial.println("All RED"); break;
    case GREEN_ACTIVE: Serial.println("GREEN active"); break;
    case YELLOW_TRANSITION: Serial.println("YELLOW transition"); break;
  }
  if (systemState.remainingDelay > 0) {
    Serial.print("Remaining delay: ");
    Serial.print(systemState.remainingDelay);
    Serial.println("ms");
  }
}

void printLightOrder() {
  Serial.print("Sequence: ");
  for (int i = 0; i < 5; i++) {
    Serial.print(LIGHT_NAMES[config.order[i]]);
    if (i < 4) Serial.print(" -> ");
  }
  Serial.println();
}

void printLightDelays() {
  Serial.println("Timings (ms):");
  for (int i = 0; i < 5; i++) {
    Serial.print("  ");
    Serial.print(LIGHT_NAMES[i]);
    Serial.print(": R=");
    Serial.print(config.delays[i][0]);
    Serial.print(" Y=");
    Serial.print(config.delays[i][1]);
    Serial.print(" G=");
    Serial.println(config.delays[i][2]);
  }
}

void sendLightStates() {
  Serial.print("STATE:");
  for (int i = 0; i < 5; i++) {
    Serial.print(LIGHT_NAMES[i]);
    Serial.print(",");
    Serial.print(digitalRead(LIGHT_PINS[i].red) ? "1" : "0");
    Serial.print(",");
    Serial.print(digitalRead(LIGHT_PINS[i].yellow) ? "1" : "0");
    Serial.print(",");
    Serial.print(digitalRead(LIGHT_PINS[i].green) ? "1" : "0");
    if (i < 4) Serial.print(",");
  }
  Serial.println();
}

void turnAllRed() {
  // First turn on all red lights
  for (int i = 0; i < 5; i++) {
    digitalWrite(LIGHT_PINS[i].red, HIGH);  // Turn on RED
  }
  
  // Then turn off all other lights (YELLOW and GREEN)
  for (int i = 0; i < 5; i++) {
    digitalWrite(LIGHT_PINS[i].yellow, LOW);   // Turn off YELLOW
    digitalWrite(LIGHT_PINS[i].green, LOW);    // Turn off GREEN
  }
  
  sendLightStates();
}