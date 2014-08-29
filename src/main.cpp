#include "agent.h"

int main(int argc, char *argv[]) {
    CthunAgent::Agent agent;
    //agent.run(argv[1], argv[2]);
    agent.connect_and_run();
    return 0;
}
