#include <iostream>
#include <string>
#include <limits>
#include "sqlite3.h"

// Structure to hold current logged-in user information
struct User {
    std::string username;
    std::string userType; // "job_seeker" or "employer"
    bool isLoggedIn = false;
};

// Global variables
sqlite3* db;
User currentLoggedInUser;

// --- Callback for SELECT queries ---
int callback(void* data, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; i++) {
        std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    std::cout << std::endl;
    return 0;
}

// --- Ensure table exists ---
void ensureTable(const std::string& tableName, const std::string& createSql) {
    char* zErrMsg = nullptr;
    int rc = sqlite3_exec(db, createSql.c_str(), 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error (" << tableName << "): " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Table '" << tableName << "' ready.\n";
    }
}

// --- Initialize DB ---
void initializeDatabase() {
    int rc = sqlite3_open("JobConnect.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        exit(1);
    }

    std::string userTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL,
            email TEXT NOT NULL,
            user_type TEXT NOT NULL CHECK(user_type IN ('job_seeker', 'employer'))
        );
    )";
    ensureTable("users", userTable);

    std::string jobTable = R"(
        CREATE TABLE IF NOT EXISTS jobs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            employer_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            description TEXT,
            requirements TEXT,
            location TEXT,
            salary_range TEXT,
            contact_number TEXT,
            FOREIGN KEY (employer_id) REFERENCES users(id)
        );
    )";
    ensureTable("jobs", jobTable);

    std::string applicationTable = R"(
        CREATE TABLE IF NOT EXISTS applications (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            job_id INTEGER NOT NULL,
            job_seeker_id INTEGER NOT NULL,
            status TEXT DEFAULT 'pending' CHECK(status IN ('pending', 'accepted', 'rejected')),
            application_date DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (job_id) REFERENCES jobs(id),
            FOREIGN KEY (job_seeker_id) REFERENCES users(id)
        );
    )";
    ensureTable("applications", applicationTable);
}

// --- Register ---
void registerUser() {
    std::string username, email, password, userType;
    int choice;

    std::cout << "\n--- Register ---\n";
    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Email: ";
    std::cin >> email;
    std::cout << "Password: ";
    std::cin >> password;
    std::cout << "User Type (1 = Job Seeker, 2 = Employer): ";
    std::cin >> choice;

    userType = (choice == 1) ? "job_seeker" : (choice == 2) ? "employer" : "";

    if (userType.empty()) {
        std::cout << "Invalid type.\n";
        return;
    }

    std::string sql = "INSERT INTO users (username, password, email, user_type) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, userType.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        std::cout << "Registered successfully!\n";
    } else {
        std::cerr << "Registration failed: " << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);
}

// --- Login ---
void loginUser() {
    std::string username, password;
    std::cout << "\n--- Login ---\n";
    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Password: ";
    std::cin >> password;

    std::string sql = "SELECT user_type FROM users WHERE username = ? AND password = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        currentLoggedInUser.username = username;
        currentLoggedInUser.userType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        currentLoggedInUser.isLoggedIn = true;
        std::cout << "Logged in as " << username << "\n";
    } else {
        std::cout << "Invalid credentials.\n";
    }

    sqlite3_finalize(stmt);
}

// --- Logout ---
void logoutUser() {
    currentLoggedInUser = {};
    std::cout << "Logged out.\n";
}

// --- Get User ID ---
int getUserId(const std::string& username) {
    std::string sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;
    int userId = -1;

    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return userId;
}

// --- Post Job ---
void postJob() {
    // Only employers are allowed to post jobs
    if (currentLoggedInUser.userType != "employer") {
        std::cout << "Only employers can post jobs.\n";
        return;
    }

    // Prompt for job details
    std::string title, description, requirements, location, salary, contactNumber;

    std::cout << "\n--- Post a Job ---\n";
    std::cin.ignore(); // Clear input buffer (especially after previous inputs)

    std::cout << "Title: ";
    std::getline(std::cin, title);

    std::cout << "Description: ";
    std::getline(std::cin, description);

    std::cout << "Requirements: ";
    std::getline(std::cin, requirements);

    std::cout << "Location: ";
    std::getline(std::cin, location);

    std::cout << "Salary Range: ";
    std::getline(std::cin, salary);

    std::cout << "Contact Number: ";
    std::getline(std::cin, contactNumber);

    // Get employer ID from the username
    int employerId = getUserId(currentLoggedInUser.username);
    if (employerId == -1) {
        std::cout << "Employer ID not found!\n";
        return;
    }

    // SQL to insert job into database
    std::string sql = "INSERT INTO jobs (employer_id, title, description, requirements, location, salary_range, contact_number) VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;

    // Prepare SQL statement
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "Failed to prepare SQL statement.\n";
        return;
    }

    // Bind values to SQL statement
    sqlite3_bind_int(stmt, 1, employerId);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, requirements.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, location.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, salary.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, contactNumber.c_str(), -1, SQLITE_STATIC);

    // Execute SQL and show result
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        std::cout << "Job posted successfully!\n";
    } else {
        std::cout << "Failed to post job: " << sqlite3_errmsg(db) << "\n";
    }

    // Free memory used by SQL statement
    sqlite3_finalize(stmt);
}


