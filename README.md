# ns-3-extended-with-execution-environment
ns-3 extended with Stein Kristiansen's methodology to model software execution.

This repo includes the DCEP-Sim module from the repo https://github.com/fabricesb/DCEP-Sim and publication in https://dl.acm.org/citation.cfm?id=3093919.<br>
The repo includes the software execution model from the publication https://dl.acm.org/citation.cfm?id=3332508. The device file for the model is in device-files/trex.device. Any simulation program that uses this device file is using this model.<br>
The communication software execution model of TinyOS running on TelosB from the publication in https://dl.acm.org/citation.cfm?id=3307371 is included in this repo. The model is in device-files/telosb.device. A similar model of a multicore system from the publication in https://dl.acm.org/citation.cfm?id=3307384 is also included in this repo, in device-files/gnex.device.<br>
