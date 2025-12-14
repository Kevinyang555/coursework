# Repository Guidelines

## Project Structure & Module Organization
- Sources: `*.sv` in repo root (e.g., `processor_top.sv`, `control_unit.sv`, `ripple_adder.sv`).
- Testbenches: `testbench_*.sv` (e.g., `testbench_mult.sv`).
- Constraints: `lab4_clean.xdc` for board pins/clocks.
- Top module: `processor_top` (unless a lab specifies otherwise).

## Build, Test, and Development Commands
- Vivado GUI: Create a project, add all `*.sv` as sources, set `lab4_clean.xdc` as constraints, set top to `processor_top`, then Run Synthesis → Implementation → Generate Bitstream.
- Vivado TCL (non-project flow example):
  ```tcl
  read_verilog [glob *.sv]
  read_xdc lab4_clean.xdc
  synth_design -top processor_top -part <FPGA_PART>
  opt_design; place_design; route_design
  write_bitstream -force build/top.bit
  ```
- Simulation (Vivado): set top to a testbench (e.g., `testbench_mult`), run `Run All` and inspect waves.
- Simulation (CLI example):
  ```sh
  xvlog *.sv && xelab testbench_mult -s tb && xsim tb -runall
  ```

## Coding Style & Naming Conventions
- Indentation: 2 spaces; no tabs.
- Filenames: module-aligned (e.g., `ripple_adder.sv` → module `ripple_adder`).
- Signals/instances: `snake_case`; parameters/localparams: `UPPER_SNAKE_CASE`.
- One module per file; keep ports/types at top. Use `logic` over `wire/reg` unless required.
- Reset polarity: match board conventions; document in module header.

## Testing Guidelines
- Testbench naming: `testbench_<unit>.sv`; top module matches filename.
- Use self-checking where possible (`$fatal`/`assert`) and concise stimulus.
- Run unit sims before integration; prefer deterministic clocks/resets.
- Minimum: exercise edge cases (overflow, sign, debounced inputs, etc.).

## Commit & Pull Request Guidelines
- Commits: small, focused, imperative present tense (e.g., "add control unit alu op").
- Reference labs/issues in body (e.g., "Lab4: fix ripple carry width").
- PRs: include summary, affected modules, test evidence (waveform screenshot or sim log), and any timing/area notes.

## Security & Configuration Tips
- Do not edit critical clock/pin assignments in `lab4_clean.xdc` without confirming board.
- Verify `-part` matches your FPGA before synthesis.
