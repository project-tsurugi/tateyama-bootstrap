#include <iostream>
#include <cstdio>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        return -1;
    }

    char* expected = argv[1];

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    int pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        if (execv(argv[2], argv + 2) < 0) {
            perror("execv");
        }
    }
    close(pipefd[1]);
    sleep(1);
    kill(pid, SIGINT);

    char buf[128];
    std::string output{};
    while (true) {
        if (int s = read(pipefd[0], buf, sizeof(buf)); s > 0) {
            output += std::string(buf, s);
        } else {
            break;
        }
    }

    auto expect = std::string(argv[1]);
    if (output.length() == expect.length()) {
        return (output == expect) ? 0 : 1;
    }
    if (output.length() > expect.length()) {
        std::cout << "length differ, received " << output.length() << " and expected " << expect.length();
        return (output.substr(0, expect.length())) == expect ? 0 : 1;
    }
    return 1;
}
