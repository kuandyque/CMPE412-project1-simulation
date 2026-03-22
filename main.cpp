#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <algorithm>

using namespace std;

// Configuration Constants
const int NUM_SERVERS = 3;
const int TOTAL_CUSTOMERS = 100;

// Structures for Probability Distribution
struct ProbRow {
    int value;
    double prob;
    double cumulative_prob;
};

// Customer Record for Statistics
struct CustomerRecord {
    int id;
    int arrival_time;
    int start_service_time;
    int service_time;
    int end_time;
    int wait_time;
};

// Server Structure
struct Server {
    int id;
    bool is_busy;
    int current_customer_id;
};

// Event Structure for FEL
// Event Types: 1 for Arrival, 2 for Departure
struct Event {
    int type;
    int time;
    int customer_id;
    int server_id; // -1 if not applicable
    
    // For sorting based on time if needed
    bool operator<(const Event& other) const {
        return time < other.time;
    }
};

// Function to generate random variable using cumulative probability table
int generate_random_value(const vector<ProbRow>& table) {
    double rand_val = (double)rand() / RAND_MAX; // 0.0 to 1.0
    for (size_t i = 0; i < table.size(); ++i) {
        if (rand_val <= table[i].cumulative_prob) {
            return table[i].value;
        }
    }
    // Fallback in case of floating point inaccuracies
    return table.back().value;
}

