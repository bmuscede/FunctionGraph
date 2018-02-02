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

private:
    std::vector<std::string> functions;
    std::vector<std::pair<std::string, std::string>> callRel;

    enum {FUNC_DEC = 0, FUNC_CALLER, FUNC_CALLEE};
    const char* types[3] = {"func_dec", "caller", "callee"};

    bool isInSystemHeader(const MatchFinder::MatchResult &result, const clang::Decl *decl);
};


#endif //FUNCTIONGRAPH_FUNCTIONWALKER_H
