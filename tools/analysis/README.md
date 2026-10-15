# tools/analysis: Python prototype (analysis only, not the port)
Runs the ROM on the third-party `z80` package and draws screens. It is the reference oracle for the C++ port.
Rules: do not change harness.py or render.py behaviour (the golden hashes in tests/test_prototype.py guard it). Add new analysis as new files named oracle_*.py. dis.py reads build/rom/prog_raw.asm (linear disassembly, mixes code and data; superseded by step S08).