int main() {
    // Seed random number generator as requested
    srand(time(NULL));

    // Define Inter-Arrival Time (IAT) Table
    // Example: 1 to 5 minutes
    vector<ProbRow> iat_table = {
        {1, 0.20, 0.20},
        {2, 0.30, 0.50},
        {3, 0.25, 0.75},
        {4, 0.15, 0.90},
        {5, 0.10, 1.00}
    };

    // Define Service Time (ST) Table
    // Example: 2 to 6 minutes
    vector<ProbRow> st_table = {
        {2, 0.10, 0.10},
        {3, 0.20, 0.30},
        {4, 0.40, 0.70},
        {5, 0.20, 0.90},
        {6, 0.10, 1.00}
    };

    // Initialize Servers Activity List
    vector<Server> servers(NUM_SERVERS);
    for (int i = 0; i < NUM_SERVERS; ++i) {
        servers[i].id = i;
        servers[i].is_busy = false;
        servers[i].current_customer_id = -1;
    }

    // Initialize Future Event List (FEL)
    // We will use a vector that we can sort (as instructed format)
    vector<Event> fel;

    // Customer Records (to store stats)
    vector<CustomerRecord> customers(TOTAL_CUSTOMERS);

    // Initial setup: Generate all arrivals based on IAT
    // Note: Service Time is NOT generated here, as per CRUCIAL LOGIC RULE
    int current_arrival_time = 0;
    for (int i = 0; i < TOTAL_CUSTOMERS; ++i) {
        int iat = 0;
        if (i > 0) {
            iat = generate_random_value(iat_table);
        }
        current_arrival_time += iat;
        
        customers[i].id = i;
        customers[i].arrival_time = current_arrival_time;
        customers[i].start_service_time = -1;
        customers[i].service_time = -1;
        customers[i].end_time = -1;
        customers[i].wait_time = -1;

        // Add arrival event to FEL
        Event arr_event;
        arr_event.type = 1; // 1 = Arrival
        arr_event.time = current_arrival_time;
        arr_event.customer_id = i;
        arr_event.server_id = -1;
        fel.push_back(arr_event);
    }

    // Sort the FEL initially by time as a good practice
    sort(fel.begin(), fel.end());

    // Customer Waiting List
    queue<int> waiting_list;

    // Setup Log File
    ofstream logFile("simulation_log.txt");
    if (!logFile.is_open()) {
        cout << "Error opening log file!" << endl;
        return 1;
    }

    logFile << "Starting Simulation for " << TOTAL_CUSTOMERS << " customers." << endl;
    logFile << "--------------------------------------------------------" << endl;

    // Statistics Variables
    int clock = 0;
    int finished_customers = 0;
    int total_idle_time = 0;

    // Main Simulation Loop (Discrete Minute-by-Minute)
    while (finished_customers < TOTAL_CUSTOMERS) {
        bool tick_activity = false;

        // 3.a Check FEL for completing services (Departures) at current clock instance
        for (size_t i = 0; i < fel.size(); ++i) {
            if (fel[i].type == 2 && fel[i].time == clock) {
                int s_id = fel[i].server_id;
                int c_id = fel[i].customer_id;

                servers[s_id].is_busy = false;
                servers[s_id].current_customer_id = -1;

                customers[c_id].end_time = clock;
                finished_customers++;

                logFile << "[Time " << clock << "] Customer " << c_id 
                        << " finished service at Server " << s_id << "." << endl;
                tick_activity = true;
            }
        }

        // 3.b If there are customers waiting in waiting list, check for available servers
        while (!waiting_list.empty()) {
            // Find a free server
            int free_server_id = -1;
            for (int i = 0; i < NUM_SERVERS; ++i) {
                if (!servers[i].is_busy) {
                    free_server_id = i;
                    break;
                }
            }

            if (free_server_id != -1) {
                // We have a waiting customer AND an available server right on that clock time
                int c_id = waiting_list.front();
                waiting_list.pop();

                // Select empty server, assign it
                servers[free_server_id].is_busy = true;
                servers[free_server_id].current_customer_id = c_id;

                // CRUCIAL: Generate service time only at exact moment assigned to free server!
                int service_time = generate_random_value(st_table);
                
                customers[c_id].start_service_time = clock;
                customers[c_id].service_time = service_time;
                customers[c_id].wait_time = clock - customers[c_id].arrival_time;

                // Add Departure event to FEL
                Event dep_event;
                dep_event.type = 2; // 2 = Departure
                dep_event.time = clock + service_time;
                dep_event.customer_id = c_id;
                dep_event.server_id = free_server_id;
                fel.push_back(dep_event);
                
                // Keep FEL sorted (simple approach to maintain order)
                sort(fel.begin(), fel.end());

                logFile << "[Time " << clock << "] Customer " << c_id 
                        << " removed from waiting list and assigned to Server " 
                        << free_server_id << " (Service Time: " << service_time << ")." << endl;
                tick_activity = true;
            } else {
                // No free servers available, exit assignment loop
                break;
            }
        }

        // 3.c Check FEL for new arrivals (newcomers) exactly at current clock
        for (size_t i = 0; i < fel.size(); ++i) {
            if (fel[i].type == 1 && fel[i].time == clock) {
                int c_id = fel[i].customer_id;
                logFile << "[Time " << clock << "] Customer " << c_id << " arrived." << endl;
                tick_activity = true;

                // Check for a free server for newcomer
                int free_server_id = -1;
                for (int s = 0; s < NUM_SERVERS; ++s) {
                    if (!servers[s].is_busy) {
                        free_server_id = s;
                        break;
                    }
                }

                // If we can assign it to a server (free server exists AND no one is waiting ahead in line)
                if (free_server_id != -1 && waiting_list.empty()) {
                    servers[free_server_id].is_busy = true;
                    servers[free_server_id].current_customer_id = c_id;

                    // Generate service time
                    int service_time = generate_random_value(st_table);

                    customers[c_id].start_service_time = clock;
                    customers[c_id].service_time = service_time;
                    customers[c_id].wait_time = 0; // immediate assignment

                    // Add Departure event to FEL
                    Event dep_event;
                    dep_event.type = 2; // 2 = Departure
                    dep_event.time = clock + service_time;
                    dep_event.customer_id = c_id;
                    dep_event.server_id = free_server_id;
                    fel.push_back(dep_event);
                    
                    sort(fel.begin(), fel.end());

                    logFile << "[Time " << clock << "] Server is free! Newcomer Customer " << c_id 
                            << " assigned to Server " << free_server_id 
                            << " (Service Time: " << service_time << ")." << endl;
                } else {
                    // No empty servers OR there's a line already -> put in waiting list
                    waiting_list.push(c_id);
                    logFile << "[Time " << clock << "] Customer " 
                            << c_id << " added to waiting list." << endl;
                }
            }
        }

        // Calculate and collect statistics for this clock tick
        int idle_servers_this_tick = 0;
        for (int i = 0; i < NUM_SERVERS; ++i) {
            if (!servers[i].is_busy) {
                idle_servers_this_tick++;
            }
        }
        total_idle_time += idle_servers_this_tick;

        // Print basic trace into log file if any activity occurred
        if (tick_activity) {
            logFile << "   --> [Queue Length: " << waiting_list.size() << "] ";
            logFile << "[Server Status: ";
            for (int s = 0; s < NUM_SERVERS; ++s) {
                if (servers[s].is_busy) {
                    logFile << "S" << s << "(C" << servers[s].current_customer_id << ") ";
                } else {
                    logFile << "S" << s << "(IDLE) ";
                }
            }
            logFile << "]" << endl;
        }

        // Advance Time Mechanism
        clock++;
    }

    logFile << "--------------------------------------------------------" << endl;
    logFile << "Simulation finished at clock tick: " << (clock - 1) << endl;
    logFile.close();

    // --------------------------------------------------------
    // Calculate and Console Display KPIs
    // --------------------------------------------------------
    double total_wait_time = 0.0;
    int wait_count = 0;               
    double total_service_time = 0.0;
    double total_system_time = 0.0;   
    double total_iat = 0.0;           

    for (int i = 0; i < TOTAL_CUSTOMERS; ++i) {
        total_wait_time += customers[i].wait_time;
        if (customers[i].wait_time > 0) {
            wait_count++;
        }
        total_service_time += customers[i].service_time;
        total_system_time += (customers[i].wait_time + customers[i].service_time);

        if (i > 0) {
            total_iat += (customers[i].arrival_time - customers[i - 1].arrival_time);
        }
    }

    // Calculations based on the 7 required outputs
    double avg_wait_time = total_wait_time / TOTAL_CUSTOMERS;
    double prob_wait = (double)wait_count / TOTAL_CUSTOMERS;
    
    // Overall idle server probability = Total Idle Server Mins / (Total Servers * Total Simulated Mins)
    // Note: Simulated minutes is 'clock' because loop ticked 0 to (clock - 1)
    double prob_idle_server = (double)total_idle_time / (NUM_SERVERS * clock);
    
    double avg_service_time = total_service_time / TOTAL_CUSTOMERS;
    double avg_time_between_arrivals = total_iat / (TOTAL_CUSTOMERS - 1);
    double avg_wait_of_waiters = (wait_count > 0) ? (total_wait_time / wait_count) : 0.0;
    double avg_time_in_system = total_system_time / TOTAL_CUSTOMERS;

    // Displaying KPIs strictly to console
    cout << "======================================================\n";
    cout << "   MULTI-SERVER QUEUE SIMULATION RESULTS (CMPE 412)   \n";
    cout << "======================================================\n";
    cout << fixed << setprecision(4);
    cout << "1. Average waiting time per customer         : " << avg_wait_time << " minutes\n";
    cout << "2. Probability that a customer waits         : " << prob_wait << "\n";
    cout << "3. Probability of idle server (overall)      : " << prob_idle_server << "\n";
    cout << "4. Average service time                      : " << avg_service_time << " minutes\n";
    cout << "5. Average time between arrivals             : " << avg_time_between_arrivals << " minutes\n";
    cout << "6. Average waiting time of those who wait    : " << avg_wait_of_waiters << " minutes\n";
    cout << "7. Average time customer spends in the system: " << avg_time_in_system << " minutes\n";
    cout << "======================================================\n";
    cout << "Detailed tick-by-tick log has been saved to: simulation_log.txt\n";

    return 0;
}
