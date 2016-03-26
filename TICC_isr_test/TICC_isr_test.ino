// TICC_test -- 06 March 2016
// Tests load created by hardware interrupt

int interrupt = 1; // interrupt for Uno pin 3
int int_pin = 3; // physical pin

float i = 0.0;
float temp = 0.0;

volatile unsigned long long coarse_count = 0;
int times_through = 0;
unsigned long j = 0;
unsigned long ticks = 0;
unsigned long start_ticks = 0;
unsigned long start_time = 0;
unsigned long duration = 0;

void setup() {
  pinMode(int_pin, INPUT);
  attachInterrupt(interrupt, coarse_timer, RISING);
  Serial.begin(115200);
  Serial.println("Starting...");
}

void loop() {

  while ( times_through < 1 ) {
    start_ticks = coarse_count;
    start_time = micros();

    for (j = 4.0; j<1000; j++) { temp = j/3.1416; }
   
    ticks = (coarse_count - start_ticks);
    duration = micros() - start_time;
    Serial.print("ticks:");
    Serial.print(ticks);
    Serial.print("  loop time: ");
    Serial.println(duration);
    times_through++;
  }
}

void coarse_timer() {
  coarse_count++;
}
