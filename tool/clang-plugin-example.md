https://github.com/iclang-project/clang-plugin-test

cmake:

```cmake
cmake_minimum_required(VERSION 3.17)
project(clang-plugin-test)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(
        "$<$<CXX_COMPILER_ID:GNU,Clang>:-fno-rtti>"
)

set(MY_LLVM "/home/hzy/hzydata/softwares/llvm/clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04/lib/cmake/" CACHE STRING "LLVM lib/cmake path")

message("MY_LLVM: ${MY_LLVM}")

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${MY_LLVM})

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message("LLVM STATUS:
  Definitions ${LLVM_DEFINITIONS}
  Includes    ${LLVM_INCLUDE_DIRS}
${CLANG_INCLUDE_DIRS}
  Libraries   ${LLVM_LIBRARY_DIRS}"
)

add_definitions(${LLVM_DEFINITIONS})

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

link_directories(${LLVM_LIBRARY_DIRS})

add_library(myplugin MODULE myplugin.cpp)
```

myplugin.cpp:

```c++
#include <memory>
#include <string>
#include <vector>
#include <sstream>

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Mangle.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/TextNodeDumper.h"

using namespace clang;

namespace {
    class MyVisitor : public RecursiveASTVisitor<MyVisitor> {
    public:
        ASTContext &context;
        SourceManager &sourceManager;
        ASTNameGenerator astNameGenerator;
        int depth = 0;

        explicit MyVisitor(CompilerInstance &instance) :
                context(instance.getASTContext()),
                sourceManager(instance.getSourceManager()),
                astNameGenerator(instance.getASTContext()) {}

//        bool shouldVisitTemplateInstantiations() const { return true; }

        bool TraverseDecl(Decl *decl) {
            if (!decl) {
                return true;
            }

            depth += 1;
            std::string space = calSpaces(depth);

            llvm::errs() << space << dumpDecl(decl) << " ";

            if (auto *typedefDecl = dyn_cast<TypedefDecl>(decl)) {
                llvm::errs() << "first:" << typedefDecl->getFirstDecl() << " ";
            } else if (auto *typeAliasDecl = dyn_cast<TypeAliasDecl>(decl)) {
                llvm::errs() << "first:" << typeAliasDecl->getFirstDecl() << " ";
            } else if (auto *namespaceAliasDecl = dyn_cast<NamespaceAliasDecl>(decl)) {
                llvm::errs() << "first:" << namespaceAliasDecl->getFirstDecl() << " ";
            } else if (auto *usingShadowDecl = dyn_cast<UsingShadowDecl>(decl)) {
                llvm::errs() << "first:" << usingShadowDecl->getFirstDecl() << " ";
            } else if (auto *functionDecl = dyn_cast<FunctionDecl>(decl)) {
                llvm::errs() << "first:" << (void*)functionDecl->getFirstDecl() << " ";
                llvm::errs() << "def:" << (void*)functionDecl->getDefinition() << " ";

                llvm::errs() << "namePrefix:{" << expandPrefix(functionDecl) << "} ";
                llvm::errs() << "qName:" << functionDecl->getQualifiedNameAsString() << " ";
                llvm::errs() << "nameSR:{" <<
                             dumpExpansionRange(functionDecl->getQualifierLoc().getSourceRange())->toString() << ", " <<
                             dumpExpansionRange(functionDecl->getNameInfo().getSourceRange())->toString() << "} ";

                // clang/lib/AST/TypePrinter.cpp
                QualType returnType = functionDecl->getReturnType();
                auto retType = functionDecl->getReturnType();
                llvm::errs() << "rType:{" << retType->getTypeClassName() << " " <<
                dumpExpansionRange(functionDecl->getReturnTypeSourceRange())->toString() << " ";
                auto typeClass = retType->getTypeClass();
                if (typeClass == Type::Pointer) {
                    const auto *pointerType = llvm::cast<PointerType>(retType);
                    llvm::errs() << "->" << pointerType->getPointeeType()->getTypeClassName() << " ";
                } else if (typeClass == Type::Builtin) {
                    // ...
                } else if (typeClass == Type::Elaborated) {
                    const auto *elaboratedType = llvm::cast<ElaboratedType>(retType);
                    QualType namedType = elaboratedType->getNamedType();
                    llvm::errs() << "| nameType: " << namedType->getTypeClassName() << " ";
                    Decl *nameTarget = nullptr;
                    if (const TypedefType *typedefType = namedType->getAs<TypedefType>()) {
                        nameTarget = typedefType->getDecl();
                    } else if (const TagType *tagType = namedType->getAs<TagType>()) {
                        nameTarget = tagType->getDecl();
                    }
                    llvm::errs() << "| nameTarget: " << (void*)nameTarget << " ";
                    if (nameTarget != nullptr) {
                        llvm::errs() << "'" << expandPrefix(nameTarget) << "' ";
                    }
                    auto *target = elaboratedType->getAsTagDecl();
                    llvm::errs() << "| target: " << (void*)target << " ";
                    if (target != nullptr) {
                        llvm::errs() << "'" << expandPrefix(target) << "' ";
                    }
                }
                llvm::errs() << "} ";

                llvm::errs() << "Params:{";
                for (unsigned i = 0; i < functionDecl->getNumParams(); ++i) {
                    ParmVarDecl *param = functionDecl->getParamDecl(i);
                    llvm::outs() << " @" << param->getNameAsString();
                    if (param->hasDefaultArg()) {
                        llvm::outs() << " (default value present)";
                    }
                }
                llvm::outs() << " } ";

                llvm::errs() << "attrs:{ ";
                if (functionDecl->hasAttrs()) {
                    for (auto &attr : functionDecl->attrs()) {
                        if (attr->getAttrName()) {
                            llvm::errs() << attr->getAttrName()->getName() << " ";
                        } else if (isa<AsmLabelAttr>(attr)) {
                            llvm::outs() << "__asm__ ";
                        }
                    }
                }
                llvm::errs() << "} ";

                llvm::errs() << "PriTemp:" << (void*)functionDecl->getPrimaryTemplate() << " ";
                llvm::errs() << "DesTemp:" << (void*)functionDecl->getDescribedFunctionTemplate() << " ";
                auto linkInfo = functionDecl->getLinkageAndVisibility();
                llvm::errs() << "link:{" << linkInfo.getVisibility() << "," << (int)linkInfo.getLinkage() << "}";
                llvm::errs() << "static:" << functionDecl->isStatic() << " ";
                llvm::errs() << "operator:" << functionDecl->getOverloadedOperator() << " ";
                llvm::errs() << "inline:{" << functionDecl->isInlineSpecified() << "," << functionDecl->isInlined() << "} ";
                llvm::errs() << "friend:" << functionDecl->getFriendObjectKind() << " ";

                if (auto *cxxMethodDecl = dyn_cast<CXXMethodDecl>(decl)) {
                    llvm::errs() << "virtual:" << cxxMethodDecl->isVirtual() << " ";
                }
            } else if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
                llvm::errs() << "first:" << (void*)varDecl->getFirstDecl() << " ";
                llvm::errs() << "def: " << (void*)varDecl->getDefinition() << " ";

                QualType type = varDecl->getType();
                llvm::errs() << "type:{typeClassName:" << type->getTypeClassName() <<
                ",typeAsString:" << type.getAsString() <<
                ",dumpType:" << dumpType(type) << "} ";

                llvm::errs() << "DesVarTemplate:" << (void*)varDecl->getDescribedVarTemplate() << " ";

                if (auto *varSpecDecl =
                        llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
                    llvm::errs() << "(spec)template:" << (void*)varSpecDecl->getSpecializedTemplate() << " ";
                }
            } else if (auto *cxxRecordDecl = dyn_cast<CXXRecordDecl>(decl)) {
                llvm::errs() << "first:" << (void*)cxxRecordDecl->getFirstDecl() << " ";
                llvm::errs() << "def:" << (void*)cxxRecordDecl->getDefinition() << " ";

                llvm::errs() << "extends:{ ";
                if (cxxRecordDecl->isCompleteDefinition()) {
                    for (auto & base : cxxRecordDecl->bases()) {
                        auto tp = base.getType();
                        if (!tp.isNull()) {
                            auto fClass =  base.getType()->getAsCXXRecordDecl();
                            if (fClass != nullptr) {
                                llvm::errs() << fClass->getNameAsString() << " ";
                            }
                        }
                    }
                }
                llvm::errs() << "} ";

                llvm::errs() << "DesClasstemplate:" << (void*)cxxRecordDecl->getDescribedClassTemplate() << " ";

                if (auto *classSpecDecl = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
                    llvm::errs() << "(sepc)template:" << (void*)classSpecDecl->getSpecializedTemplate() << " ";
                }
            }

            llvm::errs() << "\n";

            bool res = RecursiveASTVisitor::TraverseDecl(decl);

            depth -= 1;

            return res;
        }

        bool TraverseStmt(Stmt *stmt, DataRecursionQueue *queue = nullptr) {
            if (!stmt) {
                return true;
            }

            depth += 1;
            std::string space = calSpaces(depth + 1);

            llvm::errs() << space << stmt->getStmtClassName() << " ";

            if (auto *declRefExpr = dyn_cast<DeclRefExpr>(stmt)) {
                llvm::errs() << "dest:" << dumpDecl(declRefExpr->getDecl()) << " ";
            } else if (auto *memberExpr = dyn_cast<MemberExpr>(stmt)) {
                llvm::errs() << "dest:" << dumpDecl(memberExpr->getMemberDecl()) << " ";
            } else if (auto *unresolvedLookupExpr = dyn_cast<UnresolvedLookupExpr>(stmt)) {
                llvm::errs() << "dest name(unresolved):{" << unresolvedLookupExpr->getName().getAsString() << ",";
                for (auto *decl : unresolvedLookupExpr->decls()) {
                    llvm::errs() << ((void*)(decl)) << ",";
                }
                llvm::errs() << "} ";
            } else if (auto* tempMemberExpr = dyn_cast<CXXDependentScopeMemberExpr>(stmt)) {
                llvm::errs() << "dest member(unresolved):" << tempMemberExpr->getMemberNameInfo().getName() << " ";
            }

            llvm::errs() << "\n";

            bool res = RecursiveASTVisitor::TraverseStmt(stmt, queue);

            depth -= 1;

            return res;
        }

        bool VisitType(Type* type) {
            if (!type) {
                return true;
            }

            std::string space = calSpaces(depth + 1);

            llvm::errs() << space << "[" << type->getTypeClassName() << "] " ;

            llvm::errs() << "tag:" << dumpDecl(type->getAsTagDecl()) << " ";

            if (auto *tempType = dyn_cast<TemplateSpecializationType>(type)) {
                llvm::errs() << "template:" << dumpDecl(tempType->getTemplateName().getAsTemplateDecl()) << " ";
            } else if (auto *typedefType = dyn_cast<TypedefType>(type)) {
                llvm::errs() << "typedef:" << dumpDecl(typedefType->getDecl()) << " ";
            } else if (auto *recordType = dyn_cast<RecordType>(type)) {
                llvm::errs() << "record:" << dumpDecl(recordType->getDecl()) << " ";
            } else if (auto *enumType = dyn_cast<EnumType>(type)) {
                llvm::errs() << "enum:" << dumpDecl(enumType->getDecl()) << " ";
            }

            llvm::errs() << "\n";

            return true;
        }

    private:
        struct ExRange {
            bool isValid;
            unsigned startLine, startColumn, endLine, endColumn;
            unsigned startOffset, endOffset;
            unsigned fileIdHash;

            std::string toString() {
                if (!isValid) {
                    return "<invalid>";
                } else {
                    std::ostringstream oss;
                    oss << "<" << startLine << ":" << startColumn << "," << endLine << ":" << endColumn << ">" <<
                    "(" << startOffset << "," << endOffset << ")[" << fileIdHash << "]";
                    return oss.str();
                }
            }
        };

        std::string getMangledName(const NamedDecl* decl) {
            if (decl && decl->getDeclName()) {
                if (isa<RequiresExprBodyDecl>(decl->getDeclContext()))
                    return "";

                auto *varDecl = dyn_cast<VarDecl>(decl);
                if (varDecl && varDecl->hasLocalStorage())
                    return "";

                return astNameGenerator.getName(decl);
            }
            return "";
        }

        std::unique_ptr<ExRange> dumpExpansionRange(const SourceRange& sourceRange) {
            std::unique_ptr<ExRange> res = std::make_unique<ExRange>();

            res->isValid = false;

            // start
            FullSourceLoc startFullSourceLoc(sourceRange.getBegin(), sourceManager);
            if (startFullSourceLoc.isInvalid()) {
                return res;
            }startFullSourceLoc.getFileID();
            res->startLine = startFullSourceLoc.getExpansionLineNumber();
            res->startColumn = startFullSourceLoc.getExpansionColumnNumber();

            // end
            SourceLocation endSourceLoc = Lexer::getLocForEndOfToken(sourceRange.getEnd(), 0, sourceManager, context.getLangOpts());
            FullSourceLoc endFullSourceLoc(endSourceLoc, sourceManager);
            if (endFullSourceLoc.isInvalid()) {
                return res;
            }
            res->endLine = endFullSourceLoc.getExpansionLineNumber();
            res->endColumn = endFullSourceLoc.getExpansionColumnNumber();

            res->isValid = true;

            res->startOffset = startFullSourceLoc.getFileOffset();
            res->endOffset = endFullSourceLoc.getFileOffset();

            res->fileIdHash = startFullSourceLoc.getFileID().getHashValue();

            return res;
        }

        std::string dumpOriginalCode(const SourceRange& sourceRange) {
            SourceLocation startLoc = sourceRange.getBegin();
            SourceLocation endLoc = Lexer::getLocForEndOfToken(sourceRange.getEnd(), 0, sourceManager, context.getLangOpts());
            const char* start = sourceManager.getCharacterData(startLoc);
            const char* end = sourceManager.getCharacterData(endLoc);
            if (start == nullptr || end == nullptr || end - start <= 0) {
                return "";
            }
            return std::string(start, end);
        }

        std::string dumpType(QualType type) {
            std::string res;

            SplitQualType splitType = type.split();
            if (!type.isNull()) {
                splitType = type.getSplitDesugaredType();
            }

            res.append(QualType::getAsString(splitType, context.getLangOpts()));

            return res;
        }

        std::string dumpDecl(Decl* decl) {
            if (decl == nullptr) {
                return "{}";
            }

            std::ostringstream oss;
            if (decl->isImplicit()) {
                oss << "(impl)";
            }
            if (decl->isOutOfLine()) {
                oss << "(out-of-line)";
            }
            oss << "{" << decl->getDeclKindName() << "Decl " << (void*)decl << " " <<
                dumpExpansionRange(decl->getSourceRange())->toString();

            if (auto *namedDecl = dyn_cast<NamedDecl>(decl)) {
                oss << " <" << namedDecl->getNameAsString() << "> <" << getMangledName(namedDecl) << "> ";
            }

            oss << "DesTemplate:" << (void*)decl->getDescribedTemplate() << " ";

            oss << "inFunc:" << isLocalDecl(decl) << " ";

            oss << "}";

            return oss.str();
        }

        bool isLocalDecl(Decl *decl) {
            auto *declContext = decl->getDeclContext();
            while (declContext != nullptr) {
                if (declContext->isFunctionOrMethod()) {
                    return true;
                }
                declContext = declContext->getParent();
            }
            return false;
        }

        std::string calSpaces(int num) {
            std::string space;
            for (int i = 0; i < num; i += 1) {
                space.push_back(' ');
                space.push_back(' ');
                space.push_back('|');
            }
            return space;
        }

        std::string expandPrefix(Decl *decl) {
            std::ostringstream oss;

            // cal namespaces and class prefix
            SmallVector<const DeclContext *, 8> ctxs;
            for (const DeclContext *ctx = decl->getDeclContext(); ctx;
                 ctx = ctx->getParent()) {
                // Skip non-named contexts such as linkage specifications and ExportDecls.
                if (!dyn_cast<NamedDecl>(ctx)) {
                    continue;
                }
                ctxs.push_back(ctx);
            }
            for (const DeclContext *ctx : llvm::reverse(ctxs)) {
                if (const auto *nd = dyn_cast<NamespaceDecl>(ctx)) {
                    oss << nd->getNameAsString() << "::";
                } else if (const auto *rd = dyn_cast<RecordDecl>(ctx)) {
                    if (rd->getDescribedTemplate() != nullptr) {
                        oss << "(T)";
                    }
                    if (llvm::dyn_cast<ClassTemplateSpecializationDecl>(rd) != nullptr) {
                        oss << "(S)";
                    }
                    oss << rd->getNameAsString() + "::";
                } else if (const auto *spec = dyn_cast<ClassTemplateSpecializationDecl>(ctx)) {
                    oss << "(Spec)::";
                }
            }
            return oss.str();
        }

    };

    class MyConsumer : public ASTConsumer {
        CompilerInstance &Instance;

    public:
        MyConsumer(CompilerInstance &Instance) : Instance(Instance) {}

        void HandleTranslationUnit(ASTContext& context) override {
            MyVisitor v(Instance);
            v.TraverseDecl(context.getTranslationUnitDecl());
        }
    };

    class MyPluginAction : public PluginASTAction {
    protected:
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                       llvm::StringRef) override {
            return std::make_unique<MyConsumer>(CI);
        }

        bool ParseArgs(const CompilerInstance &CI,
                       const std::vector<std::string> &args) override {
            for (unsigned i = 0, e = args.size(); i != e; ++i) {
                llvm::errs() << "arg " << i << " :" << args[i] << "\n";
            }
            return true;
        }

        // Automatically run the plugin after the main AST action
        PluginASTAction::ActionType getActionType() override {
            return AddAfterMainAction;
        }

    };
}

static FrontendPluginRegistry::Add<MyPluginAction>
        X("myplugin", "my plugin");
```

test.sh:

```shell
#!/bin/bash

set -e

LLVM_PATH=${LLVM_PATH:-"/home/hzy/hzydata/softwares/llvm/clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04"}
LLVM_BIN_PATH=${LLVM_PATH}/bin
LLVM_LIB_PATH=${LLVM_PATH}/lib
CLANG=${LLVM_BIN_PATH}/clang++

echo "LLVM_PATH:${LLVM_PATH}"
echo "LLVM_BIN_PATH:${LLVM_BIN_PATH}"
echo "LLVM_LIB_PATH:${LLVM_LIB_PATH}"
echo "CLANG:${CLANG}"

export LD_LIBRARY_PATH=${LLVM_LIB_PATH}

echo "ast dump ========== ========== ========== ========== =========="
${CLANG} -Xclang -ast-dump -fsyntax-only $1

echo "myplugin ========== ========== ========== ========== =========="
${CLANG} -fplugin=../cmake-build-release/libmyplugin.so -fplugin-arg-myplugin-hello -fplugin-arg-myplugin-world -c -o $1.o $1
```

Change `LLVM_PATH` to your own llvm path.