//
// Created by bmuscede on 01/02/18.
//

#include "FunctionWalker.h"
#include <iostream>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;
using namespace std;

void FunctionWalker::printGraph(std::string filename){
    for (pair<string, string> entry : callRel){
        cout << entry.first << " -> " << entry.second << endl;
    }
}

void FunctionWalker::run(const MatchFinder::MatchResult &result){
    if (const FunctionDecl *functionDecl = result.Nodes.getNodeAs<clang::FunctionDecl>(types[FUNC_DEC])) {
        //Get whether we have a system header.
        if (isInSystemHeader(result, functionDecl)) return;

        string funcName = functionDecl->getQualifiedNameAsString();
        funcName.replace(funcName.begin(), funcName.end(), ':', '-');

        functions.push_back(funcName);
    } else if (const CallExpr *expr = result.Nodes.getNodeAs<clang::CallExpr>(types[FUNC_CALLEE])) {
        if (expr->getCalleeDecl() == nullptr || !(isa<const clang::FunctionDecl>(expr->getCalleeDecl()))) return;
        auto callee = expr->getCalleeDecl()->getAsFunction();
        auto caller = result.Nodes.getNodeAs<clang::DeclaratorDecl>(types[FUNC_CALLER]);

        //Get whether this call expression is a system header.
        if (isInSystemHeader(result, callee)) return;

        string calleeName = callee->getQualifiedNameAsString();
        string callerName = caller->getQualifiedNameAsString();
        calleeName.replace(calleeName.begin(), calleeName.end(), ':', '-');
        callerName.replace(callerName.begin(), callerName.end(), ':', '-');

        callRel.push_back(pair<string, string>(callerName, calleeName));
    }
}

void FunctionWalker::generateASTMatches(MatchFinder *finder){
    //Finds function declarations for current C/C++ file.
    finder->addMatcher(functionDecl(isDefinition()).bind(types[FUNC_DEC]), this);

    //Finds function calls from one function to another.
    finder->addMatcher(callExpr(hasAncestor(functionDecl().bind(types[FUNC_CALLER]))).bind(types[FUNC_CALLEE]), this);
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