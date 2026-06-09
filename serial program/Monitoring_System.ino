int pin_RAIN_STEAM = A0;

int pin_DUST_LED = A3;
int pin_DUST_OUT = A2;

void setup() {
// put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(pin_RAIN_STEAM, INPUT);
  
  pinMode(pin_DUST_LED, OUTPUT);
  pinMode(pin_DUST_OUT, INPUT);
  digitalWrite(pin_DUST_LED, HIGH);
}

void loop() {

  int humidityValue = analogRead(pin_RAIN_STEAM);

  int dustValue = SensorRead();

  Serial.print("H:");
  Serial.print(humidityValue);
  Serial.print(" D:");
  Serial.println(dustValue);

  delay(500);
}


// 먼지 센서 함수
uint16_t SensorRead(void)
{
  uint16_t Sensor_data;

  digitalWrite(pin_DUST_LED, LOW);   // LED ON
  delayMicroseconds(280);

  Sensor_data = analogRead(pin_DUST_OUT);

  delayMicroseconds(40);
  digitalWrite(pin_DUST_LED, HIGH);  // LED OFF
  delayMicroseconds(9680);

  return Sensor_data;
}
