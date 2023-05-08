#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include <iostream>
using namespace std;

int main() {
    int server_pid = fork();
    if (server_pid == -1) {
        return 1;
    } else if (server_pid == 0) {
        // child
        execl("./ServerTest", "./ServerTest", 0);
    }

    cout << "Started ./ServerTest, waiting 2 seconds before starting ./ClientTest" << endl;

    sleep(2);

    int client_pid = fork();
    if (client_pid == -1) return 1;
    else if (client_pid == 0) {
        execl("./ClientTest", "./ClientTest", 0);
    }

    int status = 0;
    waitpid(client_pid, &status, 0);
    if (status) return status;

    waitpid(server_pid, &status, 0);
    return status;
}
