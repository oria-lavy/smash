#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";
const int CMD_MAX_LEN = 80;
const string BACKGROUND_SYMBOL = "&";
const string DEFAULT_PROMPT = "smash";
const string QUOTATION = "\"";
#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define PIPE_READ 0
#define PIPE_WRITE 1

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundCommand(const char* cmd_line) {
    char c = cmd_line[string(cmd_line).find_last_not_of(WHITESPACE)];
    return c == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.7
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

void _removeGivenSign(char* cmd_line, char sign, bool from_start = false) {
    const string str(cmd_line);
    unsigned int idx;
    if(!from_start) {
        // find last character other than spaces
        idx = str.find_last_not_of(WHITESPACE);
        // if all characters are spaces then return
        if (idx == string::npos) {
            return;
        }
        // if the command line does not end with & then return
        if (cmd_line[idx] != sign) {
            return;
        }
    }
    if(from_start){
        idx = str.find_first_not_of(WHITESPACE);
        // if all characters are spaces then return
        if (idx == string::npos) {
            return;
        }
        // if the command line does not end with & then return
        if (cmd_line[idx] != sign) {
            return;
        }
    }
    // replace the & (background sign) with space and then remove all tailing spaces.7
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    if(!from_start) {
        cmd_line[str.find_last_not_of(WHITESPACE, idx)] = 0;
    }
    if(from_start){
        cmd_line[str.find_first_not_of(WHITESPACE, idx)] = 0;
    }
}

bool is_digits(const std::string &str){
    return str.find_first_not_of("-0123456789") == std::string::npos;
}


bool is_complex_command(char* cmd_line){
    for(int i=0; i<strlen(cmd_line); i++){
        if(cmd_line[i] == '?' || cmd_line[i] == '*'){
            return true;
        }
    }
    return false;
}

/***************************************************************************
****************************************************************************
*******************************COMMAND**************************************
****************************************************************************
***************************************************************************/

Command::Command(const char* original_cmd_line, bool ignore_ampersand, int pid) : orig_cmd_line(original_cmd_line), is_in_bg(false), child_pid(-1) {
    SmallShell& smash = SmallShell::getInstance();
    cmd_line = new char[strlen(original_cmd_line)+1];
    strcpy(cmd_line, original_cmd_line);
    num_of_args = _parseCommandLine(cmd_line, args);
    if(ignore_ampersand) {
        _removeBackgroundSign(args[0]);
        if (num_of_args > 1) {
            _removeBackgroundSign(args[1]);
            if (*args[1] == ' ') {
                for (int i = 1; i < num_of_args - 1; i++) {
                    args[i] = args[i + 1];
                }
                num_of_args--;
            }
        }
    }
}

BuiltInCommand::BuiltInCommand(const char* cmd_line, int pid) :
        Command(cmd_line, true, pid) {}

Command::~Command(){
    delete[] cmd_line;
    for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
        if (args[i] != nullptr) {
            free(args[i]);
        }
    }
}

char* Command::getCmdLine() {
    return cmd_line;
}

/***************************************************************************
****************************************************************************
***************************CHPROMPT_COMMAND*********************************
****************************************************************************
***************************************************************************/

ChpromptCommand::ChpromptCommand(const char* cmd_line, int pid) : BuiltInCommand(cmd_line, pid) {}

void ChpromptCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    if(num_of_args > 1){
        smash.is_prompt_changed = true;
        if(*args[1] == '&'){
            if(num_of_args > 2) {
                smash.setPrompt(args[2]);
            }
            else{
                smash.is_prompt_changed = false;
                smash.setPrompt(DEFAULT_PROMPT);
            }
        }
        else {
            smash.setPrompt(args[1]);
        }
    }
    else {
        smash.is_prompt_changed = false;
        smash.setPrompt(DEFAULT_PROMPT);
    }
}

/***************************************************************************
****************************************************************************
*****************************SMALL_SHELL************************************
****************************************************************************
***************************************************************************/

SmallShell::SmallShell() : prompt(DEFAULT_PROMPT), is_prompt_changed(false), prev_path(nullptr), fg_cmd(nullptr), fg_cmd_job_id(-1) {
    Jobs_List = new JobsList();
}


void SmallShell::setPrompt(std::string new_prompt) {
    SmallShell& smash = SmallShell::getInstance();
    smash.prompt = new_prompt;
}

