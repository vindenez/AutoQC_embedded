An implementation of Adaptive Anomaly Detector (AdapAD) for embedded systems with limited computational resources. AdapAD is an AI-based model for automatic data quality control.

The model aims to detect anomalous measurements from real-time univariate time series data.


Make sure makefile suits your CPU architecture. Config path is specified in Main.cpp, other paths/hyperparameters are defined in Config.yaml.

If you use included Makefile:  
Clean - make clean  
Build - make  
Run - ./adapad  

## Runtime on ARM Cortex-A7 528 MHz

0.76 Seconds processing time (prediction/forward pass and update/backprop) per time step.

## Memory usage

2.5 MB


## Model comparsion - Tide pressure validation dataset
![Screenshot 2025-03-10 at 15 01 06](https://github.com/user-attachments/assets/ff04a6e6-28ca-4393-af57-b29c016c7a55)
