#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <string>
#include <vector>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
    char* args[COMMAND_MAX_ARGS];
    int num_of_args;
    bool is_stopped;
public:
    int pid;
    pid_t child_pid;
    const char* orig_cmd_line;
    char* cmd_line;
    bool is_in_bg;
    Command(const char* original_cmd_line, bool ignore_ampersand, int pid);
    char* getCmdLine();
    virtual ~Command();
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:

    BuiltInCommand(const char* cmd_line, int pid);
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line, int pid);
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
    std::string cmd1;
    std::string cmd2;
    int fd[2];
public:
    PipeCommand(const char* cmd_line, int pid);
    virtual ~PipeCommand() {}
    void execute() override;
    void pipe_exec(int pipe_index, bool dup);
    void pipe_ampersand_exec(int pipe_index, bool dup);
};
//
class RedirectionCommand : public Command {
    std::string fixed_cmd;
    std::string file_path;
    std::string cmd_str;
    int out_channel;
    int temp_stdout;
    bool append;
public:
    explicit RedirectionCommand(const char* cmd_line, int pid);
    virtual ~RedirectionCommand() {}
    void execute() override;
    void prepare();
    void cleanup();
    void split_cmd();
};

class ChangeDirCommand : public BuiltInCommand {
private:
    char** plastPwd;
public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd, int pid);
    virtual ~ChangeDirCommand() {}
    void execute() override;

    //void setLastDir(char** path);
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line, int pid);
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ChpromptCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ChpromptCommand(const char* cmd_line, int pid);
    virtual ~ChpromptCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line, int pid);
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobsList;


typedef enum
{
    FOREGROUND,
    BACKGROUND,
    STOPPED
} Job_State;

class JobsList {
public:
    class JobEntry {
    public:
        time_t start_time;
        int job_id;
        int process_id;
        Job_State state;
        Command* cmd;
        JobEntry(int process_id, int job_id, Command* cmd, Job_State state);
    };
public:
    int max;
    std::vector<JobsList::JobEntry>* jobs_list;
    JobsList();
    ~JobsList();
    std::vector<JobsList::JobEntry>* getJobsList();
    void updateMax();
    void addJob(Command* cmd, Job_State state, int job_id);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs(); //need to go over again
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
    JobEntry * getLastJob();
    JobEntry *getLastStoppedJob();
    // TODO: Add extra methods or modify exisitng ones as needed
};

class QuitCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs, int pid);
    virtual ~QuitCommand() {}
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList* job_list;
public:
    JobsCommand(const char* cmd_line, JobsList* job_list, int pid);
    virtual ~JobsCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* job_list;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs, int pid);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs, int pid);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
/* Optional */
// TODO: Add your data members
public:
    explicit TimeoutCommand(const char* cmd_line, int pid);
    virtual ~TimeoutCommand() {}
    void execute() override;
};

class FareCommand : public BuiltInCommand {
    /* Optional */
    // TODO: Add your data members
public:
    FareCommand(const char* cmd_line, int pid); //need to split to 3 parameters
    virtual ~FareCommand() {}
    void execute() override;
};

class SetcoreCommand : public BuiltInCommand {
    /* Optional */
    // TODO: Add your data members
public:
    SetcoreCommand(const char* cmd_line, int pid);
    virtual ~SetcoreCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    /* Bonus */
    // TODO: Add your data members
    JobsList* jobs;
public:
    KillCommand(const char* cmd_line, JobsList* jobs, int pid);
    virtual ~KillCommand() {}
    void execute() override;
};

class SmallShell {
private:
    std::string prompt;
    char* prev_path;
    SmallShell();
public:
    Command* fg_cmd;
    int fg_cmd_job_id;
    JobsList* Jobs_List;
    char** getPlastPwd();
    void setPlastPwd(char** new_plast);
    bool is_prompt_changed;
    std::string getPrompt();
    void setPrompt(std::string new_prompt);
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void executeCommand(const char* cmd_line);
    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_