#include "Solver.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>

#include <cmath>


using namespace std;


namespace ck {

#pragma region Solver::Cli
int Solver::Cli::run(int argc, char * argv[]) {
    Log(LogSwitch::Ck::Cli) << "parse command line arguments." << endl;
    Set<String> switchSet;
    Map<String, char*> optionMap({ // use string as key to compare string contents instead of pointers.
        { InstancePathOption(), nullptr },
        { SolutionPathOption(), nullptr },
        { RandSeedOption(), nullptr },
        { TimeoutOption(), nullptr },
        { MaxIterOption(), nullptr },
        { JobNumOption(), nullptr },
        { RunIdOption(), nullptr },
        { EnvironmentPathOption(), nullptr },
        { ConfigPathOption(), nullptr },
        { LogPathOption(), nullptr }
    });

    for (int i = 1; i < argc; ++i) { // skip executable name.
        auto mapIter = optionMap.find(argv[i]);
        if (mapIter != optionMap.end()) { // option argument.
            mapIter->second = argv[++i];
        } else { // switch argument.
            switchSet.insert(argv[i]);
        }
    }

    Log(LogSwitch::Ck::Cli) << "execute commands." << endl;
    if (switchSet.find(HelpSwitch()) != switchSet.end()) {
        cout << HelpInfo() << endl;
    }

    if (switchSet.find(AuthorNameSwitch()) != switchSet.end()) {
        cout << AuthorName() << endl;
    }

    Solver::Environment env;
    env.load(optionMap);
    if (env.instPath.empty() || env.slnPath.empty()) { return -1; }

    Solver::Configuration cfg;
    cfg.load(env.cfgPath);

    Log(LogSwitch::Ck::Input) << "load instance " << env.instPath << " (seed=" << env.randSeed << ")." << endl;
    Problem::Input input;
    if (!input.load(env.instPath)) { return -1; }

    Solver solver(input, env, cfg);
    solver.solve();

    pb::Submission submission;
    submission.set_thread(to_string(env.jobNum));
    submission.set_instance(env.friendlyInstName());
    submission.set_duration(to_string(solver.timer.elapsedSeconds()) + "s");

    solver.output.save(env.slnPath, submission);
    #if CK_DEBUG
    solver.output.save(env.solutionPathWithTime(), submission);
    solver.record();
    #endif // CK_DEBUG

    return 0;
}
#pragma endregion Solver::Cli

#pragma region Solver::Environment
void Solver::Environment::load(const Map<String, char*> &optionMap) {
    char *str;

    str = optionMap.at(Cli::EnvironmentPathOption());
    if (str != nullptr) { loadWithoutCalibrate(str); }

    str = optionMap.at(Cli::InstancePathOption());
    if (str != nullptr) { instPath = str; }

    str = optionMap.at(Cli::SolutionPathOption());
    if (str != nullptr) { slnPath = str; }

    str = optionMap.at(Cli::RandSeedOption());
    if (str != nullptr) { randSeed = atoi(str); }

    str = optionMap.at(Cli::TimeoutOption());
    if (str != nullptr) { msTimeout = static_cast<Duration>(atof(str) * Timer::MillisecondsPerSecond); }

    str = optionMap.at(Cli::MaxIterOption());
    if (str != nullptr) { maxIter = atoi(str); }

    str = optionMap.at(Cli::JobNumOption());
    if (str != nullptr) { jobNum = atoi(str); }

    str = optionMap.at(Cli::RunIdOption());
    if (str != nullptr) { rid = str; }

    str = optionMap.at(Cli::ConfigPathOption());
    if (str != nullptr) { cfgPath = str; }

    str = optionMap.at(Cli::LogPathOption());
    if (str != nullptr) { logPath = str; }

    calibrate();
}

void Solver::Environment::load(const String &filePath) {
    loadWithoutCalibrate(filePath);
    calibrate();
}

void Solver::Environment::loadWithoutCalibrate(const String &filePath) {
    // EXTEND[ck][8]: load environment from file.
    // EXTEND[ck][8]: check file existence first.
}

void Solver::Environment::save(const String &filePath) const {
    // EXTEND[ck][8]: save environment to file.
}
void Solver::Environment::calibrate() {
    // adjust thread number.
    int threadNum = thread::hardware_concurrency();
    if ((jobNum <= 0) || (jobNum > threadNum)) { jobNum = threadNum; }

    // adjust timeout.
    msTimeout -= Environment::SaveSolutionTimeInMillisecond;
}
#pragma endregion Solver::Environment

#pragma region Solver::Configuration
void Solver::Configuration::load(const String &filePath) {
    // EXTEND[ck][5]: load configuration from file.
    // EXTEND[ck][8]: check file existence first.
}

void Solver::Configuration::save(const String &filePath) const {
    // EXTEND[ck][5]: save configuration to file.
}
#pragma endregion Solver::Configuration

#pragma region Solver
bool Solver::solve() {
    init();

    int workerNum = (max)(1, env.jobNum / cfg.threadNumPerWorker);
    cfg.threadNumPerWorker = env.jobNum / workerNum;
    List<Solution> solutions(workerNum, Solution(this));
    List<bool> success(workerNum);

    Log(LogSwitch::Ck::Framework) << "launch " << workerNum << " workers." << endl;
    List<thread> threadList;
    threadList.reserve(workerNum);
    for (int i = 0; i < workerNum; ++i) {
        // TODO[ck][2]: as *this is captured by ref, the solver should support concurrency itself, i.e., data members should be read-only or independent for each worker.
        // OPTIMIZE[ck][3]: add a list to specify a series of algorithm to be used by each threads in sequence.
        threadList.emplace_back([&, i]() { success[i] = optimize(solutions[i], i); });
    }
    for (int i = 0; i < workerNum; ++i) { threadList.at(i).join(); }

    Log(LogSwitch::Ck::Framework) << "collect best result among all workers." << endl;
    int bestIndex = -1;
    int minLength = 999;
    for (int i = 0; i < workerNum; ++i) {
        if (!success[i]) { continue; }
        Log(LogSwitch::Ck::Framework) << "worker " << i << " got " << solutions[i].length()<< endl;
        if (solutions[i].length()  > minLength) { continue; }
        bestIndex = i;
        minLength = solutions[i].length();
    }

    env.rid = to_string(bestIndex);
    if (bestIndex < 0) { return false; }
    output = solutions[bestIndex];
    return true;
}

void Solver::record() const {
    #if CK_DEBUG
    int generation = 0;

    ostringstream log;

    System::MemoryUsage mu = System::peakMemoryUsage();

    int obj = output.length();
    Length checkerObj = -1;
    bool feasible = check(checkerObj);

    // record basic information.
	log << env.friendlyLocalTime() << ","
		<< env.rid << ","
		<< env.instPath << ","
		<< feasible << "," << (obj - checkerObj) << ","
		<< output.length() << ","
		<< timer.elapsedSeconds() << ","
		<< mu.physicalMemory << "," << mu.virtualMemory << ","
		<< env.randSeed << ","
		<< cfg.toBriefStr() << ","
		<< generation << "," << iteration;

    // record solution vector.
    // EXTEND[ck][2]: save solution in log.
    log << endl;

    // append all text atomically.
    static mutex logFileMutex;
    lock_guard<mutex> logFileGuard(logFileMutex);

    ofstream logFile(env.logPath, ios::app);
    logFile.seekp(0, ios::end);
    if (logFile.tellp() <= 0) {
        logFile << "Time,ID,Instance,Feasible,ObjMatch,Width,Duration,PhysMem,VirtMem,RandSeed,Config,Generation,Iteration,Ratio,Solution" << endl;
    }
    logFile << log.str();
    logFile.close();
    #endif // CK_DEBUG
}

bool Solver::check(Length &checkerObj) const {
    #if CK_DEBUG
    enum CheckerFlag {
		IoError = 0x0,
		FormatError = 0x1,
		CoordinateOverError = 0x2,
		RectangleOverlapError = 0x4,
		LackingRectanglesError = 0x8
    };

    checkerObj = System::exec("Checker.exe " + env.instPath + " " + env.solutionPathWithTime());
    if (checkerObj > 0) { return true; }
    checkerObj = ~checkerObj;
    if (checkerObj == CheckerFlag::IoError) { Log(LogSwitch::Checker) << "IoError." << endl; }
    if (checkerObj & CheckerFlag::FormatError) { Log(LogSwitch::Checker) << "FormatError." << endl; }
    if (checkerObj & CheckerFlag::CoordinateOverError) { Log(LogSwitch::Checker) << "CoordinateOverError." << endl; }
    if (checkerObj & CheckerFlag::RectangleOverlapError) { Log(LogSwitch::Checker) << "RectangleOverlapError." << endl; }
	if (checkerObj & CheckerFlag::LackingRectanglesError) { Log(LogSwitch::Checker) << "LackingRectanglesError" << endl; }
    return false;
    #else
    checkerObj = 0;
    return true;
    #endif // CK_DEBUG
}

void Solver::init() {

}

bool Solver::optimize(Solution &sln, ID workerId) {
	Log(LogSwitch::Ck::Framework) << "worker " << workerId << " starts." << endl;
	Random rand;
	int rectangleNum = input.rectangles().size();
	bool status = true;
	int maxLength = 0;

	//replace the following random assignment with your own algorithm
    for (int i = 0; !timer.isTimeOut() && (i < rectangleNum); i++) {
        int x = rand.pick(0, 100);
        int y = rand.pick(0, 100);
        auto &placement(*sln.add_placements());
        placement.set_id(i);
        placement.set_x(x);
        placement.set_y(y);
        placement.set_rotated(false);
        maxLength = x + input.rectangles(i).width() > maxLength ? x + input.rectangles(i).width() : maxLength;
        maxLength = y + input.rectangles(i).height() > maxLength ? y + input.rectangles(i).height() : maxLength;
    }
	sln.set_length(maxLength);
	
	Log(LogSwitch::Ck::Framework) << "worker " << workerId << " ends." << endl;
	return status;
	
    
}
#pragma endregion Solver

}
