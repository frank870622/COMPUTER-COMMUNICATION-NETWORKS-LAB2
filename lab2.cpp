#include <iostream>
#include <algorithm>
#include <memory.h>
#include <vector>
#include <string>
using namespace std;

class Processor //class of each processor
{
  public:
    int id;
    int ability;
};

class ProcessTime //class of each processor's process time
{
  public:
    int ID;
    int processtime;
};

class Task //class of each task
{
  public:
    int ID;
    int release_time; //arrival time
    int execution_time;
    int deadline;
    int period;
    int preemption;
    int task;
};

bool sort_execution_time(Task, Task);             //sort taskvector's execute time from small to big
bool sort_realease_time(Task, Task);              //sort taskvector's release time(arrival time) from small to big
bool sort_process_time(ProcessTime, ProcessTime); //sort provector's process time from small to big

int main()
{
    vector<Task> taskvector;         //vector to store ready task
    vector<Processor> provector;     //vector to store processor
    vector<ProcessTime> processtime; //vector to store processor's process time

    freopen("input.txt", "r", stdin); //set input.txt as the stdin
    int processor_num, task_num;
    cin >> processor_num >> task_num; //input processor number & task number

    for (int i = 0; i < processor_num; i++)
    {
        Processor processor;                      //initial each processor
        cin >> processor.id >> processor.ability; //input each processor's ID & ability
        provector.push_back(processor);           //push processor into vector

        ProcessTime protime; //initial each process time
        protime.ID = i;      //set process tims's ID & processtime
        protime.processtime = 0;
        processtime.push_back(protime); //push process time into vector
    }

    for (int i = 0; i < task_num; i++)
    {
        Task task;
        //initial each task
            cin >>
            task.ID; //input each task's parameter
        cin >> task.release_time;
        cin >> task.execution_time;
        cin >> task.deadline;
        cin >> task.period;
        cin >> task.preemption;
        cin >> task.task;
        taskvector.push_back(task); //push task into vector
    }

    //sort taskvector by it's execution_time to get the shortest task
    sort(taskvector.begin(), taskvector.end(), sort_execution_time);

    //create string for every process
    string outputstring[processor_num];
    for (int i = 0; i < processor_num; i++) //initial the output string of each processor
    {
        outputstring[i] = outputstring[i] + "Processor: " + to_string(processtime[i].ID) + "\n";
    }

    //job schedule
    int i = 0; //a integer for choose taskvector element
    while (1)
    {
        //if taskvector is empty -> all task is terminate -> program end
        if (taskvector.size() <= 0)
            break;

        //sort process time to choose the processoer of smallest process time to use
        sort(processtime.begin(), processtime.end(), sort_process_time);

        //if process time > a task's release time , the task is ready to run
        if (taskvector[i].release_time <= processtime.begin()->processtime)
        {
            //add new string to a processor's output string
            outputstring[processtime.begin()->ID] = outputstring[processtime.begin()->ID] + "\t" + to_string(processtime.begin()->processtime) + " Task" + to_string(taskvector[i].ID) + " " + to_string(processtime.begin()->processtime + taskvector[i].execution_time) + "\n";
            processtime.begin()->processtime += taskvector[i].execution_time; //add a processor's process time by task's execution time
            taskvector.erase(taskvector.begin() + i);                         //delete this task from vector ,because it is terminate
            i = 0;                                                            //reset the integer for taskvector element to 0
        }
        else
        {
            //find another task which's release time <= process time
            ++i;
            if (i >= taskvector.size())     //if all release time > process time, it means ready queue is empty, so the process need to wait until the new ready task coming
            {
                i = 0;  //reset the integer for taskvector element to 0
                sort(taskvector.begin(), taskvector.end(), sort_realease_time);     //sort the taskvector by release time, get the early task will come
                //add new string to a processor's output string
                outputstring[processtime.begin()->ID] = outputstring[processtime.begin()->ID] + "\t" + to_string(taskvector[i].release_time) + " Task" + to_string(taskvector[i].ID) + " " + to_string(taskvector[i].release_time + taskvector[i].execution_time) + "\n";
                processtime.begin()->processtime = taskvector[i].release_time + taskvector[i].execution_time;   //set the processor's process time as the task's release time + execution time
                taskvector.erase(taskvector.begin() + i);                               //delete this task from vector ,because it is terminate
                sort(taskvector.begin(), taskvector.end(), sort_execution_time);        //sort taskvector by it's execution_time to get the next shortest task
            }
        }
    }
    for (int i = 0; i < processor_num; i++)     //print the output string
    {
        cout << outputstring[i];
    }

    return 0;
}

bool sort_execution_time(Task task1, Task task2)
{
    return task1.execution_time < task2.execution_time;
}
bool sort_process_time(ProcessTime processtime1, ProcessTime processtime2)
{
    return processtime1.processtime < processtime2.processtime;
}
bool sort_realease_time(Task task1, Task task2)
{
    return task1.release_time < task2.release_time;
}