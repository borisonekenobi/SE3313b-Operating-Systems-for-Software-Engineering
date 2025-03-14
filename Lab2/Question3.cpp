#include <condition_variable>
#include <iomanip>
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

#define FIXED_FLOAT(x) fixed << setprecision(2) << (x)
#define CHECKMARK "\u001b[32m✓\u001b[37m"
#define CROSSMARK "\u001b[31m✗\u001b[37m"

using namespace std;

string country;

class TemperatureMonitor
{
    int num_threads;
    int num_threads_submitted = 0;
    string current_date;
    chrono::system_clock::time_point first_timestamp = {};
    vector<float> temperatures;
    mutable mutex mtx;
    condition_variable cvRead, cvWrite;

public:
    explicit TemperatureMonitor(const int num_threads) : num_threads(num_threads)
    {
    }

    void setNumThreads(const int num_threads)
    {
        this->num_threads = num_threads;
    }

    void addTemperature(const string& date, const float temp)
    {
        unique_lock lock(mtx);
        if (!current_date.empty() && date != current_date) cvWrite.wait(lock);

        current_date = date;
        temperatures.push_back(temp);

        if (++num_threads_submitted == num_threads)
        {
            cvRead.notify_one();
        }
    }

    void printAverageTemperature()
    {
        unique_lock lock(mtx);
        if (num_threads_submitted < num_threads) cvRead.wait(lock);
        if (num_threads_submitted == 0) return;
        if (first_timestamp.time_since_epoch().count() == 0) first_timestamp = chrono::system_clock::now();

        float sum = 0.0;
        for (const float temp : temperatures)
        {
            sum += temp;
        }

        const auto now = chrono::system_clock::now();
        const auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - first_timestamp).count();
        cout << "[" << current_date << "], [" << country << "], [Temperature: " <<
            FIXED_FLOAT(sum / static_cast<float>(num_threads)) << "°C], [" << elapsed << "ms]" <<
            endl;

        current_date.clear();
        temperatures.clear();
        num_threads_submitted = 0;
        cvWrite.notify_all();
    }

    void unlockAll()
    {
        cvRead.notify_all();
        cvWrite.notify_all();
    }
};

chrono::time_point<chrono::system_clock> start;
vector<tuple<string, float, int>> averageTemperatures;
TemperatureMonitor monitor(0);
bool keepAlive = true;

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

        monitor.addTemperature(date, stof(temperature));
    }

    return nullptr;
}

void* averageThread(void* args)
{
    while (keepAlive)
    {
        monitor.printAverageTemperature();
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

    vector<pthread_t> threads;
    monitor.setNumThreads(static_cast<int>(query_results.size()));

    for (auto& row : query_results)
    {
        cout << "Starting thread for city: " << row[0] << ", " << row[1] << ", " << row[2] << " ";
        pthread_t thread;
        if (pthread_create(&thread, nullptr, cityThread, &row[0]))
        {
            cout << CROSSMARK << endl;
            return EXIT_FAILURE;
        }
        cout << CHECKMARK << endl;
        threads.push_back(thread);
    }

    cout << "Starting thread for country averages ";
    pthread_t thread;
    if (pthread_create(&thread, nullptr, averageThread, nullptr))
    {
        cout << CROSSMARK << endl;
        return EXIT_FAILURE;
    }
    cout << CHECKMARK << endl;

    for (const auto& t : threads)
    {
        pthread_join(t, nullptr);
    }

    keepAlive = false;
    monitor.unlockAll();

    return EXIT_SUCCESS;
}
