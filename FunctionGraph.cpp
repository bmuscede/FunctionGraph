#include <iostream>
#include <vector>
#include <boost/filesystem.hpp>
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include <llvm/Support/CommandLine.h>
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "FunctionWalker.h"

using namespace std;
using namespace boost::filesystem;
using namespace clang::tooling;
using namespace clang::ast_matchers;

const string DEFAULT_START = "./FunctionGraph";
const string INCLUDE_DIR = "./include";
const string INCLUDE_DIR_LOC = "--extra-arg=-I" + INCLUDE_DIR;
const int BASE_LEN = 2;
const string OUTPUT_FILENAME = "output.ta";
const string C_FILE_EXT = ".c";
const string CPLUS_FILE_EXT = ".cc";
const string CPLUSPLUS_FILE_EXT = ".cpp";

const string IGNORE_FILES = "FuncIgnore.db";

vector<path> files;
vector<std::string> ext;

char** prepareArgs(int *argc){
    int size = BASE_LEN + (int) files.size();

    //Sets argc.
    *argc = size;

    //Next, argv.
    char** argv = new char*[size];

    //Copies the base start.
    argv[0] = new char[DEFAULT_START.size() + 1];
    argv[1] = new char[INCLUDE_DIR_LOC.size() + 1];

    //Next, moves them over.
    strcpy(argv[0], DEFAULT_START.c_str());
    strcpy(argv[1], INCLUDE_DIR_LOC.c_str());

    //Next, loops through the files and copies.
    for (int i = 0; i < files.size(); i++){
        string curFile = canonical(files.at(i)).string();
        argv[i + BASE_LEN] = new char[curFile.size() + 1];
        strcpy(argv[i + BASE_LEN], curFile.c_str());
    }

    return argv;
}

int addFile(path file){
    //Gets the string that is added.
    files.push_back(file);
    return 1;
}

int addDirectory(const path directory){
    int numAdded = 0;
    vector<path> interiorDir = vector<path>();
    directory_iterator endIter;

    //Start by iterating through and inspecting each file.
    for (directory_iterator iter(directory); iter != endIter; iter++){
        //Check what the current file is.
        if (is_regular_file(iter->path())){
            //Check the extension.
            string extFile = extension(iter->path());

            //Iterates through the extension vector.
            for (int i = 0; i < ext.size(); i++){
                //Checks the file.
                if (extFile.compare(ext.at(i)) == 0){
                    numAdded += addFile(iter->path());
                }
            }
        } else if (is_directory(iter->path())){
            //Add the directory to the search system.
            interiorDir.push_back(iter->path());
        }
    }

    //Next, goes to all the internal directories.
    for (path cur : interiorDir){
        numAdded += addDirectory(cur);
    }

    return numAdded;
}

string trim(string& str) {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last-first+1));
}

vector<string> loadLibraries(string libs){
    vector<string> output;
    string line;

    std::ifstream inputFile(libs);
    while(getline(inputFile, line)){
        line = trim(line);
        if (line[0] == '#' || line.length() == 0) continue;

        output.push_back(line);
    }

    return output;
}

int main() {
    ext.push_back(C_FILE_EXT);
    ext.push_back(CPLUS_FILE_EXT);
    ext.push_back(CPLUSPLUS_FILE_EXT);

    cout << "Function Call Generator" << endl;
    cout << "By: Bryan Muscedere" << endl << endl;

    string line;
    bool invalid = true;
    while (invalid){
        cout << "Enter Top Level File Path to Analyze: ";
        getline(cin, line);

        if (!line.empty()) invalid = false;
    }

    //Add the directory.
    int num = addDirectory(line);
    cout << "Found " << num << " C++ files. ";
    if (num == 0){
        cout << "Now exiting." << endl;
        return 1;
    }
    cout << "Now processing." << endl;

    //Starts Clang
    llvm::cl::OptionCategory FunctionGraphCategory("FunctionGraph Options");

    //Creates the command line arguments.
    int argc = 0;
    char** argv = prepareArgs(&argc);

    //Sets up the processor.
    CommonOptionsParser OptionsParser(argc, (const char**) argv, FunctionGraphCategory);
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());

    //Generates files to ignore.
    vector<string> pathsToIgnore = loadLibraries(IGNORE_FILES);

    auto walker = new FunctionWalker();
    walker->setIgnoreLibs(pathsToIgnore);

    //Generates a matcher system.
    MatchFinder finder;
    walker->generateASTMatches(&finder);

    //Runs the Clang tool.
    int code = Tool.run(newFrontendActionFactory(&finder).get());

    //Gets the code and checks for warnings.
    if (code != 0) {
        cerr << "Warning: Compilation errors were detected." << endl;
    }

    walker->printGraph(OUTPUT_FILENAME);
    return 0;
}
