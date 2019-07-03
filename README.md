# ns-3-extended-with-execution-environment
ns-3 extended with a framework that follows Stein Kristiansen's methodology to simulate the execution times of nodes.<br><br>

This repo includes the DCEP-Sim module from the repo https://github.com/fabricesb/DCEP-Sim and publication in https://dl.acm.org/citation.cfm?id=3093919. The repo contains the software execution model from the paper https://dl.acm.org/citation.cfm?id=3332508 of the CEP system T-Rex running on a Raspberry Pi 3B. The device file for the model is in device-files/trex.device. Any simulation program that uses this device file is using this model.<br><br>

The communication software execution model of TinyOS running on TelosB from the paper in https://dl.acm.org/citation.cfm?id=3307371 is included in this repo. The model is in device-files/telosb.device. A similar model of a multicore system from the publication in https://dl.acm.org/citation.cfm?id=3307384 is also included in this repo, in device-files/gnex.device.
