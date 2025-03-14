#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>
#include "sqlite3.h"
#include "utils.h"

#define DATE 0
#define TEMP_SUM 1
#define COUNT 2

#define CHECKMARK "\u001b[32m✓\u001b[37m"

using namespace std;

string country;
vector<tuple<string, float, int>> averageTemperatures;
mutex mtx;

// Function to execute a query and return results
vector<vector<string>> executeQuery(sqlite3* db, const string& query)
{
    char* errMsg = nullptr;
    vector<vector<string>> results;

    if (sqlite3_exec(db, query.c_str(), [](void* data, const int argc, char** argv, char** colName) -> int
    {
        auto* results = static_cast<vector<vector<string>>*>(data);
        vector<string> row;
        row.reserve(argc);
        for (int i = 0; i < argc; i++)
        {
            row.emplace_back(argv[i] ? argv[i] : "NULL");
        }
        results->push_back(row);
        return 0;
    }, &results, &errMsg) != SQLITE_OK)
    {
        cerr << "Error executing query: " << errMsg << endl;
        sqlite3_free(errMsg);
    }

    return results;
}

vector<vector<string>> query(const string& query)
{
    sqlite3* db;
    if (sqlite3_open("lab2.db", &db))
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        exit(EXIT_FAILURE);
    }
    const vector<vector<string>> query_results = executeQuery(db, query);
    sqlite3_close(db);

    return query_results;
}

void writeFile()
{
    ofstream file(country + ".txt");
    if (!file.is_open())
    {
        cerr << "Failed to create file!" << endl;
        exit(EXIT_FAILURE);
    }

    for (const auto& row : averageTemperatures)
    {
        file << get<DATE>(row) << " : " << get<TEMP_SUM>(row) / static_cast<float>(get<COUNT>(row)) << endl;
    }

    file.close();
}

void addTempToAverage(const string& date, const string& temperature)
{
    bool found = false;

    auto it = averageTemperatures.end();
    while (it != averageTemperatures.begin())
    {
        --it;
        if (date == get<DATE>(*it))
        {
            found = true;
            get<TEMP_SUM>(*it) += stof(temperature);
            get<COUNT>(*it)++;
            writeFile();
            break;
        }

        if (date > get<DATE>(*it)) break;
    }

    if (!found)
    {
        averageTemperatures.emplace_back(date, stof(temperature), 1);
        writeFile();
    }
}

void* cityThread(void* args)
{
    const string city_id = *static_cast<string*>(args);

    const auto query_results = query(
        "SELECT date, value "
        "FROM city_data "
        "WHERE city_data.city_id=" + city_id + " "
        "ORDER BY date "
        "COLLATE NOCASE;"
    );

    for (const auto& row : query_results)
    {
        const string& date = row[0];
        const string& temperature = row[1];

        mtx.lock();
        addTempToAverage(date, temperature);
        mtx.unlock();

        sleep(10);
    }

    return nullptr;
}

int main()
{
start:
    cout << "Enter a country name: ";
    getline(cin, country);
    trim(country);

    auto query_results = query(
        "SELECT id, city, country "
        "FROM cities "
        "WHERE country='" + country + "' "
        "COLLATE NOCASE;"
    );

    if (query_results.empty())
    {
        cout << "Country not found." << endl;
        goto start;
    }

    cout << endl;

    for (auto& row : query_results)
    {
        cout << "Starting thread for city: " << row[1] << ", " << row[2] << " ";
        pthread_t thread;
        if (pthread_create(&thread, nullptr, cityThread, &row[0]))
        {
            cerr << "Error creating thread." << endl;
            return EXIT_FAILURE;
        }
        cout << CHECKMARK << endl;
    }

    while (true)
    {
        string input;
        cout << endl << "Enter 'exit' at any time to exit the program." << endl << endl;
        getline(cin, input);

        if (input == "exit") break;
    }

    return EXIT_SUCCESS;
}
