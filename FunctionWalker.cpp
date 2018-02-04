//
// Created by bmuscede on 01/02/18.
//

#include "FunctionWalker.h"
#include <iostream>
#include <fstream>
#include <openssl/md5.h>
#include <cstring>

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
        outputFile << INSTANCE_FLAG << " " << getMD5(function) << " " << FUNCTION_FLAG << endl;
    }
    outputFile << endl;

    for (pair<string, string> contain : containRel){
        outputFile << CONTAIN_FLAG << " " << contain.first << " " << getMD5(contain.second) << endl;
    }
    outputFile << endl;

    for (pair<string, string> call : callRel){
        outputFile << CALL_FLAG << " " << getMD5(call.first) << " " << getMD5(call.second) << endl;
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

        string funcName = generateID(result, functionDecl);
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

        string calleeName = generateID(result, callee);
        string callerName = generateID(result, caller);
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

string FunctionWalker::generateID(const MatchFinder::MatchResult &result, const NamedDecl* decl){
    //Gets the canonical decl.
    decl = dyn_cast<NamedDecl>(decl->getCanonicalDecl());
    string name = "";

    //Generates a special name for function overloading.
    if (isa<FunctionDecl>(decl) || isa<CXXMethodDecl>(decl)){
        const FunctionDecl* cur = decl->getAsFunction();
        name = cur->getReturnType().getAsString() + "-" + decl->getNameAsString();
        for (int i = 0; i < cur->getNumParams(); i++){
            name += "-" + cur->parameters().data()[i]->getType().getAsString();
        }
    } else {
        name = decl->getNameAsString();
    }


    bool getParent = true;
    bool recurse = false;
    const NamedDecl* originalDecl = decl;

    //Get the parent.
    auto parent = result.Context->getParents(*decl);
    while(getParent){
        //Check if it's empty.
        if (parent.empty()){
            getParent = false;
            continue;
        }

        //Get the current decl as named.
        decl = parent[0].get<clang::NamedDecl>();
        if (decl) {
            name = generateID(result, decl) + "::" + name;
            recurse = true;
            getParent = false;
            continue;
        }

        parent = result.Context->getParents(parent[0]);
    }

    //Sees if no true qualified name was used.
    Decl::Kind kind = originalDecl->getKind();
    if (!recurse) {
        if (kind != Decl::Function && kind == Decl::CXXMethod){
            //We need to get the parent function.
            const DeclContext *parentContext = originalDecl->getParentFunctionOrMethod();

            //If we have nullptr, get the parent function.
            if (parentContext != nullptr) {
                string parentQualName = generateID(result, static_cast<const FunctionDecl *>(parentContext));
                name = parentQualName + "::" + originalDecl->getNameAsString();
            }
        }
    }

    //Finally, check if we have a main method.
    if (name.compare("int-main-int-char **") == 0 || name.compare("int-main") == 0){
        name = result.Context->getSourceManager().getFilename(originalDecl->getLocStart()).str() + "--" + name;
    }

    return name;
}


string FunctionWalker::getMD5(string ID){
    //Creates a digest buffer.
    unsigned char digest[MD5_DIGEST_LENGTH];
    const char* cText = ID.c_str();

    //Initializes the MD5 string.
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, cText, strlen(cText));
    MD5_Final(digest, &ctx);

    //Fills it with characters.
    char mdString[MD5_LENGTH];
    for (int i = 0; i < 16; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

    return string(mdString);
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
            string functionName = generateID(result, decl);
            string className = classDecl->getQualifiedNameAsString();
            replace(functionName.begin(), functionName.end(), ':', '-');
            replace(className.begin(), className.end(), ':', '-');
            containRel.emplace_back(pair<string, string>(className, functionName));

            return;

        }

        parent = result.Context->getParents(parent[0]);
    }
}