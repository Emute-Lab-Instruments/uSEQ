void setup() {
  pinMode(6, OUTPUT);
}

int delayTime = 150;
void loop() {
  digitalWrite(6, HIGH); 
  delay(delayTime);
  digitalWrite(6, LOW);
  delay(delayTime); 
  
  delayTime -= 70;
  if (delayTime < 50)
    delayTime = 50;
}
