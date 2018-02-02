//
// Created by bmuscede on 01/02/18.
//

#include "FunctionWalker.h"
#include <iostream>
#include <fstream>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;
using namespace std;

void FunctionWalker::printGraph(std::string filename) {
    //Print results.
    ofstream outputFile(filename);
    if (!outputFile.is_open()) {
        cerr << "Problem outputting graph! Please try again." << endl;
        return;
    }

    for (string clazz : classes){
        outputFile << INSTANCE_FLAG << " " << clazz << " " << CLASS_FLAG << endl;
    }
    outputFile << endl;

    for (string function : functions){
        outputFile << INSTANCE_FLAG << " " << function << " " << FUNCTION_FLAG << endl;
    }
    outputFile << endl;

    for (pair<string, string> contain : containRel){
        outputFile << CONTAIN_FLAG << " " << contain.first << " " << contain.second << endl;
    }
    outputFile << endl;

    for (pair<string, string> call : callRel){
        outputFile << CALL_FLAG << " " << call.first << " " << call.second << endl;
    }
    outputFile << endl;

    //Closes the file.
    outputFile.close();
    cout << "Successfully wrote graph to " << filename << "!" << endl;
}

void FunctionWalker::run(const MatchFinder::MatchResult &result){
    if (const FunctionDecl *functionDecl = result.Nodes.getNodeAs<clang::FunctionDecl>(types[FUNC_DEC])) {
        //Get whether we have a system header.
        if (isInSystemHeader(result, functionDecl)) return;

        string funcName = functionDecl->getQualifiedNameAsString();
        replace(funcName.begin(), funcName.end(), ':', '-');

        if (!exists(functions, funcName)) {
            functions.push_back(funcName);
            addParentClass(result, functionDecl);
        }
    } else if (const CallExpr *expr = result.Nodes.getNodeAs<clang::CallExpr>(types[FUNC_CALLEE])) {
        if (expr->getCalleeDecl() == nullptr || !(isa<const clang::FunctionDecl>(expr->getCalleeDecl()))) return;
        auto callee = expr->getCalleeDecl()->getAsFunction();
        auto caller = result.Nodes.getNodeAs<clang::DeclaratorDecl>(types[FUNC_CALLER]);

        //Get whether this call expression is a system header.
        if (isInSystemHeader(result, callee)) return;

        string calleeName = callee->getQualifiedNameAsString();
        string callerName = caller->getQualifiedNameAsString();
        replace(calleeName.begin(), calleeName.end(), ':', '-');
        replace(callerName.begin(), callerName.end(), ':', '-');

        callRel.emplace_back(pair<string, string>(callerName, calleeName));
    } else if (const CXXRecordDecl *classDecl = result.Nodes.getNodeAs<clang::CXXRecordDecl>(types[CLASS_DEC])){
        //Get whether we have a system header.
        if (isInSystemHeader(result, classDecl)) return;

        string className = classDecl->getNameAsString();
        replace(className.begin(), className.end(), ':', '-');

        if (!exists(classes, className)) classes.push_back(className);
    }
}

void FunctionWalker::generateASTMatches(MatchFinder *finder){
    //Finds function declarations for current C/C++ file.
    finder->addMatcher(functionDecl(isDefinition()).bind(types[FUNC_DEC]), this);

    //Finds function calls from one function to another.
    finder->addMatcher(callExpr(hasAncestor(functionDecl().bind(types[FUNC_CALLER]))).bind(types[FUNC_CALLEE]), this);

    //Finds class declarations for the current C++ file.
    finder->addMatcher(cxxRecordDecl(isClass()).bind(types[CLASS_DEC]), this);
}

bool FunctionWalker::isInSystemHeader(const MatchFinder::MatchResult &result, const Decl *decl){
    if (decl == nullptr) return false;
    bool isIn;

    //Some system headers cause Clang to segfault.
    try {
        //Gets where this item is located.
        auto &SourceManager = result.Context->getSourceManager();
        auto ExpansionLoc = SourceManager.getExpansionLoc(decl->getLocStart());

        //Checks if we have an invalid location.
        if (ExpansionLoc.isInvalid()) {
            return false;
        }

        //Now, checks if we don't have a system header.
        isIn = SourceManager.isInSystemHeader(ExpansionLoc);
    } catch ( ... ){
        return false;
    }

    return isIn;
}

bool FunctionWalker::exists(vector<string> list, string item){
    for (string src : list){
        if (src == item) return true;
    }
    return false;
}

void FunctionWalker::addParentClass(const MatchFinder::MatchResult &result, const FunctionDecl *decl){
    //Use the manual walker system.
    bool getParent = true;
    const CXXRecordDecl* classDecl;
    decl = decl->getDefinition()->getCanonicalDecl();

    //Get the parent.
    auto parent = result.Context->getParents(*decl);
    while (getParent) {
        //Check if it's empty.
        if (parent.empty()) {
            getParent = false;
            continue;
        }

        //Get the current decl as named.
        classDecl = parent[0].get<clang::CXXRecordDecl>();
        if (classDecl) {
            string functionName = decl->getQualifiedNameAsString();
            string className = classDecl->getQualifiedNameAsString();
            replace(functionName.begin(), functionName.end(), ':', '-');
            replace(className.begin(), className.end(), ':', '-');
            containRel.emplace_back(pair<string, string>(className, functionName));

            return;

        }

        parent = result.Context->getParents(parent[0]);
    }

    //Checks if we can add a class reference (secondary attempt).
    NestedNameSpecifier* declSpec = decl->getQualifier();
    if (declSpec != nullptr && declSpec->getAsType() == nullptr)
        classDecl = declSpec->getAsType()->getAsCXXRecordDecl();
    if (classDecl != nullptr) {
        string functionName = decl->getQualifiedNameAsString();
        string className = classDecl->getQualifiedNameAsString();
        replace(functionName.begin(), functionName.end(), ':', '-');
        replace(className.begin(), className.end(), ':', '-');
        containRel.emplace_back(pair<string, string>(className, functionName));
    }
}