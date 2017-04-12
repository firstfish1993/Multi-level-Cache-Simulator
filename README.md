# Multi-level-Cache-Simulator

**Detailed description please see the PDF file in this repository

Input trace file format:
<cycle>,<R/W>,<address>
<cycle>,<R/W>,<address>
<cycle>,<R/W>,<address>
...

<cycle>: cycle number in hexadecimal format
<R/W>: 1 if read, 0 if write
<address>: address in hexadecimal format

Ouput format Example:
% cachesim_l1…
L1 hit rate: 0.87
Total latency: 3000
L1 references: 150
AMAT: 20.00%
