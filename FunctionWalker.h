//
// Created by bmuscede on 01/02/18.
//

#ifndef FUNCTIONGRAPH_FUNCTIONWALKER_H
#define FUNCTIONGRAPH_FUNCTIONWALKER_H

#include <string>
#include <vector>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang::ast_matchers;

class FunctionWalker : public MatchFinder::MatchCallback {

public:
    void printGraph(std::string filename);

    void run(const MatchFinder::MatchResult &result) override;
    void generateASTMatches(MatchFinder *finder);

    void setIgnoreLibs(std::vector<std::string> libs);

private:
    const std::string INSTANCE_FLAG = "$INSTANCE";
    const std::string CLASS_FLAG = "cClass";
    const std::string FUNCTION_FLAG = "cFunction";
    const std::string CONTAIN_FLAG = "contain";
    const std::string CALL_FLAG = "call";
    int const MD5_LENGTH = 33;

    std::vector<std::string> ignoreLibs;

    std::vector<std::string> functions;
    std::vector<std::string> classes;
    std::vector<std::pair<std::string, std::string>> callRel;
    std::vector<std::pair<std::string, std::string>> containRel;

    enum {FUNC_DEC = 0, FUNC_CALLER, FUNC_CALLEE, CLASS_DEC};
    const char* types[4] = {"func_dec", "caller", "callee", "class_dec"};

    bool isInSystemHeader(const MatchFinder::MatchResult &result, const clang::Decl *decl);
    bool exists(std::vector<std::string> list, std::string item);
    std::string generateID(const MatchFinder::MatchResult &result, const clang::NamedDecl* decl);
    std::string getMD5(std::string ID);

    void addParentClass(const MatchFinder::MatchResult &result, const clang::FunctionDecl *decl);
};


#endif //FUNCTIONGRAPH_FUNCTIONWALKER_H
