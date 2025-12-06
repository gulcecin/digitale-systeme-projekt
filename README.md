Digitale Systeme – Programmierprojekt (Teil 1 & Teil 2)
Dieses Repository enthält meine Lösung für das Programmierprojekt aus dem Kurs
Digitale Systeme 2025 an der Humboldt-Universität zu Berlin.
Das Projekt besteht aus zwei Teilen:
1. Teil 1 – RS232 Deserialisierung
2. Teil 2 – Bare-Metal RISC-V (rv32i) Programm zur Approximation von atan2
Teil 1 – RS232 Deserialisierung
Im ersten Teil wird ein C-Programm implementiert, das die seriellen Bits aus dem
Simulator korrekt abtastet, zu Bytes zusammenfügt und die dekodierten Zeichen über
printf() auf der Standardausgabe ausgibt.
Implementierung: src/rs232.c
Simulation (Teil 1):
./run.sh
Teil 2 – Bare-Metal RISC-V Programm (atan2 in Festkommaarithmetik)
Im zweiten Teil wird ein eigenes RISC-V-Programm (rv32i) geschrieben, das:
atan2(a3 - a1, a2 - a0)
in 32-Bit Fixpunktarithmetik berechnet und die Resultate als 8×8 Matrix ausgibt.
Implementierung: rv32iProgramm/main_rv32.c
Kompilieren & Ausführen (Teil 2):
module load riscv/rv32i
cd rv32iProgramm
make
cd ..
./run.sh
Projektstruktur:
projekt/
   src/ (RS232 Teil 1)
   rv32iProgramm/ (RISC-V Teil 2)
   run.sh
   Makefile
   Aufgabenstellung.pdf
Benötigte Tools:
- Verilator
- gcc / g++
- RISC-V gcc Toolchain (rv32i)
Autor: Gülce Cin