string SmallShell::getPrompt() {
    return prompt;
}



SmallShell::~SmallShell() {

}



/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    SmallShell& smash = SmallShell::getInstance();
    JobsList* jobs = smash.Jobs_List;
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if(cmd_s.find(">") != std::string::npos) {
        return new RedirectionCommand(cmd_line, -1);
    }
    else if(cmd_s.find("|") != std::string::npos) {
        return new PipeCommand(cmd_line, -1);
    }
    if (firstWord.compare("pwd") == 0 || firstWord.compare("pwd&") == 0) {
        return new GetCurrDirCommand(cmd_line, -1);
    }
    else if (firstWord.compare("chprompt") == 0 || firstWord.compare("chprompt&") == 0) {
        return new ChpromptCommand(cmd_line, -1);
    }
    else if (firstWord.compare("showpid") == 0 || firstWord.compare("showpid&") == 0) {
        return new ShowPidCommand(cmd_line, -1);
    }
    else if(firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0) {
        return new QuitCommand(cmd_line, jobs, -1);
    }
    else if (firstWord.compare("cd") == 0 || firstWord.compare("cd&") == 0) {
        return new ChangeDirCommand(cmd_line, &prev_path, -1);
    }
    else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0) {
        return new JobsCommand(cmd_line, jobs, -1);
    }
    else if(firstWord.compare("kill") == 0 || firstWord.compare("&kill") == 0) {
        return new KillCommand(cmd_line, jobs, -1);
    }
    else if(firstWord.compare("fg") == 0 || firstWord.compare("fg&") == 0) {
        return new ForegroundCommand(cmd_line, jobs, -1);
    }
    else if(firstWord.compare("bg") == 0 || firstWord.compare("bg&") == 0) {
        return new BackgroundCommand(cmd_line, jobs, -1);
    }
    else if(firstWord.compare("setcore") == 0 || firstWord.compare("setcore&") == 0) {
        return new SetcoreCommand(cmd_line, -1);
    }
//    else if(firstWord.compare("tail") == 0) {
//        return new TailCommand(cmd_line);
//    }
//    else if(firstWord.compare("touch") == 0) {
//        return new TouchCommand(cmd_line);
//    }
//    else if(firstWord.compare("timeout") == 0) {
//        return new TimeoutCommand(cmd_line);
//    }
//
    else {
        return new ExternalCommand(cmd_line, -1);
    }
    return nullptr;
}