// --- View My Posted Jobs ---
void viewMyPostedJobs() {
    // Check if the current user is an employer
    if (currentLoggedInUser.userType != "employer") {
        std::cout << "Only employers can view their posted jobs.\n";
        return;
    }

    // Get the employer's ID using their username
    int employerId = getUserId(currentLoggedInUser.username);
    if (employerId == -1) {
        std::cerr << "Employer ID not found!\n";
        return;
    }

    // Prepare SQL query to get jobs posted by this employer
    std::string sql = "SELECT id, title, location, salary_range, contact_number "
                      "FROM jobs WHERE employer_id = " + std::to_string(employerId) + ";";

    std::cout << "\n--- My Posted Jobs ---\n";

    // Execute the query and use the callback to display results
    char* errMsg = nullptr;
    int result = sqlite3_exec(db, sql.c_str(), callback, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        std::cerr << "Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
    }
}


// --- View All Jobs ---
void viewAllJobs() {
    if (currentLoggedInUser.userType != "job_seeker") {
        std::cout << "Only job seekers can view jobs.\n";
        return;
    }

    std::string sql = "SELECT id, title, description, requirements, location, salary_range, contact_number FROM jobs;";
    std::cout << "\n--- Available Jobs ---\n";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), callback, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
    }
}

// --- Apply for Job ---
void applyForJob() {
    // Only job seekers can apply
    if (currentLoggedInUser.userType != "job_seeker") {
        std::cout << "Only job seekers can apply for jobs.\n";
        return;
    }

    // Ask for job ID
    int jobId;
    std::cout << "Enter Job ID to apply: ";
    std::cin >> jobId;

    // Get current user's ID
    int seekerId = getUserId(currentLoggedInUser.username);

    // Check if already applied
    std::string checkQuery = "SELECT COUNT(*) FROM applications WHERE job_id = ? AND job_seeker_id = ?;";
    sqlite3_stmt* checkStmt;
    sqlite3_prepare_v2(db, checkQuery.c_str(), -1, &checkStmt, nullptr);
    sqlite3_bind_int(checkStmt, 1, jobId);
    sqlite3_bind_int(checkStmt, 2, seekerId);

    int alreadyApplied = 0;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        alreadyApplied = sqlite3_column_int(checkStmt, 0);
    }
    sqlite3_finalize(checkStmt);

    // If already applied, notify and exit
    if (alreadyApplied > 0) {
        std::cout << "You have already applied for this job.\n";
        return;
    }

    // Otherwise, insert new application
    std::string applyQuery = "INSERT INTO applications (job_id, job_seeker_id) VALUES (?, ?);";
    sqlite3_stmt* insertStmt;
    sqlite3_prepare_v2(db, applyQuery.c_str(), -1, &insertStmt, nullptr);
    sqlite3_bind_int(insertStmt, 1, jobId);
    sqlite3_bind_int(insertStmt, 2, seekerId);

    if (sqlite3_step(insertStmt) == SQLITE_DONE) {
        std::cout << "Application submitted successfully!\n";
    } else {
        std::cerr << "Failed to apply: " << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(insertStmt);
}


// --- View My Applications ---
void viewMyApplications() {
    if (currentLoggedInUser.userType != "job_seeker") {
        std::cout << "Only job seekers can view their applications.\n";
        return;
    }

    int seekerId = getUserId(currentLoggedInUser.username);
    std::string sql = "SELECT job_id, status, application_date FROM applications WHERE job_seeker_id = " + std::to_string(seekerId) + ";";
    std::cout << "\n--- My Applications ---\n";
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), callback, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
    }
}

// --- Main Menu (Simplified) ---
void showMainMenu() {
    int choice;
    do {
        std::cout << "\n=== JobConnect Menu ===\n";
        std::cout << "1. Register\n";
        std::cout << "2. Login\n";
        std::cout << "3. Post Job\n";
        std::cout << "4. View My Posted Jobs\n";
        std::cout << "5. View All Jobs\n";
        std::cout << "6. Apply for Job\n";
        std::cout << "7. View My Applications\n";
        std::cout << "8. Logout\n";
        std::cout << "9. Exit\n";
        std::cout << "Choice: ";
        std::cin >> choice;

        bool loggedIn = currentLoggedInUser.isLoggedIn;

        switch (choice) {
            case 1:
                registerUser();
                break;
            case 2:
                loginUser();
                break;
            case 3:
                if (loggedIn) postJob();
                else std::cout << "Please login first.\n";
                break;
            case 4:
                if (loggedIn) viewMyPostedJobs();
                else std::cout << "Please login first.\n";
                break;
            case 5:
                if (loggedIn) viewAllJobs();
                else std::cout << "Please login first.\n";
                break;
            case 6:
                if (loggedIn) applyForJob();
                else std::cout << "Please login first.\n";
                break;
            case 7:
                if (loggedIn) viewMyApplications();
                else std::cout << "Please login first.\n";
                break;
            case 8:
                logoutUser();
                break;
            case 9:
                std::cout << "Exiting...\n";
                break;
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
        }
    } while (choice != 9);
}

// --- Main ---
int main() {
    initializeDatabase();
    showMainMenu();
    sqlite3_close(db);
    return 0;
}
