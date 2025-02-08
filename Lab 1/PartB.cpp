#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
using namespace std;
/*
• Prompt the user to enter a name of a City
• Check if the City entered by the user exists in the dataset (daily_temperature_1000_cities_1980_2020_transformed.csv).
    If it does not exist, prompt the user to enter a Country. If the Country exist, then randomly select a city from the
    dataset corresponding to the Country entered. It is up to you to decide how you will handle the case if the Country
    does not exist – handle it elegantly.
• Once you have the city name, use fork to create a new process. Make sure the child is aware of the city name that has
    been entered.
• At this point the main routine goes back to waiting for new user city input. User input of “done” should terminate the
    main process and all its children.
• Each child process should open a separate text file on the hard disk. The name of the file, I leave to your
    discretion, so long as it allows you to distinguish one city from another (tip: think how you would handle the case
    when user inputs the same city twice). Once per second, the child should write the new string to the file on a new
    line; this string should contain the date and temperature of the city extracted from the file. It is up to you to
    decide how you will select the dates or which dates you will select, but the goal is to get meaningful information
    about the change of temperature over time.
    o Example: If I type in 2 different cities, I should have 2 children create two different files. Each of these children
    has their own process writing in their own separate files.
• Children never terminate unless externally killed.
*/
#define ID 0
#define CITY 1
#define CITY_ASCII 2
#define LATITUDE 3
#define LOGITUDE 4
#define COUNTRY 5
#define ISO2 6
#define ISO3 7
#define ADMIN_NAME 8
#define CAPITAL 9
#define POPULATION 10
#define ID2 11
#define DATETIME 12
#define NUM_MACROS 13

size_t findNthComma(const std::string& str, const int n)
{
    if (n <= 0) return 0;

    size_t pos = 0;
    int count = 0;

    while (count < n)
    {
        pos = str.find(',', pos);
        if (pos == std::string::npos)
        {
            return std::string::npos;
        }
        ++pos;
        ++count;
    }

    return pos - 1;
}

string getNthColumn(const string& str, const int n)
{
    const size_t pos = findNthComma(str, n);
    if (pos == string::npos)
    {
        throw runtime_error("Invalid dataset format!");
    }
    if (n <= 0) return str.substr(0, str.find(','));
    return str.substr(pos + 1, str.find(',', pos + 1) - (pos + 1));
}

int countCommas(const string& str)
{
    int count = 0;
    for (const char i : str)
    {
        if (i == ',') count++;
    }
    return count;
}

void readCSVLine(string& line, vector<string>& splitLine)
{
    size_t pos;
    while ((pos = line.find(',')) != string::npos)
    {
        splitLine.push_back(line.substr(0, pos));
        line.erase(0, pos + 1);
    }
}

void readCSVHeader(vector<string>& csvHeader)
{
    ifstream file("daily_temperature_1000_cities_1980_2020_transformed.csv");
    if (!file.is_open())
    {
        throw runtime_error("Dataset file not found!");
    }

    string line;
    getline(file, line);
    readCSVLine(line, csvHeader);
}

string loadCity(const string& cityName)
{
    ifstream file("daily_temperature_1000_cities_1980_2020_transformed.csv");
    if (!file.is_open())
    {
        throw runtime_error("Dataset file not found!");
    }

    string line;
    while (getline(file, line))
    {
        if (getNthColumn(line, CITY) == cityName || getNthColumn(line, CITY_ASCII) == cityName) return line;
    }

    return "";
}

string loadCityByCountry(const string& countryName)
{
    ifstream file("daily_temperature_1000_cities_1980_2020_transformed.csv");
    if (!file.is_open())
    {
        throw runtime_error("Dataset file not found!");
    }

    vector<string> cities;
    string line;
    while (getline(file, line))
    {
        if (getNthColumn(line, COUNTRY) == countryName) cities.push_back(line);
    }

    if (!cities.empty())
    {
        srand(time(nullptr));
        return cities[rand() % cities.size()];
    }
    return "";
}

pid_t createChildProcess(string& data, const vector<string>& csvHeader)
{
    const pid_t pid = fork();
    if (pid < 0)
    {
        // fork failed
        cerr << "Fork failed!" << endl;
        exit(-1);
    }

    if (pid == 0)
    {
        // child process
        const string cityName = getNthColumn(data, 1);
        ofstream file(cityName + to_string(getpid()) + ".txt");
        if (!file.is_open())
        {
            cerr << "Failed to create file!" << endl;
            exit(-1);
        }

        vector<string> values;
        readCSVLine(data, values);

        for (int i = NUM_MACROS; i < csvHeader.size(); i++)
        {
            file << csvHeader[i] << "," << values[i] << endl;
            sleep(1);
        }
    }

    // parent process
    return pid;
}

void killChildren(vector<pid_t>& children)
{
    for (pid_t pid : children)
    {
        kill(pid, SIGKILL);
    }
}

int main()
{
    vector<string> csvHeader;
    readCSVHeader(csvHeader);

    vector<pid_t> children;
    while (true)
    {
        string input;
        cout << "Enter a city name: ";
        cin >> input;
        if (input == "done")
        {
            killChildren(children);
            return 0;
        }
        string data;
        try
        {
            data = loadCity(input);
        }
        catch (runtime_error& e)
        {
            cerr << e.what() << endl;
            return -1;
        }

        if (data.empty())
        {
            cout << "City not found. Enter a country: ";
            cin >> input;
            if (input == "done") return 0;
            try
            {
                data = loadCityByCountry(input);
            }
            catch (runtime_error& e)
            {
                cerr << e.what() << endl;
                return -1;
            }
        }

        cout << "Found city: " << getNthColumn(data, CITY) << endl;
        /*cout << getNthColumn(data, ID) << endl;
	    cout << getNthColumn(data, CITY) << endl;
	    cout << getNthColumn(data, CITY_ASCII) << endl;
	    cout << getNthColumn(data, LATITUDE) << endl;
	    cout << getNthColumn(data, LOGITUDE) << endl;
	    cout << getNthColumn(data, COUNTRY) << endl;
	    cout << getNthColumn(data, ISO2) << endl;
	    cout << getNthColumn(data, ISO3) << endl;
	    cout << getNthColumn(data, ADMIN_NAME) << endl;
	    cout << getNthColumn(data, CAPITAL) << endl;
	    cout << getNthColumn(data, POPULATION) << endl;
	    cout << getNthColumn(data, ID2) << endl;
	    cout << getNthColumn(data, DATETIME) << endl;*/
        children.push_back(createChildProcess(data, csvHeader));

        if (input == "done")
        {
            killChildren(children);
            return 0;
        }
    }
}
