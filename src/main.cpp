#include "crow.h"
#include <iostream>
#include <sqlite3.h>


// Helper function to check if a string ends with a specific suffix
bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() >= suffix.length()) {
        return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
    } else {
        return false;
    }
}

// Function to create the database and user table if not exists
void initialize_database() {
    sqlite3* DB;
    int exit = 0;
    exit = sqlite3_open("securebank.db", &DB);

    std::string sql = "CREATE TABLE IF NOT EXISTS USERS ("
                      "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "USERNAME TEXT NOT NULL, "
                      "PASSWORD TEXT NOT NULL);";
    
    char* messageError;
    exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messageError);

    if (exit != SQLITE_OK) {
        std::cerr << "Error creating table: " << messageError << std::endl;
        sqlite3_free(messageError);
    }
    
    sqlite3_close(DB);
}

// Function to execute an SQL statement
bool execute_sql(const std::string& sql) {
    sqlite3* DB;
    int exit = sqlite3_open("securebank.db", &DB);

    char* messageError;
    exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messageError);
    
    if (exit != SQLITE_OK) {
        std::cerr << "SQL error: " << messageError << std::endl;
        sqlite3_free(messageError);
        sqlite3_close(DB);
        return false;
    }
    
    sqlite3_close(DB);
    return true;
}

// Function to check if a user exists
bool user_exists(const std::string& username) {
    sqlite3* DB;
    sqlite3_stmt* stmt;
    int exit = sqlite3_open("securebank.db", &DB);
    
    std::string sql = "SELECT 1 FROM USERS WHERE USERNAME = ?;";
    sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    
    int step = sqlite3_step(stmt);
    bool exists = (step == SQLITE_ROW);
    
    sqlite3_finalize(stmt);
    sqlite3_close(DB);
    return exists;
}

// Function to verify user credentials
bool verify_user(const std::string& username, const std::string& password) {
    sqlite3* DB;
    sqlite3_stmt* stmt;
    int exit = sqlite3_open("securebank.db", &DB);
    
    std::string sql = "SELECT 1 FROM USERS WHERE USERNAME = ? AND PASSWORD = ?;";
    sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    
    int step = sqlite3_step(stmt);
    bool verified = (step == SQLITE_ROW);
    
    sqlite3_finalize(stmt);
    sqlite3_close(DB);
    return verified;
}

int main()
{
    initialize_database();

    // Define my Crow app
    crow::SimpleApp app;

    // Serve static files from the templates directory
    app.route_dynamic("/<path>")
        .name("static")
        ([](const crow::request& req, crow::response& res, std::string path) {
            std::string file_path = "./templates/" + path;
            std::ifstream file(file_path.c_str(), std::ios::binary);

            if (!file) {
                res.code = 404;
                res.end("File not found");
                return;
            }

            res.body.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (ends_with(path, ".css")) {
                res.add_header("Content-Type", "text/css");
            } else if (ends_with(path, ".html")) {
                res.add_header("Content-Type", "text/html");
            }
            res.end();
        });

    // Define endpoint for index route
    CROW_ROUTE(app, "/")([]() {
        auto page = crow::mustache::load("index.html");
        crow::mustache::context ctx;
        return crow::response{page.render(ctx)};
    });

    // Define endpoint for register page
    CROW_ROUTE(app, "/register")([]() {
        auto page = crow::mustache::load("register.html");
        crow::mustache::context ctx;
        return crow::response{page.render(ctx)};
    });

    // Handle registration form submission
    CROW_ROUTE(app, "/register").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) {
            return crow::response(400, "Invalid input");
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s();

        if (user_exists(username)) {
            return crow::response(400, "Username already exists");
        }

        std::string sql = "INSERT INTO USERS (USERNAME, PASSWORD) VALUES ('" + username + "', '" + password + "');";
        if (execute_sql(sql)) {
            return crow::response(200, "Registration successful");
        } else {
            return crow::response(500, "Failed to register user");
        }
    });

    // Define endpoint for login page
    CROW_ROUTE(app, "/login")([]() {
        auto page = crow::mustache::load("login.html");
        crow::mustache::context ctx;
        return crow::response{page.render(ctx)};
    });

    // Handle login form submission
    CROW_ROUTE(app, "/login").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) {
            return crow::response(400, "Invalid input");
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s();

        if (verify_user(username, password)) {
            return crow::response(200, "Login successful");
        } else {
            return crow::response(401, "Invalid username or password");
        }
    });

    // Set port and then multiple threads for the app to run
    app.port(18080).multithreaded().run();
}
