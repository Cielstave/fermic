# Job queue simulation

A runnable Fermi example that generates a deterministic workload, skips
low-priority jobs, and stops before exceeding a work-unit budget. It simulates
sequential admission and completion; it does not create threads or network jobs.

The fixed nonzero xorshift32 seed produces the same workload on each run. The
application attempts at most 24 jobs with a budget of 60 work units. Each job
costs between one and nine units. Skipped jobs consume no work units; a job that
would exceed the remaining budget is not completed.

## Build and run

From the repository root:

```sh
make
./build/fermi run examples/tools/job_queue/main.fe

# Keep the native executable for later runs:
./build/fermi run examples/tools/job_queue/main.fe -o build/job-queue -O2
```

`scheduler.fe` contains workload generation and admission rules. `report.fe`
formats numbers into a fixed byte array and handles standard-output Results.
`main.fe` connects the simulation to its report. The example exercises local
modules, structs, enum matching, u32 bitwise operations, explicit widening,
for loops, break/continue, byte views, and checked standard-library calls.

The work-unit budget is application accounting, not a compiler work certificate
or elapsed-time deadline. Successful execution exits with status 0; report
failures return a nonzero status.
