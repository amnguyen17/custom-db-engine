# CustomDB Engine MVP (C++17)

This project is a simple SQL database engine written in C++17. I built it to better understand how relational databases work internally, including how they store data, execute queries, use indexes, and manage transactions.

## Features

- SQL-like interactive command-line interface (REPL)
- `CREATE TABLE`
- `INSERT INTO`
- `SELECT * FROM ...` with optional `WHERE column = value` filtering
- `CREATE INDEX ... ON table(column)` for integer columns
- B-tree index implementation for fast equality lookups on integer keys
- Simple query planner that chooses between:
  - `INDEX SEEK` when an index exists on the filtered column
  - `FULL TABLE SCAN` otherwise
- Basic transaction support:
  - `BEGIN`
  - `COMMIT`
  - `ROLLBACK`
- Disk persistence for table data under `data/tables/`

## What I Learned

Building this project helped me gain a better understanding of:

- Database storage and file persistence
- B-tree indexes and how they improve query performance
- Basic query planning and execution
- Transaction handling using commit and rollback
- Designing a larger C++ project with modular components

## Building the Project

Using CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Or compile directly with g++:

```bash
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp -o customdb
```

## Running

```bash
./customdb
```

If using CMake, run the executable from the `build/` directory.

## Example

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

BEGIN;
INSERT INTO users VALUES (5, 'Nolan', 23);
COMMIT;

SELECT * FROM users;

.quit
```

## Future Improvements

- Add a write-ahead log (WAL) for crash recovery
- Persist indexes instead of rebuilding them at startup
- Support column projections (e.g. `SELECT name, age FROM users`)
- Add `UPDATE` and `DELETE` statements
- Implement joins between tables
- Add a page manager with fixed-size pages
- Improve the query planner with a simple cost model
- Explore MVCC or lock-based concurrency