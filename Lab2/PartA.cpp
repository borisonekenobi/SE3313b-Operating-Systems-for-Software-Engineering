#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include "sqlite3.h"
#include "utils.h"

#define PID 0
#define IN_PIPE 1
#define OUT_PIPE 2

using namespace std;

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

void writeCityToChild(const string& data, const tuple<pid_t, int, int>& child_info)
{
    write(get<OUT_PIPE>(child_info), data.c_str(), data.length());
}

void* childThread(void* args)
{
    const string city_id = *static_cast<string*>(args);

    auto query_results = query(
        "SELECT city, population "
        "FROM cities "
        "WHERE id=" + city_id + " "
        "COLLATE NOCASE;"
    );

    const string city = query_results[0][0];
    int population;
    try
    {
        population = stoi(query_results[0][1]);
    }
    catch (const invalid_argument&)
    {
        population = 1;
    }
    const long sleep_duration_μs = (20000000 / population) * 1000000;

    query_results = query(
        "SELECT date, value "
        "FROM cities "
        "JOIN city_data ON cities.id = city_data.city_id "
        "WHERE cities.id=" + city_id + " "
        "ORDER BY date "
        "COLLATE NOCASE;"
    );

    ofstream file(city + city_id + ".txt", ios::trunc);
    if (!file.is_open())
    {
        cerr << "Failed to create file!" << endl;
        exit(-1);
    }
    file.close();

    for (const auto& row : query_results)
    {
        file.open(city + city_id + ".txt", ios::app);
        if (!file.is_open())
        {
            cerr << "Failed to create file!" << endl;
            exit(-1);
        }

        file << row[0] << " : " << row[1] << endl;
        file.close();

         usleep(sleep_duration_μs);
    }

    exit(EXIT_SUCCESS);
}

[[noreturn]] void childProcess(const int in_pipe)
{
    set<string> cities;
    vector<pthread_t> threads;

    do
    {
        constexpr int buffer_size = 32;
        char buf[buffer_size] = {};
        read(in_pipe, &buf, buffer_size);

        string city_id(buf);
        trim(city_id);
        if (cities.count(city_id) == 0)
        {
            cities.insert(city_id);
            pthread_t thread;
            if (pthread_create(&thread, nullptr, childThread, &city_id))
            {
                cerr << "Error creating thread." << endl;
                exit(EXIT_FAILURE);
            }
            threads.push_back(thread);
        }
    }
    while (true);
}

tuple<pid_t, int, int> createChildProcess()
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    const int in_pipe = pipefd[0];
    const int out_pipe = pipefd[1];

    const pid_t pid = fork();

    // Fork failed
    if (pid < 0) exit(EXIT_FAILURE);
    // Parent process
    if (pid > 0) return make_tuple(pid, in_pipe, out_pipe);

    // Child process
    childProcess(in_pipe);
    exit(EXIT_SUCCESS);
}

void closePipe(const tuple<pid_t, int, int>& child)
{
    close(get<IN_PIPE>(child));
    close(get<OUT_PIPE>(child));
}

void killChildrenWithPipes(const map<string, tuple<pid_t, int, int>>& children)
{
    for (const auto& child : children)
    {
        kill(get<PID>(child.second), SIGKILL);
        closePipe(child.second);
    }
}

void* parentThread(void* args)
{
    const pid_t pid = *static_cast<pid_t*>(args);

    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status))
    {
        if (WEXITSTATUS(status) == 0)
        {
            cout << endl << "Child process exited successfully." << endl;
            exit(EXIT_SUCCESS);
        }
        cout << "Child process exited with status " << WEXITSTATUS(status) << endl;
    }
    else if (WIFSIGNALED(status))
    {
        cout << "Child process was killed by signal " << WTERMSIG(status) << endl;
    }

    return nullptr;
}

int main()
{
    map<string, tuple<pid_t, int, int>> children;

    while (true)
    {
        string city;
        cout << "Enter a city name: ";
        getline(cin, city);
        trim(city);

        if (city == "exit") break;

        const auto query_results = query(
            "SELECT id, city, country, lat, lng "
            "FROM cities "
            "WHERE city='" + city + "' "
            "COLLATE NOCASE;"
        );

        if (query_results.empty())
        {
            cout << "City not found." << endl;
            continue;
        }

        string city_id;
        string country;
        if (query_results.size() > 1)
        {
        multiple_results:
            cout << "Multiple cities found." << endl;
            for (int i = 0; i < query_results.size(); i++)
            {
                cout << "[" << i + 1 << "] " << query_results[i][1] << "\t" << query_results[i][2] << "\t" <<
                    query_results[i][3] << "\t" << query_results[i][4] << endl;
            }
            cout << endl << "Please select one: ";

            string input;
            getline(cin, input);

            int selection;
            try
            {
                selection = stoi(trim_copy(input));
            }
            catch (const invalid_argument&)
            {
                cout << "Invalid input." << endl;
                goto multiple_results;
            }

            if (selection < 1 || selection > query_results.size())
            {
                cout << "Invalid selection." << endl;
                goto multiple_results;
            }

            city_id = query_results[selection - 1][0];
            country = query_results[selection - 1][2];
        }
        else
        {
            city_id = query_results[0][0];
            country = query_results[0][2];
        }

        if (children.count(country) == 0)
        {
            tuple<pid_t, int, int> child = createChildProcess();
            children.insert({country, child});
            pthread_t thread;
            if (pthread_create(&thread, nullptr, parentThread, &get<PID>(child)))
            {
                cerr << "Error creating thread." << endl;
                exit(EXIT_FAILURE);
            }
        }

        writeCityToChild(city_id, children.at(country));
    }

    killChildrenWithPipes(children);
    exit(EXIT_SUCCESS);
}
