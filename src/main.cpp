
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) b--;
    return s.substr(a, b-a);
}

static std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool starts_with_i(const std::string& s, const std::string& prefix) {
    return upper(s).rfind(upper(prefix), 0) == 0;
}

static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quote = false;
    for (char c : s) {
        if (c == '\'') { in_quote = !in_quote; cur.push_back(c); }
        else if (c == ',' && !in_quote) { out.push_back(trim(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty() || !s.empty()) out.push_back(trim(cur));
    return out;
}

static std::string unquote(std::string s) {
    s = trim(s);
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') return s.substr(1, s.size()-2);
    return s;
}

static std::string escape_field(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\\' || c == '|') r.push_back('\\');
        r.push_back(c);
    }
    return r;
}

static std::vector<std::string> split_row(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool esc = false;
    for (char c : line) {
        if (esc) { cur.push_back(c); esc = false; }
        else if (c == '\\') esc = true;
        else if (c == '|') { fields.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    fields.push_back(cur);
    return fields;
}

enum class Type { INT, TEXT };

struct Column { std::string name; Type type; };
struct Row { std::vector<std::string> values; };

struct Table {
    std::string name;
    std::vector<Column> columns;
    std::vector<Row> rows;
};

// Minimal B-Tree for integer equality lookups. Stores key -> row ids.
class BTree {
    static constexpr int T = 3;
    struct Node {
        bool leaf = true;
        std::vector<int> keys;
        std::vector<std::vector<int>> row_ids;
        std::vector<std::unique_ptr<Node>> child;
    };
    std::unique_ptr<Node> root;

    std::vector<int>* search(Node* x, int k) {
        size_t i = 0;
        while (i < x->keys.size() && k > x->keys[i]) i++;
        if (i < x->keys.size() && k == x->keys[i]) return &x->row_ids[i];
        if (x->leaf) return nullptr;
        return search(x->child[i].get(), k);
    }

    void split_child(Node* x, size_t i) {
        auto z = std::make_unique<Node>();
        Node* y = x->child[i].get();
        z->leaf = y->leaf;

        int mid_key = y->keys[T-1];
        std::vector<int> mid_rows = y->row_ids[T-1];

        for (int j = 0; j < T-1; ++j) {
            z->keys.push_back(y->keys[j+T]);
            z->row_ids.push_back(y->row_ids[j+T]);
        }
        if (!y->leaf) {
            for (int j = 0; j < T; ++j) z->child.push_back(std::move(y->child[j+T]));
            y->child.resize(T);
        }
        y->keys.resize(T-1);
        y->row_ids.resize(T-1);

        x->child.insert(x->child.begin() + static_cast<long>(i + 1), std::move(z));
        x->keys.insert(x->keys.begin() + static_cast<long>(i), mid_key);
        x->row_ids.insert(x->row_ids.begin() + static_cast<long>(i), mid_rows);
    }

    void insert_nonfull(Node* x, int k, int row_id) {
        int i = static_cast<int>(x->keys.size()) - 1;
        if (x->leaf) {
            auto it = std::lower_bound(x->keys.begin(), x->keys.end(), k);
            size_t pos = static_cast<size_t>(it - x->keys.begin());
            if (it != x->keys.end() && *it == k) {
                x->row_ids[pos].push_back(row_id);
                return;
            }
            x->keys.insert(it, k);
            x->row_ids.insert(x->row_ids.begin() + static_cast<long>(pos), std::vector<int>{row_id});
        } else {
            while (i >= 0 && k < x->keys[static_cast<size_t>(i)]) i--;
            if (i >= 0 && k == x->keys[static_cast<size_t>(i)]) {
                x->row_ids[static_cast<size_t>(i)].push_back(row_id);
                return;
            }
            i++;
            if (x->child[static_cast<size_t>(i)]->keys.size() == 2*T - 1) {
                split_child(x, static_cast<size_t>(i));
                if (k > x->keys[static_cast<size_t>(i)]) i++;
                else if (k == x->keys[static_cast<size_t>(i)]) {
                    x->row_ids[static_cast<size_t>(i)].push_back(row_id);
                    return;
                }
            }
            insert_nonfull(x->child[static_cast<size_t>(i)].get(), k, row_id);
        }
    }

public:
    BTree() : root(std::make_unique<Node>()) {}

    void insert(int k, int row_id) {
        if (root->keys.size() == 2*T - 1) {
            auto s = std::make_unique<Node>();
            s->leaf = false;
            s->child.push_back(std::move(root));
            root = std::move(s);
            split_child(root.get(), 0);
        }
        insert_nonfull(root.get(), k, row_id);
    }

    std::vector<int> find(int k) {
        auto p = search(root.get(), k);
        if (!p) return {};
        return *p;
    }
};

struct Index {
    std::string name;
    std::string table;
    std::string column;
    BTree tree;
};

class Database {
    std::string data_dir = "data";
    std::unordered_map<std::string, Table> tables;
    std::unordered_map<std::string, Index> indexes;
    bool in_tx = false;
    std::unordered_map<std::string, std::vector<Row>> tx_inserts;

    int col_index(const Table& t, const std::string& col) const {
        for (size_t i = 0; i < t.columns.size(); ++i)
            if (upper(t.columns[i].name) == upper(col)) return static_cast<int>(i);
        return -1;
    }

    std::string table_path(const std::string& name) const {
        return data_dir + "/tables/" + name + ".tbl";
    }

    void save_table(const Table& t) {
        fs::create_directories(data_dir + "/tables");
        std::ofstream out(table_path(t.name), std::ios::trunc);
        for (size_t i = 0; i < t.columns.size(); ++i) {
            if (i) out << ",";
            out << t.columns[i].name << ":" << (t.columns[i].type == Type::INT ? "INT" : "TEXT");
        }
        out << "\n";
        for (const auto& r : t.rows) {
            for (size_t i = 0; i < r.values.size(); ++i) {
                if (i) out << "|";
                out << escape_field(r.values[i]);
            }
            out << "\n";
        }
    }

    void save_catalog() {
        fs::create_directories(data_dir);
        std::ofstream out(data_dir + "/catalog.txt", std::ios::trunc);
        for (const auto& [name, idx] : indexes) {
            out << "INDEX|" << idx.name << "|" << idx.table << "|" << idx.column << "\n";
        }
    }

    void rebuild_index(Index& idx) {
        idx.tree = BTree();
        auto& t = tables.at(idx.table);
        int c = col_index(t, idx.column);
        if (c < 0) return;
        for (size_t i = 0; i < t.rows.size(); ++i) idx.tree.insert(std::stoi(t.rows[i].values[static_cast<size_t>(c)]), static_cast<int>(i));
    }

    void apply_insert(const std::string& table, const Row& r) {
        auto& t = tables.at(table);
        int row_id = static_cast<int>(t.rows.size());
        t.rows.push_back(r);
        for (auto& [_, idx] : indexes) {
            if (upper(idx.table) == upper(table)) {
                int c = col_index(t, idx.column);
                if (c >= 0) idx.tree.insert(std::stoi(r.values[static_cast<size_t>(c)]), row_id);
            }
        }
        save_table(t);
    }

public:
    Database() { load(); }

    void load() {
        fs::create_directories(data_dir + "/tables");
        for (const auto& ent : fs::directory_iterator(data_dir + "/tables")) {
            if (ent.path().extension() != ".tbl") continue;
            std::ifstream in(ent.path());
            std::string line;
            if (!std::getline(in, line)) continue;
            Table t;
            t.name = ent.path().stem().string();
            for (const auto& part : split_csv(line)) {
                auto p = part.find(':');
                if (p == std::string::npos) continue;
                Column c{part.substr(0, p), upper(part.substr(p+1)) == "INT" ? Type::INT : Type::TEXT};
                t.columns.push_back(c);
            }
            while (std::getline(in, line)) if (!line.empty()) t.rows.push_back(Row{split_row(line)});
            tables[t.name] = t;
        }
        std::ifstream cat(data_dir + "/catalog.txt");
        std::string line;
        while (std::getline(cat, line)) {
            auto f = split_row(line);
            if (f.size() == 4 && f[0] == "INDEX") {
                Index idx{f[1], f[2], f[3], BTree()};
                indexes[idx.name] = std::move(idx);
            }
        }
        for (auto& [_, idx] : indexes) if (tables.count(idx.table)) rebuild_index(idx);
    }

    void create_table(const std::string& sql) {
        auto l = sql.find('('), r = sql.rfind(')');
        if (l == std::string::npos || r == std::string::npos || r <= l) throw std::runtime_error("Invalid CREATE TABLE syntax");
        std::string head = trim(sql.substr(0, l));
        std::istringstream hs(head);
        std::string create_kw, table_kw, name;
        hs >> create_kw >> table_kw >> name;
        if (name.empty()) throw std::runtime_error("Missing table name");
        Table t; t.name = name;
        for (const auto& def : split_csv(sql.substr(l+1, r-l-1))) {
            std::istringstream ds(def);
            std::string cname, ctype;
            ds >> cname >> ctype;
            if (cname.empty() || ctype.empty()) throw std::runtime_error("Invalid column definition");
            std::string utype = upper(ctype);
            if (utype != "INT" && utype != "TEXT") throw std::runtime_error("Only INT and TEXT are supported");
            t.columns.push_back(Column{cname, utype == "INT" ? Type::INT : Type::TEXT});
        }
        tables[name] = t;
        save_table(tables[name]);
        std::cout << "OK: created table " << name << "\n";
    }

    void insert_into(const std::string& sql) {
        auto up = upper(sql);
        size_t into = up.find("INTO"), values = up.find("VALUES");
        auto l = sql.find('(', values), r = sql.rfind(')');
        if (into == std::string::npos || values == std::string::npos || l == std::string::npos || r == std::string::npos) throw std::runtime_error("Invalid INSERT syntax");
        std::string table = trim(sql.substr(into + 4, values - (into + 4)));
        if (!tables.count(table)) throw std::runtime_error("Unknown table: " + table);
        auto vals = split_csv(sql.substr(l+1, r-l-1));
        Row row;
        for (auto& v : vals) row.values.push_back(unquote(v));
        if (row.values.size() != tables[table].columns.size()) throw std::runtime_error("Column count does not match value count");
        for (size_t i = 0; i < row.values.size(); ++i) {
            if (tables[table].columns[i].type == Type::INT) std::stoi(row.values[i]);
        }
        if (in_tx) {
            tx_inserts[table].push_back(row);
            std::cout << "OK: queued insert in transaction\n";
        } else {
            apply_insert(table, row);
            std::cout << "OK: inserted 1 row\n";
        }
    }

    void create_index(const std::string& sql) {
        // CREATE INDEX idx ON users(id)
        auto up = upper(sql);
        size_t on = up.find(" ON ");
        auto l = sql.find('(', on), r = sql.rfind(')');
        if (on == std::string::npos || l == std::string::npos || r == std::string::npos) throw std::runtime_error("Invalid CREATE INDEX syntax");
        std::istringstream hs(sql.substr(0, on));
        std::string create_kw, index_kw, idx_name;
        hs >> create_kw >> index_kw >> idx_name;
        std::string table = trim(sql.substr(on + 4, l - (on + 4)));
        std::string column = trim(sql.substr(l+1, r-l-1));
        if (!tables.count(table)) throw std::runtime_error("Unknown table: " + table);
        int c = col_index(tables[table], column);
        if (c < 0) throw std::runtime_error("Unknown column: " + column);
        if (tables[table].columns[static_cast<size_t>(c)].type != Type::INT) throw std::runtime_error("MVP indexes only support INT columns");
        Index idx{idx_name, table, column, BTree()};
        indexes[idx_name] = std::move(idx);
        rebuild_index(indexes[idx_name]);
        save_catalog();
        std::cout << "OK: created B-Tree index " << idx_name << " on " << table << "(" << column << ")\n";
    }

    void select_from(const std::string& sql) {
        auto up = upper(sql);
        size_t from = up.find("FROM");
        if (from == std::string::npos) throw std::runtime_error("Invalid SELECT syntax");
        size_t where = up.find("WHERE", from);
        std::string table = where == std::string::npos ? trim(sql.substr(from + 4)) : trim(sql.substr(from + 4, where - (from + 4)));
        if (!tables.count(table)) throw std::runtime_error("Unknown table: " + table);
        auto& t = tables[table];
        std::optional<std::pair<std::string, std::string>> pred;
        if (where != std::string::npos) {
            std::string clause = trim(sql.substr(where + 5));
            auto eq = clause.find('=');
            if (eq == std::string::npos) throw std::runtime_error("Only WHERE col = value is supported");
            pred = { {trim(clause.substr(0, eq)), unquote(trim(clause.substr(eq+1)))} };
        }
        std::vector<int> candidate_rows;
        std::string plan = "FULL TABLE SCAN";
        if (pred) {
            int c = col_index(t, pred->first);
            if (c < 0) throw std::runtime_error("Unknown column: " + pred->first);
            for (auto& [_, idx] : indexes) {
                if (upper(idx.table) == upper(table) && upper(idx.column) == upper(pred->first) && t.columns[static_cast<size_t>(c)].type == Type::INT) {
                    candidate_rows = idx.tree.find(std::stoi(pred->second));
                    plan = "INDEX SEEK using " + idx.name;
                    break;
                }
            }
        }
        std::cout << "Plan: " << plan << "\n";
        for (size_t i = 0; i < t.columns.size(); ++i) std::cout << (i ? " | " : "") << t.columns[i].name;
        std::cout << "\n";
        std::cout << std::string(40, '-') << "\n";
        auto matches = [&](int row_id) {
            if (!pred) return true;
            int c = col_index(t, pred->first);
            return t.rows[static_cast<size_t>(row_id)].values[static_cast<size_t>(c)] == pred->second;
        };
        int count = 0;
        if (plan.rfind("INDEX SEEK", 0) == 0) {
            for (int id : candidate_rows) if (id >= 0 && id < static_cast<int>(t.rows.size()) && matches(id)) {
                const auto& row = t.rows[static_cast<size_t>(id)];
                for (size_t i = 0; i < row.values.size(); ++i) std::cout << (i ? " | " : "") << row.values[i];
                std::cout << "\n"; count++;
            }
        } else {
            for (size_t id = 0; id < t.rows.size(); ++id) if (matches(static_cast<int>(id))) {
                const auto& row = t.rows[id];
                for (size_t i = 0; i < row.values.size(); ++i) std::cout << (i ? " | " : "") << row.values[i];
                std::cout << "\n"; count++;
            }
        }
        std::cout << count << " row(s)\n";
    }

    void begin() {
        if (in_tx) throw std::runtime_error("Transaction already active");
        in_tx = true; tx_inserts.clear();
        std::cout << "OK: transaction started\n";
    }

    void commit() {
        if (!in_tx) throw std::runtime_error("No active transaction");
        int n = 0;
        for (auto& [table, rows] : tx_inserts) for (auto& r : rows) { apply_insert(table, r); n++; }
        tx_inserts.clear(); in_tx = false;
        std::cout << "OK: committed " << n << " row(s)\n";
    }

    void rollback() {
        if (!in_tx) throw std::runtime_error("No active transaction");
        tx_inserts.clear(); in_tx = false;
        std::cout << "OK: rolled back transaction\n";
    }

    void execute(std::string sql) {
        sql = trim(sql);
        if (!sql.empty() && sql.back() == ';') sql.pop_back();
        sql = trim(sql);
        if (sql.empty()) return;
        if (starts_with_i(sql, "CREATE TABLE")) create_table(sql);
        else if (starts_with_i(sql, "INSERT INTO")) insert_into(sql);
        else if (starts_with_i(sql, "CREATE INDEX")) create_index(sql);
        else if (starts_with_i(sql, "SELECT")) select_from(sql);
        else if (upper(sql) == "BEGIN") begin();
        else if (upper(sql) == "COMMIT") commit();
        else if (upper(sql) == "ROLLBACK") rollback();
        else throw std::runtime_error("Unknown command");
    }
};

int main() {
    std::cout << "CustomDB Engine MVP - type .quit to exit\n";
    std::cout << "Supports CREATE TABLE, INSERT, SELECT, CREATE INDEX, BEGIN/COMMIT/ROLLBACK\n";
    Database db;
    std::string line, sql;
    while (true) {
        std::cout << (sql.empty() ? "db> " : "... ");
        if (!std::getline(std::cin, line)) break;
        if (trim(line) == ".quit" || trim(line) == ".exit") break;
        sql += " " + line;
        if (line.find(';') != std::string::npos || upper(trim(line)) == "BEGIN" || upper(trim(line)) == "COMMIT" || upper(trim(line)) == "ROLLBACK") {
            try { db.execute(sql); }
            catch (const std::exception& e) { std::cout << "ERROR: " << e.what() << "\n"; }
            sql.clear();
        }
    }
    return 0;
}
