# CustomDB Engine MVP (C++17)

A compact SQL database engine built from scratch for a strong systems/resume project.

## Features implemented

- SQL-like REPL
- `CREATE TABLE`
- `INSERT INTO`
- `SELECT * FROM ...` with optional `WHERE col = value`
- `CREATE INDEX ... ON table(column)` for integer columns
- Tiny from-scratch B-Tree index for equality lookup on integer keys
- Extremely simple query planner that chooses either:
  - `INDEX SEEK` when an index exists on the filtered column
  - `FULL TABLE SCAN` otherwise
- Basic transaction flow:
  - `BEGIN`
  - `COMMIT`
  - `ROLLBACK`
- Disk persistence for table rows under `data/tables/`

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Or without CMake:

```bash
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp -o customdb
```

## Run

```bash
./customdb
```

If using CMake from `build/`:

```bash
./customdb
```

## Example commands

```sql
CREATE TABLE users (id INT, name TEXT, age INT);
INSERT INTO users VALUES (1, 'Andrew', 20);
INSERT INTO users VALUES (2, 'Maya', 22);
INSERT INTO users VALUES (3, 'Chris', 21);
SELECT * FROM users;
SELECT * FROM users WHERE id = 2;
CREATE INDEX idx_users_id ON users(id);
SELECT * FROM users WHERE id = 2;
BEGIN;
INSERT INTO users VALUES (4, 'Sara', 24);
ROLLBACK;
SELECT * FROM users;
BEGIN;
INSERT INTO users VALUES (5, 'Nolan', 23);
COMMIT;
SELECT * FROM users;
.quit
```

## Next upgrades

1. Add a write-ahead log (WAL) for crash-safe transactions.
2. Add secondary index persistence instead of rebuilding indexes at startup.
3. Support projections like `SELECT name, age FROM users`.
4. Add joins and a simple cost model.
5. Add a page manager with fixed-size pages.
6. Add delete/update support.
7. Add MVCC or lock-based concurrency.
4