#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    //Command* current_fg_cmd = smash.fg_cmd;
    if (smash.fg_cmd) //there is a command in the fg of smash. need to add to jobs list and send SIGSTP
    {
        smash.Jobs_List->addJob(smash.fg_cmd, STOPPED, smash.fg_cmd_job_id);

        //second, send SIGSTOP
        if (kill(smash.fg_cmd->pid, SIGSTOP) == -1) 
        {
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << smash.fg_cmd->pid << " was stopped" << endl;
    }
    smash.fg_cmd = nullptr;
    return;
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    //Command* current_fg_cmd = smash.fg_cmd;
    if (smash.fg_cmd) //there is a command in the fg of smash. need to send SIGKILL
    {
        if (kill(smash.fg_cmd->pid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << smash.fg_cmd->pid << " was killed" << endl;
    }
    smash.fg_cmd = nullptr;
}

void alarmHandler(int sig_num) {
    // TODO: Add your implementation
}