void SmallShell::executeCommand(const char *cmd_line) {
	this->Jobs_List->removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

/***************************************************************************
****************************************************************************
*******************************JOBS_LIST************************************
****************************************************************************
***************************************************************************/

JobsList::JobEntry::JobEntry(int process_id, int job_id, Command* cmd, Job_State state) : process_id(process_id),
                                                                                          job_id(job_id), cmd(cmd), state(state), start_time(time(NULL)){
//    time_t p_time;
//    if(time(&p_time) < 0 ) {
//        perror("smash error: time failed");
//        return;
//    }
//    start_time = p_time;
}

void JobsList::printJobsList(){
    SmallShell& smash = SmallShell::getInstance();
    vector<JobsList::JobEntry>* job_list = smash.Jobs_List->getJobsList();
    vector<JobsList::JobEntry>::iterator iter;
    smash.Jobs_List->removeFinishedJobs();

    for(iter = job_list->begin();iter < job_list->end() ;iter++){
        cout << "[" << iter->job_id << "] " << iter->cmd->cmd_line << " : " << iter->process_id << " "
             << difftime(time(NULL), iter->start_time) << " secs";
        if(iter->state == STOPPED){
            cout << " (stopped)";
        }
        cout << endl;
    }
}

void JobsList::addJob(Command* cmd, Job_State state, int job_id){
    SmallShell& smash = SmallShell::getInstance();
    int new_job_id;
    //cout << "job_id is: " << job_id << endl;
    //cout << "max is: " << max << endl;
    removeFinishedJobs();
    updateMax();
    //cout << "after update max is: " << max << endl;
    if(job_id == -1){
        
        if(jobs_list->size() == 0){
            new_job_id = 1;
        }
        else {
            new_job_id = max + 1;
        }   
        max = new_job_id;
        JobEntry new_job = JobEntry(cmd->pid, new_job_id, cmd, state);
        jobs_list->push_back(new_job);
    }
    else{
        new_job_id = job_id;
        vector<JobsList::JobEntry>::iterator iter;
        iter = jobs_list->begin();
        while(iter->job_id < new_job_id && iter != jobs_list->end()){
            iter++;
        }
        JobEntry new_job = JobEntry(cmd->pid, new_job_id, cmd, state);
        jobs_list->insert(iter, new_job);
    }
    updateMax();
}

void JobsList::updateMax() {
    vector<JobsList::JobEntry>::iterator iter;
    iter = jobs_list->begin();
    while(iter != jobs_list->end()){
        max = iter->job_id;
        iter++;
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
    vector<JobsList::JobEntry>::iterator iter;
    for(iter = jobs_list->begin();iter != jobs_list->end() ;iter++){
        if (iter->job_id == jobId)
        {
            return &*iter;
        }
    }
    return nullptr; //didn't find this Job Id
}

void JobsList::removeJobById(int jobId) {
    vector<JobsList::JobEntry>::iterator iter;
    for(iter = jobs_list->begin();iter != jobs_list->end() ;iter++){
        if (iter->job_id == jobId)
        {
            jobs_list->erase(iter);
            return;
        }
    }
}

JobsList::JobEntry* JobsList::getLastJob() {
    SmallShell& smash = SmallShell::getInstance();
    removeFinishedJobs();
    JobEntry* last_job = smash.Jobs_List->getJobById(max);
    return last_job;
}

JobsList::JobEntry* JobsList::getLastStoppedJob() {
    SmallShell& smash = SmallShell::getInstance();
    vector<JobsList::JobEntry>::iterator iter;
    if(smash.Jobs_List->getJobsList()->size() == 0){
        return nullptr;
    }
    for(iter = jobs_list->end() - 1;iter != jobs_list->begin() ;iter--){ //going from the end of the vector
        if (iter->state == STOPPED)
        {
            return &*iter;
        }
    }
    if(jobs_list->begin()->state == STOPPED){
        return &*(jobs_list->begin());
    }
    return nullptr;
}

void JobsList::removeFinishedJobs() {

    if (jobs_list->size() == 0) {
        return;
    }
    vector<JobsList::JobEntry>::iterator iter = jobs_list->begin();
    for (; iter <= jobs_list->end();) {
        //we will check if wait gives us jobs that are done and not stopped
        if(iter==jobs_list->end()){
            updateMax();
            return;
        }
        int status;
        pid_t return_pid = waitpid(iter->process_id, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (return_pid > 0 || return_pid == -1) {
            if(WIFSIGNALED(status) || WIFEXITED(status))
            {
                jobs_list->erase(iter);
                continue; //erase increases the iterator
            }
            else if(WIFSTOPPED(status))
            {
                iter->state = STOPPED;
            }
            else if(WIFCONTINUED(status))
            {
                iter->state = BACKGROUND;
            }
        }
        iter++;
    }
    return;
}

JobsList::JobsList(){
    jobs_list = new vector<JobsList::JobEntry>;
}

JobsList::~JobsList(){
    delete(jobs_list);
}

void JobsList::killAllJobs(){
    SmallShell& smash = SmallShell::getInstance();
    vector<JobsList::JobEntry>* job_list = smash.Jobs_List->getJobsList();
    vector<JobsList::JobEntry>::iterator iter;
    removeFinishedJobs();
    cout << "smash: sending SIGKILL signal to " << smash.Jobs_List->getJobsList()->size() << " jobs:" << endl;
    for(iter = job_list->begin();iter != job_list->end() ;iter++){
        cout << iter->process_id << ": " << iter->cmd->cmd_line << endl;
        int killed = kill(iter->process_id, SIGKILL);
        if(killed == -1){
            perror("smash error: kill failed");
        }
    }
}

vector<JobsList::JobEntry>* JobsList::getJobsList(){
    return jobs_list;
}

/***************************************************************************
****************************************************************************
*******************************SHOW_PID*************************************
****************************************************************************
***************************************************************************/

ShowPidCommand::ShowPidCommand(const char* cmd_line, int pid) : BuiltInCommand(cmd_line, pid) {}

void ShowPidCommand::execute(){
    std::cout << "smash pid is ";
    std::cout << getpid() << std::endl;
}

/***************************************************************************
****************************************************************************
*****************************JOBS_COMMAND***********************************
****************************************************************************
***************************************************************************/

JobsCommand::JobsCommand(const char* cmd_line, JobsList* job_list, int pid): BuiltInCommand(cmd_line, pid), job_list(job_list) {}

void JobsCommand::execute(){
    if (job_list->jobs_list->size() == 0) //if the list is empty we should print nothing. according to piazza
    {
        return;
    }
    job_list->printJobsList();
}

/***************************************************************************
****************************************************************************
**********************************FG****************************************
****************************************************************************
***************************************************************************/

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs, int pid): BuiltInCommand(cmd_line, pid), job_list(job_list) {}

void ForegroundCommand::execute(){
    if(num_of_args > 2 || (num_of_args > 1 && !(is_digits(string(args[1]))))){
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    smash.Jobs_List->removeFinishedJobs();
    vector<JobsList::JobEntry>* job_list = smash.Jobs_List->getJobsList();
    vector<JobsList::JobEntry>::iterator iter;
    int job_id;
    if(num_of_args == 2){
        job_id = atoi(args[1]);
        JobsList::JobEntry* job1 = smash.Jobs_List->getJobById(job_id);
        if(job1 == nullptr){
            cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
            return;
        }
    }
    if(num_of_args == 1){
        if(job_list->size() == 0){
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        job_id = smash.Jobs_List->max;
    }
    JobsList::JobEntry* job = smash.Jobs_List->getJobById(job_id);
    if(job->state == BACKGROUND) {
        if (kill(job->process_id, SIGSTOP) == -1) {
            perror("smash error: kill failed");
            return;
        }
    }
    if(kill(job->process_id, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
    }
    job->state = FOREGROUND; ///if didnt return than it worked
    cout << job->cmd->cmd_line << " : " << job->process_id << endl;
    smash.fg_cmd = job->cmd;
    smash.fg_cmd_job_id = job->job_id;
    int pid = job->process_id;
    smash.Jobs_List->removeJobById(job_id);
    waitpid(pid, NULL, WUNTRACED);
    delete smash.fg_cmd;
    smash.fg_cmd = nullptr;
    smash.fg_cmd_job_id = -1;
}

/***************************************************************************
****************************************************************************
**********************************BG****************************************
****************************************************************************
***************************************************************************/

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs, int pid): BuiltInCommand(cmd_line, pid), jobs(jobs) {}

void BackgroundCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    vector<JobsList::JobEntry>* job_list = smash.Jobs_List->getJobsList();
    vector<JobsList::JobEntry>::iterator iter;
    JobsList::JobEntry* job;
    ///IF WE HAVE JOB ID
    if(num_of_args > 1){
        if(num_of_args > 2 || !(is_digits(string(args[1])))){
            cerr << "smash error: bg: invalid arguments" << endl;
            return;
        }
        else{
            int wanted_job_id = atoi(args[1]);
            job = smash.Jobs_List->getJobById(wanted_job_id);
            if(job == nullptr){
                cerr << "smash error: bg: job-id " << wanted_job_id <<  " does not exist" << endl;
                return;
            }
            if(job->state != STOPPED){
                cerr << "smash error: bg: job-id " << job->job_id << " is already running in the background" << endl;
                return;
            }
        }
    }
    else{
        job = smash.Jobs_List->getLastStoppedJob();
        if(job == nullptr){
            cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
            return;
        }
    }
    cout << job->cmd->cmd_line << " : " << job->process_id << endl;
    if(kill(job->process_id, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
    }
    job->state = BACKGROUND;
}

/***************************************************************************
****************************************************************************
**********************************CD****************************************
****************************************************************************
***************************************************************************/

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd, int pid) : BuiltInCommand(cmd_line, pid), plastPwd(plastPwd) {}

void ChangeDirCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    if (num_of_args == 1) //no parameters were given so according to pg 2 in pdf
    {
        cerr << "smash error:> " + QUOTATION + (string)cmd_line + QUOTATION  << endl;
        return;
    }
    else if(num_of_args > 2){
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }
    char* current_path = getcwd(nullptr, 0);
    // On success, these functions return a pointer to a string containing the pathname of the current working directory.
    //On failure, these functions return NULL
    if (current_path == nullptr) //if the system call fails then according to error handling. check if need to write NULL instead of nullptr
    {
        perror("smash error: getcwd failed");
        return;
    }

    else if(num_of_args > 1){ //num of args is 2 meaning the cmd line and the argument
        if(*args[1] == '&'){ //we should ignore background
            if(num_of_args > 2) {
                args[1] = args[2];
            }
            else{ //not valid parameters given- no arguments (only cd &)
                cout << "smash error:>" + QUOTATION + (string)cmd_line + QUOTATION  << endl; //check if to cerr
                return;
            }
        }

        if((strcmp(args[1], "-")) == 0) //change here to the last working directory
        {
            if (*plastPwd == nullptr) //there wasn't a prev working directory
            {
                cerr << "smash error: cd: OLDPWD not set" << endl;
                return;
            }
            else //there is a prev directory to change to
            {
                int changed = chdir(*plastPwd); //return 0 if success, -1 if error
                if (changed != 0) //chdir failed (this is a system call so perror)
                {
                    perror("smash error: chdir failed");
                    return;
                }
                //if it is 0 then it was success and changed the directory.
            }
        }
        else //no "-" meaning we got a path
        {
            int changed = chdir(args[1]);
            if (changed != 0) //syscall failed
            {
                perror("smash error: chdir failed");
                return;
            }
        }
        free(*plastPwd); //this is the prev prev working directory. maybe not to delete because a couple of - in a row?
        *plastPwd = current_path; //path before this change so now its the prev
        return;
    }
}

/***************************************************************************
****************************************************************************
**********************************PWD***************************************
****************************************************************************
***************************************************************************/

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line, int pid) : BuiltInCommand(cmd_line, pid) {}

void GetCurrDirCommand::execute(){
    string pwd = getcwd(NULL, 0);
    std::cout << pwd << std::endl;
}

/***************************************************************************
****************************************************************************
**********************************QUIT**************************************
****************************************************************************
***************************************************************************/

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs, int pid) : BuiltInCommand(cmd_line, pid), jobs(jobs) {}

void QuitCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    if (num_of_args > 1) //then kill argument was maybe specified
    {
        string cmd_arg = _trim(string(args[1]));
        if(cmd_arg.compare("kill") == 0) {
            jobs->killAllJobs();
        } //if there are arguments that are not kill, we ignore them
    }
    //delete this;
    exit(0);
}

/***************************************************************************
****************************************************************************
**********************************KILL**************************************
****************************************************************************
***************************************************************************/
///bonus.

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs, int pid): BuiltInCommand(cmd_line, pid), jobs(jobs) {}

void KillCommand::execute(){
    if(num_of_args != 3 || !(is_digits((string)(args[1]))) || !(is_digits((string)(args[2])))){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    int sig = atoi(args[1]);
    if(sig >= 0){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    vector<JobsList::JobEntry>* job_list = smash.Jobs_List->getJobsList();
    vector<JobsList::JobEntry>::iterator iter;
    JobsList::JobEntry* job;
    int wanted_job_id = atoi(args[2]);
    int signal = std::abs(atoi(args[1]));
    job = smash.Jobs_List->getJobById(wanted_job_id);
    if(job == nullptr){
        cerr << "smash error: kill: job-id " << wanted_job_id << " does not exist" << endl;
        return;
    }
    if(kill(job->process_id, signal) == -1){
        perror("smash error: kill failed");
        return;
    }
    cout << "signal number " << signal << " was sent to pid " << job->process_id << endl;
}

/***************************************************************************
****************************************************************************
*********************************PIPE***************************************
****************************************************************************
***************************************************************************/

PipeCommand::PipeCommand(const char* cmd_line, int pid): Command(cmd_line, true, pid) {}

void PipeCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    Command* first_cmd, *second_cmd;
    int finished_1, finished_2;
    int pipe_index = string(cmd_line).find_first_of("|");
    if(pipe(fd) == -1){
        perror("smash error: pipe failed");
        return;
    }
    pid_t pid1 = fork();
    if (pid1 == -1)
    {
        perror("smash error: pipe failed");
        return;
    }
    if(pid1 == 0){
        if(setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            return;
        }
        if(string(cmd_line).find("|&") != std::string::npos){
            pipe_ampersand_exec(pipe_index, true);
        }
        else{
            pipe_exec(pipe_index, true);
        }
        if(close(fd[PIPE_WRITE]) == -1){ ///closing to stay with one pointer
            perror("smash error: close failed");
            return;
        }
        if(close(fd[PIPE_READ]) == -1){ ///closing because one way pipe
            perror("smash error: close failed");
            return;
        }
        /// need to check if null???
        smash.executeCommand(cmd1.c_str());
        exit(0);
    }
    pid_t pid2 = fork();
    if (pid2 == -1)
    {
        perror("smash error: pipe failed");
        return;
    }
    if(pid2 == 0){ //child
        if(setpgrp()==-1)
        {
            perror("smash error: setpgrp failed");
            return;
        }
        if(string(cmd_line).find("|&") != std::string::npos){
            pipe_ampersand_exec(pipe_index, false);
        }
        else{
            pipe_exec(pipe_index, false);
        }
        if(dup2(fd[PIPE_READ], STDIN) == -1){
            perror("smash error: dup2 failed");
            return;
        }
        if(close(fd[PIPE_WRITE]) == -1){ ///closing because one way pipe
            perror("smash error: close failed");
            return;
        }
        if(close(fd[PIPE_READ]) == -1) { ///closing to stay with one pointer
            perror("smash error: close failed");
            return;
        }
        smash.executeCommand(cmd2.c_str()); /// need to check if null???
        exit(0);
    }
    if(close(fd[PIPE_WRITE]) == -1){ ///closing because one way pipe
        perror("smash error: close failed");
        return;
    }
    if(close(fd[PIPE_READ]) == -1) { ///closing to stay with one pointer
        perror("smash error: close failed");
        return;
    }
    do{
        finished_1 = waitpid(pid1, NULL, WNOHANG | WUNTRACED | WCONTINUED);
        finished_2 = waitpid(pid2, NULL, WNOHANG | WUNTRACED | WCONTINUED);
    } while(finished_1 == 0 || finished_2 == 0);
    return;
}

void PipeCommand::pipe_exec(int pipe_index, bool dup){
    cmd1 = string(cmd_line).substr(0,pipe_index);
    cmd2 = string(cmd_line).substr(pipe_index+1);
    if(dup){
        if(dup2(fd[PIPE_WRITE], STDOUT) == -1){
            perror("smash error: dup2 failed");
            return;
        }
    }
}

void PipeCommand::pipe_ampersand_exec(int pipe_index, bool dup){
    cmd1 = string(cmd_line).substr(0,pipe_index);
    cmd2 = string(cmd_line).substr(pipe_index+2);
    if(dup){
        if(dup2(fd[PIPE_WRITE], STDERR) == -1){
            perror("smash error: dup2 failed");
            return;
        }
    }
}

/***************************************************************************
****************************************************************************
******************************REDIRECTION***********************************
****************************************************************************
***************************************************************************/

RedirectionCommand::RedirectionCommand(const char* cmd_line, int pid): Command(cmd_line, true, pid), cmd_str(string(cmd_line)), out_channel(-1) {}

void RedirectionCommand::execute(){ ///needs to use prepare and cleanup
    SmallShell& smash = SmallShell::getInstance();
    if(cmd_str.find(">>") != std::string::npos){
        append = true;
    }
    else{ //only <
        append = false;
    }
    split_cmd();
    prepare();
    if(fixed_cmd != ""){
        smash.executeCommand(fixed_cmd.c_str());
    }
    cleanup();
}

void RedirectionCommand::split_cmd(){
    int arrow_index = cmd_str.find_first_of(">");
    int file_path_start = arrow_index + (append ? 2 : 1); //skip >> instead of >
    //fixed_cmd = string(cmd_str.begin(), cmd_str.begin()+arrow_index);
    fixed_cmd = cmd_str.substr(0, arrow_index);
    //file_path = _trim(string(cmd_str.begin()+file_path_start, cmd_str.end()));
    file_path = _trim(cmd_str.substr(file_path_start));
    _removeGivenSign(const_cast<char*>(file_path.c_str()), '&');
}

void RedirectionCommand::prepare() {
    temp_stdout = dup(STDOUT);
    if(temp_stdout == -1) {
        perror("smash error: dup failed");
        fixed_cmd = "";
        return;
    }
    if(append) {
        out_channel = open(file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    else {
        out_channel = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    if(out_channel == -1) {
        perror("smash error: open failed");
        fixed_cmd = "";// reset command so nothing will happen in execute 
        return;
        //exit(0);
    }
    if (dup2(out_channel, STDOUT) == -1)
    {
        perror("smash error: dup2 failed");
        exit(0);
    }
}

void RedirectionCommand::cleanup() {
    if(dup2(temp_stdout, STDOUT) == -1) {  // returns stdout to channel 1
        perror("smash error: dup2 failed");
        exit(0);
    }
    if(out_channel != -1 && close(out_channel) == -1) {
        perror("smash error: close failed");
        exit(0);
    }
    if(close(temp_stdout) == -1) {
        perror("smash error: close failed");
        exit(0);
    }
}

/***************************************************************************
****************************************************************************
*******************************SET_CORE***********************************
****************************************************************************
***************************************************************************/
///optional.

SetcoreCommand::SetcoreCommand(const char* cmd_line, int pid): BuiltInCommand(cmd_line, pid){}

void SetcoreCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    int core_num, job_id;
    if(num_of_args != 3){
        cerr << "smash error: setcore: invalid arguments" << endl;
        return;
    }
    if(!is_digits(args[1])){ //should be job id
        cerr << "smash error: setcore: job-id " << *args[1] <<  " does not exist" << endl;
        return;
    }
    job_id = atoi(args[1]);
    if(smash.Jobs_List->getJobById(job_id) == nullptr){
        cerr << "smash error: setcore: job-id " << job_id << " does not exist" << endl;
        return;
    }
    if(!is_digits(args[2])){ //should be core num
        cerr << "smash error: setcore: invalid core number" << endl;
        return;
    }
    core_num = atoi(args[2]);
    //need to check if core num is valid- if it is in the range [0, n-1] when there are n cores
    ulong num_of_cores = sysconf(_SC_NPROCESSORS_CONF);
    if (core_num > num_of_cores - 1 || core_num < 0) {
        cerr << "smash error: setcore: invalid core number" << endl;
        return;
    }
    JobsList::JobEntry* job = smash.Jobs_List->getJobById(job_id);
    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    CPU_SET(core_num, &my_set);
    if (sched_setaffinity(job->process_id, sizeof(cpu_set_t), &my_set) == -1)
    {
        perror("smash error: sched_setaffinity failed");
        return;
    }
    ///I think this should end it...
}

/***************************************************************************
****************************************************************************
*************************EXTERNAL_COMMANDS**********************************
****************************************************************************
***************************************************************************/

ExternalCommand::ExternalCommand(const char* cmd_line, int pid) : Command(cmd_line, false, pid) {
    is_in_bg = _isBackgroundCommand(cmd_line); //this field is from the general Command
}

void ExternalCommand::execute() { //need to check if simple or complex...
    SmallShell& smash = SmallShell::getInstance();
    bool is_spaced = false;
    if(*args[num_of_args-1] == '&'){
        is_spaced = true;
    }
    _removeGivenSign(cmd_line, '&');
    if(is_spaced){
        num_of_args--;
        args[num_of_args] = nullptr;
    }
    bool bg = is_in_bg;
    Command* cmd = smash.CreateCommand(orig_cmd_line);
    cmd->is_in_bg = bg;
    if (!is_in_bg)
    {
        smash.fg_cmd = cmd;
    }
    pid_t pid = fork();
    if(pid == -1) {
        perror("smash error: fork failed");
        return;
    }
    if(pid == 0) {  /// child
        setpgrp();
        int child_pid = getpid();
        if(is_complex_command(cmd_line)) { //complex command
            if(execl("/bin/bash", "/bin/bash", "-c",(string(cmd_line)).c_str() , nullptr) == -1) {
                perror("smash error: execl failed");
                return;
            }
        }
        else { //simple command
            _removeGivenSign(args[num_of_args - 1], '&');
            if (execvp(args[0], &args[0]) == -1) {
                perror("smash error: execvp failed");
                return;
            }
        }
    }
    else ///parent
    {
        if (!is_in_bg) {
            cmd->pid = pid;
            smash.fg_cmd = cmd;
            int waited = waitpid(pid, NULL, WUNTRACED);
            if (waited == -1) {
                perror("smash error: waitpid failed");
                return;
            }
        } else {
            cmd->pid = pid;
            smash.Jobs_List->addJob(cmd, BACKGROUND, smash.fg_cmd_job_id);
        }
    }
    delete smash.fg_cmd;
    smash.fg_cmd = nullptr;
    return;
}
