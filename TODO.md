# Detecting sensor prescence 
- Only reply to RTR if the sensor is plugged in 
- However, because it is an analog read, then if it is disconnected, it will be floating and pick up random noise 
- Therefore, we need to add a resistor 100 kohm from A0 to GND 
- If sensor unplugged, A0 pulled down to 0V. Else, should have some output != 0V.