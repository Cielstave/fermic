# Library circulation system

An executable batch application written entirely in Fermi. The application has
1,300 source lines across 13 files (1,227 nonblank, noncomment lines), excluding
tests and this document. It exercises modules, explicit types, payload enums,
mutable record borrows, arrays, checked arithmetic, streaming input, resource
ownership, error propagation, and filesystem output.

## Run

From the repository root:

```sh
make
./build/fermi run examples/library_system/main.fe -- examples/library_system/sample.journal build/library.report
./build/fermi test examples/library_system/tests.fe
sh tests/library-system.sh
```

The report path must not already exist. Successful execution returns zero and
writes no terminal output. The supplied journal covers lending, reservations,
renewals, late fees, payments, losses, stock changes, and account closure.

## Modules

- `model.fe`: records, operation types, counters, and initial state.
- `catalog.fe`, `members.fe`: catalog and account policies.
- `circulation.fe`, `reservations.fe`: loans, fees, and reservation ordering.
- `lookup.fe`, `engine.fe`: record lookup and command dispatch.
- `decoder.fe`, `runner.fe`: incremental journal parsing and execution.
- `invariants.fe`: consistency checks after every command.
- `report.fe`, `diagnostics.fe`: deterministic reports and process diagnostics.

## Journal contract

Each command uses uppercase ASCII, single spaces, unsigned decimal arguments,
and a final LF. Blank lines are accepted. Numbers are limited to 1,000,000.
Identifiers refer to active records; retired slots may be reused.

| Command | Arguments |
| --- | --- |
| `BOOK` | book ID, initial copies |
| `MEMBER` | member ID, borrowing limit |
| `BORROW`, `RENEW` | member ID, book ID, days |
| `RETURN`, `HOLD`, `CANCEL`, `LOSS` | member ID, book ID |
| `RESTOCK`, `REMOVE` | book ID, copies |
| `BLOCK`, `UNBLOCK`, `CLOSE` | member ID |
| `PAY`, `LIMIT` | member ID, amount or new limit |
| `TICK` | elapsed days |
| `RETIRE` | book ID |

Reservation priority follows insertion tickets. A successful checkout consumes
its matching first reservation. Renewal permits two extensions, each at most
365 days, and refuses overdue loans or outstanding reservations. Late returns
charge two units per overdue day; loss charges 100 units. Debt prevents new
loans and reservations. Payments cannot exceed the outstanding balance.

Domain rejections update audit and rejection counters without changing catalog,
loan, account, or reservation data. Syntax and I/O failures abort processing.
Exit codes are 64 for arguments, 65 for input, 70 for invariant failure, 71 for
allocation/argument availability, and 74 for I/O. Diagnostics include line
numbers for journal processing failures.

## Reports and consistency

Reports contain `STATE` counters, active `BOOK`, `MEMBER`, `LOAN`, and `HOLD`
records in slot order, followed by the last 64 `AUDIT` entries in chronological
order. Audit operation numbers follow the command order in `model.Operation`;
reason numbers follow `model.Reason`, starting with accepted at zero.
`report.fe` defines the exact field ordering.

Checks reconcile available stock against active loans, cached borrowing counts,
unique IDs and reservation tickets, active references, historical circulation
counters, and fees against payments plus outstanding balances.

## Validation and scope

Eleven native Fermi tests cover circulation, queue priority, renewals, debt,
blocking, loss, slot reuse, all four record capacities, invariant corruption,
and streaming syntax. A separate C catalog model generates four seeded journals
of 4,096 commands each and complete expected reports, including the audit ring.
The integration script compares reports byte for byte at `-O0` and `-O2` and
checks malformed input, byte/command limits, argument errors, and overwrite
refusal. The independent randomized oracle covers catalog operations; circulation
is covered by the native tests and the executable sample, not that oracle.

This is a bounded batch system, not a concurrent service or database. Limits are
32 book records, 32 members, 64 active loans, 64 holds, four holds per member,
eight loans per member, 1,000 copies per book, a 1 MiB journal, and 16,384 commands.
The journal is the replay input; reports cannot restore state. Input is processed
before report creation, but a write failure may leave a partial new report.

The exercise exposed gaps in dynamic record collections, owned text, and
aggregate initialization. Repeated initialization and owned std.text.Text now
address the latter two; resource-containing record arrays remain unsupported.
Numeric IDs and fixed arrays keep this application executable with current Fermi;
free-form titles, database adapters, networking, and concurrency remain outside
its scope. No claim of general production readiness follows from this test.
