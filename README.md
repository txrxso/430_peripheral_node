# 430_peripheral_node
firmware for peripheral nodes

Branch `main` is for the general code logic template. 
Branch `noise` is for the noise node. 
Branch `air_quality` is for the air quality node. 

Features: 
- Samples sensor data
- Responds to gateway RTRs from the gateway node. Only responds if there is valid sensor data 
- Suppression of future alerts for 1 minute if manual clear mode was activated 
