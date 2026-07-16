CREATE TABLE users (id INT, name TEXT, age INT);
INSERT INTO users VALUES (1, 'Andrew', 20);
INSERT INTO users VALUES (2, 'Maya', 22);
INSERT INTO users VALUES (3, 'Chris', 21);
SELECT * FROM users WHERE id = 2;
CREATE INDEX idx_users_id ON users(id);
SELECT * FROM users WHERE id = 2;
BEGIN
INSERT INTO users VALUES (4, 'Sara', 24);
ROLLBACK
SELECT * FROM users;
.quit
