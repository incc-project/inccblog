示例程序:

```c++
template<typename T> class A {
public:
  T test(T x) { return x; }
};

int main() {
  A<int> a;
  return a.test(1);
}
```

# 解析流程

## 调试记录

分析入口:

clang/lib/Parse/ParseAST.cpp

```c++
    for (bool AtEOF = P.ParseFirstTopLevelDecl(ADecl, ImportState); !AtEOF;
         AtEOF = P.ParseTopLevelDecl(ADecl, ImportState)) {
      // If we got a null return and something *was* parsed, ignore it.  This
      // is due to a top-level semicolon, an action override, or a parse error
      // skipping something.
      if (ADecl && !Consumer->HandleTopLevelDecl(ADecl.get()))
        return;
    }
```

ParseFirstTopLevelDecl -> ParseTopLevelDecl

```c++
bool Parser::ParseFirstTopLevelDecl(DeclGroupPtrTy &Result,
                                    Sema::ModuleImportState &ImportState) {
  Actions.ActOnStartOfTranslationUnit();

  // For C++20 modules, a module decl must be the first in the TU.  We also
  // need to track module imports.
  ImportState = Sema::ModuleImportState::FirstDecl;
  bool NoTopLevelDecls = ParseTopLevelDecl(Result, ImportState);
  ...
```

ParseTopLevelDecl -> ParseExternalDeclaration

```c++
bool Parser::ParseTopLevelDecl(DeclGroupPtrTy &Result,
                               Sema::ModuleImportState &ImportState) {
  ...
  Result = ParseExternalDeclaration(DeclAttrs, DeclSpecAttrs);
  ...
```

ParseExternalDeclaration -> ParseDeclaration (走的`kw_template`分支)

```c++
Parser::DeclGroupPtrTy
Parser::ParseExternalDeclaration(ParsedAttributes &Attrs,
                                 ParsedAttributes &DeclSpecAttrs,
                                 ParsingDeclSpec *DS) {
  ...
  case tok::kw_using:
  case tok::kw_namespace:
  case tok::kw_typedef:
  case tok::kw_template:
  case tok::kw_static_assert:
  case tok::kw__Static_assert:
    // A function definition cannot start with any of these keywords.
    {
      SourceLocation DeclEnd;
      return ParseDeclaration(DeclaratorContext::File, DeclEnd, Attrs,
                              DeclSpecAttrs);
    }
  ...
```

ParseDeclaration -> ParseDeclarationStartingWithTemplate (走的`kw_template`分支)

```c++
Parser::DeclGroupPtrTy Parser::ParseDeclaration(DeclaratorContext Context,
                                                SourceLocation &DeclEnd,
                                                ParsedAttributes &DeclAttrs,
                                                ParsedAttributes &DeclSpecAttrs,
                                                SourceLocation *DeclSpecStart) {
  ...
  Decl *SingleDecl = nullptr;
  switch (Tok.getKind()) {
  case tok::kw_template:
  case tok::kw_export:
    ProhibitAttributes(DeclAttrs);
    ProhibitAttributes(DeclSpecAttrs);
    SingleDecl =
        ParseDeclarationStartingWithTemplate(Context, DeclEnd, DeclAttrs);
    break;
```

并不是显式实例化, ParseDeclarationStartingWithTemplate -> ParseTemplateDeclarationOrSpecialization

```c++
Decl *Parser::ParseDeclarationStartingWithTemplate(
    DeclaratorContext Context, SourceLocation &DeclEnd,
    ParsedAttributes &AccessAttrs, AccessSpecifier AS) {
  ObjCDeclContextSwitch ObjCDC(*this);

  if (Tok.is(tok::kw_template) && NextToken().isNot(tok::less)) {
    return ParseExplicitInstantiation(Context, SourceLocation(), ConsumeToken(),
                                      DeclEnd, AccessAttrs, AS);
  }
  return ParseTemplateDeclarationOrSpecialization(Context, DeclEnd, AccessAttrs,
                                                  AS);
}
```

整体是一个do while循环, 解析嵌套的template, 如:

```c++
template<typename T>
  template<typename U>
    class A<T>::B { ... };
```

```c++
  do {
	...
  } while (Tok.isOneOf(tok::kw_export, tok::kw_template));
```

读`template`

```c++
Decl *Parser::ParseTemplateDeclarationOrSpecialization(
    DeclaratorContext Context, SourceLocation &DeclEnd,
    ParsedAttributes &AccessAttrs, AccessSpecifier AS) {
    ...
  do {
    // Consume the 'export', if any.
    SourceLocation ExportLoc;
    TryConsumeToken(tok::kw_export, ExportLoc);

    // Consume the 'template', which should be here.
    SourceLocation TemplateLoc;
    if (!TryConsumeToken(tok::kw_template, TemplateLoc)) {
      Diag(Tok.getLocation(), diag::err_expected_template);
      return nullptr;
    }
```

读`<...>`模板参数列表

```c++
    // Parse the '<' template-parameter-list '>'
    SourceLocation LAngleLoc, RAngleLoc;
    SmallVector<NamedDecl*, 4> TemplateParams;
    if (ParseTemplateParameters(TemplateParamScopes,
                                CurTemplateDepthTracker.getDepth(),
                                TemplateParams, LAngleLoc, RAngleLoc)) {
      // Skip until the semi-colon or a '}'.
      SkipUntil(tok::r_brace, StopAtSemi | StopBeforeMatch);
      TryConsumeToken(tok::semi);
      return nullptr;
    }
```

CurTemplateDepthTracker记录了当前模板的深度, 举个例子:

```c++
template<typename T> // depth = 0
  template<typename U> // depth = 1
    class A<T>::B { ... };
```

消费`<`

```c++
bool Parser::ParseTemplateParameters(
    MultiParseScope &TemplateScopes, unsigned Depth,
    SmallVectorImpl<NamedDecl *> &TemplateParams, SourceLocation &LAngleLoc,
    SourceLocation &RAngleLoc) {
  // Get the template parameter list.
  if (!TryConsumeToken(tok::less, LAngleLoc)) {
    Diag(Tok.getLocation(), diag::err_expected_less_after) << "template";
    return true;
  }
```

处理空参数列表, 解析模板参数, ParseTemplateParameters -> ParseTemplateParameterList

```c++
  // Try to parse the template parameter list.
  bool Failed = false;
  // FIXME: Missing greatergreatergreater support.
  if (!Tok.is(tok::greater) && !Tok.is(tok::greatergreater)) {
    TemplateScopes.Enter(Scope::TemplateParamScope);
    Failed = ParseTemplateParameterList(Depth, TemplateParams);
  }
```

整体是一个while循环, 不断循环解析各个参数

```c++
/// ParseTemplateParameterList - Parse a template parameter list. If
/// the parsing fails badly (i.e., closing bracket was left out), this
/// will try to put the token stream in a reasonable position (closing
/// a statement, etc.) and return false.
///
///       template-parameter-list:    [C++ temp]
///         template-parameter
///         template-parameter-list ',' template-parameter
bool
Parser::ParseTemplateParameterList(const unsigned Depth,
                             SmallVectorImpl<NamedDecl*> &TemplateParams) {
  while (true) {
    ...
  }
  return true;
}
```

解析一个参数, ParseTemplateParameterList -> ParseTemplateParameter, 解析成功加入`TemplateParams`:

```c++
  while (true) {

    if (NamedDecl *TmpParam
          = ParseTemplateParameter(Depth, TemplateParams.size())) {
      TemplateParams.push_back(TmpParam);
    } else {
      // If we failed to parse a template parameter, skip until we find
      // a comma or closing brace.
      SkipUntil(tok::comma, tok::greater, tok::greatergreater,
                StopAtSemi | StopBeforeMatch);
    }
```

看一下template-parameter的产生式:

```c++
/// ParseTemplateParameter - Parse a template-parameter (C++ [temp.param]).
///
///       template-parameter: [C++ temp.param]
///         type-parameter
///         parameter-declaration
///
///       type-parameter: (See below)
///         type-parameter-key ...[opt] identifier[opt]
///         type-parameter-key identifier[opt] = type-id
/// (C++2a) type-constraint ...[opt] identifier[opt]
/// (C++2a) type-constraint identifier[opt] = type-id
///         'template' '<' template-parameter-list '>' type-parameter-key
///               ...[opt] identifier[opt]
///         'template' '<' template-parameter-list '>' type-parameter-key
///               identifier[opt] '=' id-expression
///
///       type-parameter-key:
///         class
///         typename
///
NamedDecl *Parser::ParseTemplateParameter(unsigned Depth, unsigned Position) {
```

从语法来看`template<>`中是可以继续嵌套`template<>`, 因此整体是一个递归结构:

```c++
NamedDecl *Parser::ParseTemplateParameter(unsigned Depth, unsigned Position) {
  switch (isStartOfTemplateTypeParameter()) {
  ...
  }
  
  if (Tok.is(tok::kw_template))
    return ParseTemplateTemplateParameter(Depth, Position);

  // If it's none of the above, then it must be a parameter declaration.
  // NOTE: This will pick up errors in the closure of the template parameter
  // list (e.g., template < ; Check here to implement >> style closures.
  return ParseNonTypeTemplateParameter(Depth, Position);
}
```

注意模板参数可以为类型参数, 也可以为非类型参数. 比如`template<typename T, int N>`, 前者为类型参数, 后者为非类型参数.

ParseTemplateParameter一开始的isStartOfTemplateTypeParameter会判断当前模板是否为类型参数, 如果不是, 则进一步判断是否是`template`嵌套, 如果还不是, 则通过`ParseNonTypeTemplateParameter`处理非类型参数.

这个例子中, 我们即将处理的参数为类型参数, 来看看isStartOfTemplateTypeParameter的判断逻辑:

```c++
Parser::TPResult Parser::isStartOfTemplateTypeParameter() {
  if (Tok.is(tok::kw_class)) {
  ...
  }
  ...
  // 'typedef' is a reasonably-common typo/thinko for 'typename', and is
  // ill-formed otherwise.
  if (Tok.isNot(tok::kw_typename) && Tok.isNot(tok::kw_typedef))
    return TPResult::False;
      
  // C++ [temp.param]p2:
  //   There is no semantic difference between class and typename in a
  //   template-parameter. typename followed by an unqualified-id
  //   names a template type parameter. typename followed by a
  //   qualified-id denotes the type in a non-type
  //   parameter-declaration.
  Token Next = NextToken();

  // If we have an identifier, skip over it.
  if (Next.getKind() == tok::identifier)
    Next = GetLookAheadToken(2);
```

这是首先利用if分支确定当前token是`typename`或`typedef` (在这个例子中是`typename`), 接着使用NextToken前看一个符号得到identifier `T`, 然后再前看一个符号得到`>`

接着会走`tok::greater`分支, 最终返回true:

```c++
  switch (Next.getKind()) {
  case tok::equal:
  case tok::comma:
  case tok::greater:
  case tok::greatergreater:
  case tok::ellipsis:
    return TPResult::True;

  case tok::kw_typename:
  case tok::kw_typedef:
  case tok::kw_class:
    // These indicate that a comma was missed after a type parameter, not that
    // we have found a non-type parameter.
    return TPResult::True;

  default:
    return TPResult::False;
  }
```

这里前看token是为了消除什么二义性呢? 其实注释已经说得很清楚了:

```c++
// typename followed by an unqualified-id
// names a template type parameter. typename followed by a
// qualified-id denotes the type in a non-type
// parameter-declaration.
```

由于typename+qualified id表示非类型参数, 所以要通过前看符号进行二义性消除.

回到ParseTemplateParameter, 我们会走TPResult::True分支, 并调用ParseTypeParameter:

```c++
NamedDecl *Parser::ParseTemplateParameter(unsigned Depth, unsigned Position) {

  switch (isStartOfTemplateTypeParameter()) {
  case TPResult::True:
    // Is there just a typo in the input code? ('typedef' instead of
    // 'typename')
    if (Tok.is(tok::kw_typedef)) {
      Diag(Tok.getLocation(), diag::err_expected_template_parameter);

      Diag(Tok.getLocation(), diag::note_meant_to_use_typename)
          << FixItHint::CreateReplacement(CharSourceRange::getCharRange(
                                              Tok.getLocation(),
                                              Tok.getEndLoc()),
                                          "typename");

      Tok.setKind(tok::kw_typename);
    }

    return ParseTypeParameter(Depth, Position);
```

先看下type parameter的产生式:

```c++
/// ParseTypeParameter - Parse a template type parameter (C++ [temp.param]).
/// Other kinds of template parameters are parsed in
/// ParseTemplateTemplateParameter and ParseNonTypeTemplateParameter.
///
///       type-parameter:     [C++ temp.param]
///         'class' ...[opt][C++0x] identifier[opt]
///         'class' identifier[opt] '=' type-id
///         'typename' ...[opt][C++0x] identifier[opt]
///         'typename' identifier[opt] '=' type-id
NamedDecl *Parser::ParseTypeParameter(unsigned Depth, unsigned Position) {
```

首先消费`typename`:

```c++
  if (Tok.is(tok::annot_template_id)) {
   ...
  } else {
    assert(TypeConstraintSS.isEmpty() &&
           "expected type constraint after scope specifier");

    // Consume the 'class' or 'typename' keyword.
    TypenameKeyword = Tok.is(tok::kw_typename);
    KeyLoc = ConsumeToken();
  }
```

这里我们并没有用`...`, 所以这个分支不会执行:

```c++
  // Grab the ellipsis (if given).
  SourceLocation EllipsisLoc;
  if (TryConsumeToken(tok::ellipsis, EllipsisLoc)) {
    Diag(EllipsisLoc,
         getLangOpts().CPlusPlus11
           ? diag::warn_cxx98_compat_variadic_templates
           : diag::ext_variadic_templates);
  }
```

消费接下来的identifier `T`:

```c++
  // Grab the template parameter name (if given)
  SourceLocation NameLoc = Tok.getLocation();
  IdentifierInfo *ParamName = nullptr;
  if (Tok.is(tok::identifier)) {
    ParamName = Tok.getIdentifierInfo();
    ConsumeToken();
  } else if (Tok.isOneOf(tok::equal, tok::comma, tok::greater,
                         tok::greatergreater)) {
    // Unnamed template parameter. Don't have to do anything here, just
    // don't consume this token.
  } else {
    Diag(Tok.getLocation(), diag::err_expected) << tok::identifier;
    return nullptr;
  }
```

用gdb验证一下:

```shell
(gdb) print ParamName->getName().str()
$1 = "T"
```

这里我们也没有写T=xxx这种默认参数形式, 所以这个分支也不会执行:

```c++
  // Grab a default argument (if available).
  // Per C++0x [basic.scope.pdecl]p9, we parse the default argument before
  // we introduce the type parameter into the local scope.
  SourceLocation EqualLoc;
  ParsedType DefaultArg;
  if (TryConsumeToken(tok::equal, EqualLoc))
    DefaultArg =
        ParseTypeName(/*Range=*/nullptr, DeclaratorContext::TemplateTypeArg)
            .get();
```

现在我们就可以为`T`构建NamedDecl了:

```c++
  NamedDecl *NewDecl = Actions.ActOnTypeParameter(getCurScope(),
                                                  TypenameKeyword, EllipsisLoc,
                                                  KeyLoc, ParamName, NameLoc,
                                                  Depth, Position, EqualLoc,
                                                  DefaultArg,
                                                  TypeConstraint != nullptr);

  if (TypeConstraint) {
    Actions.ActOnTypeConstraint(TypeConstraintSS, TypeConstraint,
                                cast<TemplateTypeParmDecl>(NewDecl),
                                EllipsisLoc);
  }

  return NewDecl;
```

回到ParseTemplateParameterList, 我们已经解析完了template中的全部参数, 此时来到`tok::greater`这个分支, break出while循环:

```c++
// Did we find a comma or the end of the template parameter list?
if (Tok.is(tok::comma)) {
  ConsumeToken();
} else if (Tok.isOneOf(tok::greater, tok::greatergreater)) {
  // Don't consume this... that's done by template parser.
  break;
} else {
  // Somebody probably forgot to close the template. Skip ahead and
  // try to get out of the expression. This error is currently
  // subsumed by whatever goes on in ParseTemplateParameter.
  Diag(Tok.getLocation(), diag::err_expected_comma_greater);
  SkipUntil(tok::comma, tok::greater, tok::greatergreater,
            StopAtSemi | StopBeforeMatch);
  return false;
}
}
return true;
```

回到ParseTemplateParameters, 消费掉`>`:

```c++
if (Tok.is(tok::greatergreater)) {
  // No diagnostic required here: a template-parameter-list can only be
  // followed by a declaration or, for a template template parameter, the
  // 'class' keyword. Therefore, the second '>' will be diagnosed later.
  // This matters for elegant diagnosis of:
  //   template<template<typename>> struct S;
  Tok.setKind(tok::greater);
  RAngleLoc = Tok.getLocation();
  Tok.setLocation(Tok.getLocation().getLocWithOffset(1));
} else if (!TryConsumeToken(tok::greater, RAngleLoc) && Failed) {
  Diag(Tok.getLocation(), diag::err_expected) << tok::greater;
  return true;
}
return false;
```

回到ParseTemplateDeclarationOrSpecialization, 更新CurTemplateDepthTracker, 将刚刚解析出的参数加入ParamLists:

```c++
    ExprResult OptionalRequiresClauseConstraintER;
    if (!TemplateParams.empty()) {
      isSpecialization = false;
      ++CurTemplateDepthTracker;

      ...
    } else {
      LastParamListWasEmpty = true;
    }

    ParamLists.push_back(Actions.ActOnTemplateParameterList(
        CurTemplateDepthTracker.getDepth(), ExportLoc, TemplateLoc, LAngleLoc,
        TemplateParams, RAngleLoc, OptionalRequiresClauseConstraintER.get()));
```

此外, 由于没有template递归, 退出while循环:

```c++
while (Tok.isOneOf(tok::kw_export, tok::kw_template));
```

接着, 通过ParseTemplateDeclarationOrSpecialization -> ParseSingleDeclarationAfterTemplate解析template后的decl:

```c++
  return ParseSingleDeclarationAfterTemplate(
      Context,
      ParsedTemplateInfo(&ParamLists, isSpecialization, LastParamListWasEmpty),
      ParsingTemplateParams, DeclEnd, AccessAttrs, AS);
```

先看下ParseSingleDeclarationAfterTemplate的注释:

```c++
/// Parse a single declaration that declares a template,
/// template specialization, or explicit instantiation of a template.
///
/// \param DeclEnd will receive the source location of the last token
/// within this declaration.
///
/// \param AS the access specifier associated with this
/// declaration. Will be AS_none for namespace-scope declarations.
///
/// \returns the new declaration.
Decl *Parser::ParseSingleDeclarationAfterTemplate(
    DeclaratorContext Context, const ParsedTemplateInfo &TemplateInfo,
    ParsingDeclRAIIObject &DiagsFromTParams, SourceLocation &DeclEnd,
    ParsedAttributes &AccessAttrs, AccessSpecifier AS) {
```

由于我们的template并非作为其他template class的member, 因此以下分支不执行:

```c++
  if (Context == DeclaratorContext::Member) {
    // We are parsing a member template.
    DeclGroupPtrTy D = ParseCXXClassMemberDeclaration(
        AS, AccessAttrs, TemplateInfo, &DiagsFromTParams);

    if (!D || !D.get().isSingleDecl())
      return nullptr;
    return D.get().getSingleDecl();
  }
```

由于我们的template并非是别名模板, 因此以下分支不执行:

```c++
  if (Tok.is(tok::kw_using)) {
    auto usingDeclPtr = ParseUsingDirectiveOrDeclaration(Context, TemplateInfo, DeclEnd,
                                                         prefixAttrs);
    if (!usingDeclPtr || !usingDeclPtr.get().isSingleDecl())
      return nullptr;
    return usingDeclPtr.get().getSingleDecl();
  }
```

接下来通过ParseDeclarationSpecifiers解析声明的specifier(说明符):

```c++
  ParseDeclarationSpecifiers(DS, TemplateInfo, AS,
                             getDeclSpecContextFromDeclaratorContext(Context));
```

ParseDeclarationSpecifiers的注释如下:

```c++
/// ParseDeclarationSpecifiers
///       declaration-specifiers: [C99 6.7]
///         storage-class-specifier declaration-specifiers[opt]
///         type-specifier declaration-specifiers[opt]
/// [C99]   function-specifier declaration-specifiers[opt]
/// [C11]   alignment-specifier declaration-specifiers[opt]
/// [GNU]   attributes declaration-specifiers[opt]
/// [Clang] '__module_private__' declaration-specifiers[opt]
/// [ObjC1] '__kindof' declaration-specifiers[opt]
///
///       storage-class-specifier: [C99 6.7.1]
///         'typedef'
///         'extern'
///         'static'
///         'auto'
///         'register'
/// [C++]   'mutable'
/// [C++11] 'thread_local'
/// [C11]   '_Thread_local'
/// [GNU]   '__thread'
///       function-specifier: [C99 6.7.4]
/// [C99]   'inline'
/// [C++]   'virtual'
/// [C++]   'explicit'
/// [OpenCL] '__kernel'
///       'friend': [C++ dcl.friend]
///       'constexpr': [C++0x dcl.constexpr]
void Parser::ParseDeclarationSpecifiers(
    DeclSpec &DS, const ParsedTemplateInfo &TemplateInfo, AccessSpecifier AS,
    DeclSpecContext DSContext, LateParsedAttrList *LateAttrs,
    ImplicitTypenameContext AllowImplicitTypename) {
```

这里我们要解析的是type-specifier, 代表整个类本身.

ParseDeclarationSpecifiers中有个十分复杂的while循环:

```c++
  while (true) {
  ...
  switch (Tok.getKind()){1000+lines}
  ...
  }
```

这里我们只关注和例程相关的部分, 走到class分支:

```c++
    // class-specifier:
    case tok::kw_class:
    case tok::kw_struct:
    case tok::kw___interface:
    case tok::kw_union: {
      tok::TokenKind Kind = Tok.getKind();
      ConsumeToken();

      // These are attributes following class specifiers.
      // To produce better diagnostic, we parse them when
      // parsing class specifier.
      ParsedAttributes Attributes(AttrFactory);
      ParseClassSpecifier(Kind, Loc, DS, TemplateInfo, AS,
                          EnteringContext, DSContext, Attributes);

      // If there are attributes following class specifier,
      // take them over and handle them here.
      if (!Attributes.empty()) {
        AttrsLastTime = true;
        attrs.takeAllFrom(Attributes);
      }
      continue;
    }
```

消费`class`并调用ParseClassSpecifier进行进一步的处理(正常的class的前向声明或定义都会通过ParseClassSpecifier进行解析).

首先识别tag的类型, 这里是class:

```c++
void Parser::ParseClassSpecifier(tok::TokenKind TagTokKind,
                                 SourceLocation StartLoc, DeclSpec &DS,
                                 const ParsedTemplateInfo &TemplateInfo,
                                 AccessSpecifier AS, bool EnteringContext,
                                 DeclSpecContext DSC,
                                 ParsedAttributes &Attributes) {
  DeclSpec::TST TagType;
  if (TagTokKind == tok::kw_struct)
    TagType = DeclSpec::TST_struct;
  else if (TagTokKind == tok::kw___interface)
    TagType = DeclSpec::TST_interface;
  else if (TagTokKind == tok::kw_class)
    TagType = DeclSpec::TST_class;
  else {
    assert(TagTokKind == tok::kw_union && "Not a class specifier");
    TagType = DeclSpec::TST_union;
  }
```

接着是一堆特殊情况处理, 我们这里略过, 关注class解析的核心部分:

```c++
  // Parse the (optional) class name or simple-template-id.
  IdentifierInfo *Name = nullptr;
  SourceLocation NameLoc;
  TemplateIdAnnotation *TemplateId = nullptr;
  if (Tok.is(tok::identifier)) {
    Name = Tok.getIdentifierInfo();
    NameLoc = ConsumeToken();

    if (Tok.is(tok::less) && getLangOpts().CPlusPlus) {
      // The name was supposed to refer to a template, but didn't.
      // Eat the template argument list and try to continue parsing this as
      // a class (or template thereof).
      TemplateArgList TemplateArgs;
      SourceLocation LAngleLoc, RAngleLoc;
      if (ParseTemplateIdAfterTemplateName(true, LAngleLoc, TemplateArgs,
                                           RAngleLoc)) {
        // We couldn't parse the template argument list at all, so don't
        // try to give any location information for the list.
        LAngleLoc = RAngleLoc = SourceLocation();
      }
      RecoverFromUndeclaredTemplateName(
          Name, NameLoc, SourceRange(LAngleLoc, RAngleLoc), false);
    }
  }
```

这里我们会解析到类名`A`, 由于类名后面没有`<` , 所以下面的分支不会执行.

这里通过解析到`{`判断出这是一个类的定义, 而非前向声明(注意这里也会通过前看token来判断有冒号继承的情况):

```c++
  else if (Tok.is(tok::l_brace) ||
           (DSC != DeclSpecContext::DSC_association &&
            getLangOpts().CPlusPlus && Tok.is(tok::colon)) ||
           (isClassCompatibleKeyword() &&
            (NextToken().is(tok::l_brace) || NextToken().is(tok::colon)))) {
    if (DS.isFriendSpecified()) {
      // C++ [class.friend]p2:
      //   A class shall not be defined in a friend declaration.
      Diag(Tok.getLocation(), diag::err_friend_decl_defines_type)
          << SourceRange(DS.getFriendSpecLoc());

      // Skip everything up to the semicolon, so that this looks like a proper
      // friend class (or template thereof) declaration.
      SkipUntil(tok::semi, StopBeforeMatch);
      TUK = Sema::TUK_Friend;
    } else {
      // Okay, this is a class definition.
      TUK = Sema::TUK_Definition;
    }
```

接下来解析类的主体, ParserClassSpecifier -> ParseCXXMemberSpecification:

```c++
  // If there is a body, parse it and inform the actions module.
  if (TUK == Sema::TUK_Definition) {
    assert(Tok.is(tok::l_brace) ||
           (getLangOpts().CPlusPlus && Tok.is(tok::colon)) ||
           isClassCompatibleKeyword());
    if (SkipBody.ShouldSkip)
      SkipCXXMemberSpecification(StartLoc, AttrFixitLoc, TagType,
                                 TagOrTempResult.get());
    else if (getLangOpts().CPlusPlus)
      ParseCXXMemberSpecification(StartLoc, AttrFixitLoc, attrs, TagType,
                                  TagOrTempResult.get());
    else {
      Decl *D =
          SkipBody.CheckSameAsPrevious ? SkipBody.New : TagOrTempResult.get();
      // Parse the definition body.
      ParseStructUnionBody(StartLoc, TagType, cast<RecordDecl>(D));
      if (SkipBody.CheckSameAsPrevious &&
          !Actions.ActOnDuplicateDefinition(TagOrTempResult.get(), SkipBody)) {
        DS.SetTypeSpecError();
        return;
      }
    }
  }
```

先看一下member-specification的产生式:

```c++
/// ParseCXXMemberSpecification - Parse the class definition.
///
///       member-specification:
///         member-declaration member-specification[opt]
///         access-specifier ':' member-specification[opt]
///
void Parser::ParseCXXMemberSpecification(SourceLocation RecordLoc,
                                         SourceLocation AttrFixitLoc,
                                         ParsedAttributes &Attrs,
                                         unsigned TagType, Decl *TagDecl) {
```

这里属于ParserClass的TimeScope:

```c++
  llvm::TimeTraceScope TimeScope("ParseClass", [&]() {
    if (auto *TD = dyn_cast_or_null<NamedDecl>(TagDecl))
      return TD->getQualifiedNameAsString();
    return std::string("<anonymous>");
  });
```

例程没有final, 所以这个分支可以直接跳过:

```c++
  // Parse the optional 'final' keyword.
  if (getLangOpts().CPlusPlus && Tok.is(tok::identifier)) {
    while (true) {
        ...
```

例程没有继承, 所以这个分支也可以直接跳过:

```c++
  if (Tok.is(tok::colon)) {
    ParseScope InheritanceScope(this, getCurScope()->getFlags() |
                                          Scope::ClassInheritanceScope);

    ParseBaseClause(TagDecl);
    if (!Tok.is(tok::l_brace)) {
```

消费`{`:

```c++
  assert(Tok.is(tok::l_brace));
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  if (TagDecl)
    Actions.ActOnStartCXXMemberDeclarations(getCurScope(), TagDecl, FinalLoc,
                                            IsFinalSpelledSealed, IsAbstract,
                                            T.getOpenLocation());
```

通过判断是class还是struct来决定默认的访问性:

```c++
  // C++ 11p3: Members of a class defined with the keyword class are private
  // by default. Members of a class defined with the keywords struct or union
  // are public by default.
  // HLSL: In HLSL members of a class are public by default.
  AccessSpecifier CurAS;
  if (TagType == DeclSpec::TST_class && !getLangOpts().HLSL)
    CurAS = AS_private;
  else
    CurAS = AS_public;
```

不断解析member, ParseCXXMemberSpecification -> ParseCXXClassMemberDeclarationWithPragmas:

```c++
  if (TagDecl) {
    // While we still have something to read, read the member-declarations.
    while (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
           Tok.isNot(tok::eof)) {
      // Each iteration of this loop reads one member-declaration.
      ParseCXXClassMemberDeclarationWithPragmas(
          CurAS, AccessAttrs, static_cast<DeclSpec::TST>(TagType), TagDecl);
      MaybeDestroyTemplateIds();
    }
    T.consumeClose();
  } else {
    SkipUntil(tok::r_brace);
  }
```

解析member时, 首先解析访问控制符, 这里是`public`:

```c++
Parser::DeclGroupPtrTy Parser::ParseCXXClassMemberDeclarationWithPragmas(
    AccessSpecifier &AS, ParsedAttributes &AccessAttrs, DeclSpec::TST TagType,
    Decl *TagDecl) {
  ParenBraceBracketBalancer BalancerRAIIObj(*this);

  switch (Tok.getKind()) {
  case tok::kw___if_exists:
  case tok::kw___if_not_exists:
    ParseMicrosoftIfExistsClassDeclaration(TagType, AccessAttrs, AS);
    return nullptr;

  case tok::semi:
    // Check for extraneous top-level semicolon.
    ConsumeExtraSemi(InsideStruct, TagType);
    return nullptr;
    ...
    case tok::kw_public:
  	case tok::kw_protected: {
  	  if (getLangOpts().HLSL)
  	    Diag(Tok.getLocation(), diag::ext_hlsl_access_specifiers);
  	  AccessSpecifier NewAS = getAccessSpecifierIfPresent();
  	  assert(NewAS != AS_none);
  	  // Current token is a C++ access specifier.
  	  AS = NewAS;
  	  SourceLocation ASLoc = Tok.getLocation();
  	  unsigned TokLength = Tok.getLength();
  	  ConsumeToken();
  	  AccessAttrs.clear();
  	  MaybeParseGNUAttributes(AccessAttrs);
	
  	  SourceLocation EndLoc;
  	  if (TryConsumeToken(tok::colon, EndLoc)) {
  	  } else if (TryConsumeToken(tok::semi, EndLoc)) {
  	    Diag(EndLoc, diag::err_expected)
  	        << tok::colon << FixItHint::CreateReplacement(EndLoc, ":");
  	  } else {
  	    EndLoc = ASLoc.getLocWithOffset(TokLength);
  	    Diag(EndLoc, diag::err_expected)
  	        << tok::colon << FixItHint::CreateInsertion(EndLoc, ":");
  	  }
	
  	  // The Microsoft extension __interface does not permit non-public
  	  // access specifiers.
  	  if (TagType == DeclSpec::TST_interface && AS != AS_public) {
  	    Diag(ASLoc, diag::err_access_specifier_interface) << (AS == AS_protected);
  	  }
	
  	  if (Actions.ActOnAccessSpecifier(NewAS, ASLoc, EndLoc, AccessAttrs)) {
  	    // found another attribute than only annotations
  	    AccessAttrs.clear();
  	  }
	
  	  return nullptr;
  	}
  	...
```

return nullptr后, 我们再次来到ParseCXXMemberSpecification的while循环, 第二次调用ParseCXXClassMemberDeclarationWithPragmas, 此时走的分支是default, 接下来我们会解析member decl:

```c++
  default:
    if (tok::isPragmaAnnotation(Tok.getKind())) {
      Diag(Tok.getLocation(), diag::err_pragma_misplaced_in_decl)
          << DeclSpec::getSpecifierName(
                 TagType, Actions.getASTContext().getPrintingPolicy());
      ConsumeAnnotationToken();
      return nullptr;
    }
    return ParseCXXClassMemberDeclaration(AS, AccessAttrs);
  }
```

看一下member-declaration的产生式:

```c++
/// ParseCXXClassMemberDeclaration - Parse a C++ class member declaration.
///
///       member-declaration:
///         decl-specifier-seq[opt] member-declarator-list[opt] ';'
///         function-definition ';'[opt]
///         ::[opt] nested-name-specifier template[opt] unqualified-id ';'[TODO]
///         using-declaration                                            [TODO]
/// [C++0x] static_assert-declaration
///         template-declaration
/// [GNU]   '__extension__' member-declaration
///
///       member-declarator-list:
///         member-declarator
///         member-declarator-list ',' member-declarator
///
///       member-declarator:
///         declarator virt-specifier-seq[opt] pure-specifier[opt]
/// [C++2a] declarator requires-clause
///         declarator constant-initializer[opt]
/// [C++11] declarator brace-or-equal-initializer[opt]
///         identifier[opt] ':' constant-expression
///
///       virt-specifier-seq:
///         virt-specifier
///         virt-specifier-seq virt-specifier
///
///       virt-specifier:
///         override
///         final
/// [MS]    sealed
///
///       pure-specifier:
///         '= 0'
///
///       constant-initializer:
///         '=' constant-expression
///
Parser::DeclGroupPtrTy
Parser::ParseCXXClassMemberDeclaration(AccessSpecifier AS,
                                       ParsedAttributes &AccessAttrs,
                                       const ParsedTemplateInfo &TemplateInfo,
                                       ParsingDeclRAIIObject *TemplateDiags) {
```

开始解析声明:

```c++
if (!TemplateInfo.Kind &&
      Tok.isOneOf(tok::identifier, tok::coloncolon, tok::kw___super)) {
```

接下来也是各种分支判断, 这里我们关注函数产生式.

首先是函数的specifier解析(函数名前的内容, 包括修饰符, 返回类型), 注意嵌套的子类也是在这里解析的:

```c++
  ParseDeclarationSpecifiers(DS, TemplateInfo, AS, DeclSpecContext::DSC_class,
                             &CommonLateParsedAttrs);
```

又来到了ParseDeclarationSpecifiers的while switch结构, 这次走的是identifier分支:

```c++
 case tok::identifier:
    ParseIdentifier: {
      // This identifier can only be a typedef name if we haven't already seen
      // a type-specifier.  Without this check we misparse:
      //  typedef int X; struct Y { short X; };  as 'short int'.
      if (DS.hasTypeSpecifier())
        goto DoneWithDeclSpec;
        ...
```

检查是否是scope specifier, 这里并没有用类似`foo::bar::`之类的前缀进行修饰:

```c++
// In C++, check to see if this is a scope specifier like foo::bar::, if
      // so handle it as such.  This is important for ctor parsing.
      if (getLangOpts().CPlusPlus) {
        // C++20 [temp.spec] 13.9/6.
        // This disables the access checking rules for function template
        // explicit instantiation and explicit specialization:
        // - `return type`.
        SuppressAccessChecks SAC(*this, IsTemplateSpecOrInst);

        const bool Success = TryAnnotateCXXScopeToken(EnteringContext);
```

根据当前token, 解析成相应的返回类型:

```c++
ParsedType TypeRep = Actions.getTypeName(
          *Tok.getIdentifierInfo(), Tok.getLocation(), getCurScope(), nullptr,
          false, false, nullptr, false, false,
          isClassTemplateDeductionContext(DSContext));
```

消费掉`T`:

```c++
      if (isInvalid)
        break;

      DS.SetRangeEnd(Tok.getLocation());
      ConsumeToken(); // The identifier
```

检验一下, TypeRep确实是我们预期的`T`:

```shell
(gdb) print ((QualType*)TypeRep.get())->dump()

TemplateTypeParmType 0x5555556ac400 'T' dependent depth 0 index 0
`-TemplateTypeParm 0x5555556ac380 'T
```

继续while循环:

```c++
      // Need to support trailing type qualifiers (e.g. "id<p> const").
      // If a type specifier follows, it will be diagnosed elsewhere.
      continue;
```

这次我们又来到identifier分支, 不过这次走的是DoneWithDeclSpec:

```c++
case tok::identifier:
ParseIdentifier: {
  // This identifier can only be a typedef name if we haven't already seen
  // a type-specifier.  Without this check we misparse:
  //  typedef int X; struct Y { short X; };  as 'short int'.
  if (DS.hasTypeSpecifier())
    goto DoneWithDeclSpec;
```

解释一下, 类似inline T const test这种specifier, clang会依次将前三个按照kw_inline, kw_identifier, kw_const分支进行处理, 最后解析test时, 走的也是kw_identifier分支, 那么怎么区分这是返回类型还是函数名呢? 这就用到了hasTypeSpecifier, 通过判断当前identifier前是否有type specifier来确定这是否是一个函数名.

```c++
    case tok::identifier:
    ParseIdentifier: {
      // This identifier can only be a typedef name if we haven't already seen
      // a type-specifier.  Without this check we misparse:
      //  typedef int X; struct Y { short X; };  as 'short int'.
      if (DS.hasTypeSpecifier())
        goto DoneWithDeclSpec;
```

当然, 函数名并不属于parseDeclarationSpecifiers的作用范围, 因此return走:

```c++
    switch (Tok.getKind()) {
    default:
    DoneWithDeclSpec:
      if (!AttrsLastTime)
        ProhibitAttributes(attrs);
      else {
        // Reject C++11 / C2x attributes that aren't type attributes.
        for (const ParsedAttr &PA : attrs) {
          if (!PA.isCXX11Attribute() && !PA.isC2xAttribute())
            continue;
          if (PA.getKind() == ParsedAttr::UnknownAttribute)
            // We will warn about the unknown attribute elsewhere (in
            // SemaDeclAttr.cpp)
            continue;
          // GCC ignores this attribute when placed on the DeclSpec in [[]]
          // syntax, so we do the same.
          if (PA.getKind() == ParsedAttr::AT_VectorSize) {
            Diag(PA.getLoc(), diag::warn_attribute_ignored) << PA;
            PA.setInvalid();
            continue;
          }
          // We reject AT_LifetimeBound and AT_AnyX86NoCfCheck, even though they
          // are type attributes, because we historically haven't allowed these
          // to be used as type attributes in C++11 / C2x syntax.
          if (PA.isTypeAttr() && PA.getKind() != ParsedAttr::AT_LifetimeBound &&
              PA.getKind() != ParsedAttr::AT_AnyX86NoCfCheck)
            continue;
          Diag(PA.getLoc(), diag::err_attribute_not_type_attr) << PA;
          PA.setInvalid();
        }

        DS.takeAttributesFrom(attrs);
      }

      // If this is not a declaration specifier token, we're done reading decl
      // specifiers.  First verify that DeclSpec's are consistent.
      DS.Finish(Actions, Policy);
      return;
```

回到ParseCXXClassMemberDeclaration, 解析函数头:

```c++
  // Parse the first declarator.
  if (ParseCXXMemberDeclaratorBeforeInitializer(
          DeclaratorInfo, VS, BitfieldSize, LateParsedAttrs)) {
    TryConsumeToken(tok::semi);
    return nullptr;
  }
```

相关注释如下. 意思是只解析到(不包括)可选的初始化部分(brace-or-equal-initializer)或纯虚函数的标识符(pure-specifier).

换句话说, 这个函数会解析成员声明符的核心部分, 例如函数的返回类型、名称和参数列表, 但不会处理函数体或纯虚函数的`= 0`这样的标识符.

在C++中, brace-or-equal-initializer 指的是类似`int x = 0;`中的`= 0`或构造函数的花括号初始化部分 `{ }`, 而纯虚函数的标识符就是纯虚函数声明中的`= 0`.

注意, 如果是`A():x(1){`这种有构造列表的的形式, 这里只会解析到`:`.

```c++
/// Parse a C++ member-declarator up to, but not including, the optional
/// brace-or-equal-initializer or pure-specifier.
bool Parser::ParseCXXMemberDeclaratorBeforeInitializer(
    Declarator &DeclaratorInfo, VirtSpecifiers &VS, ExprResult &BitfieldSize,
    LateParsedAttrList &LateParsedAttrs) {
  // member-declarator:
  //   declarator virt-specifier-seq[opt] pure-specifier[opt]
  //   declarator requires-clause
  //   declarator brace-or-equal-initializer[opt]
  //   identifier attribute-specifier-seq[opt] ':' constant-expression
  //       brace-or-equal-initializer[opt]
  //   ':' constant-expression
  //
  // NOTE: the latter two productions are a proposed bugfix rather than the
  // current grammar rules as of C++20.
  if (Tok.isNot(tok::colon))
    ParseDeclarator(DeclaratorInfo);
  else
    DeclaratorInfo.SetIdentifier(nullptr, Tok.getLocation());
```

ParseCXXMemberDeclaratorBeforeInitializer -> ParseDeclarator:

```c++
/// ParseDeclarator - Parse and verify a newly-initialized declarator.
void Parser::ParseDeclarator(Declarator &D) {
  /// This implements the 'declarator' production in the C grammar, then checks
  /// for well-formedness and issues diagnostics.
  Actions.runWithSufficientStackSpace(D.getBeginLoc(), [&] {
    ParseDeclaratorInternal(D, &Parser::ParseDirectDeclarator);
  });
}
```

ParseDeclarator -> ParseDeclaratorInternal:

```c++
/// ParseDeclaratorInternal - Parse a C or C++ declarator. The direct-declarator
/// is parsed by the function passed to it. Pass null, and the direct-declarator
/// isn't parsed at all, making this function effectively parse the C++
/// ptr-operator production.
///
/// If the grammar of this construct is extended, matching changes must also be
/// made to TryParseDeclarator and MightBeDeclarator, and possibly to
/// isConstructorDeclarator.
///
///       declarator: [C99 6.7.5] [C++ 8p4, dcl.decl]
/// [C]     pointer[opt] direct-declarator
/// [C++]   direct-declarator
/// [C++]   ptr-operator declarator
///
///       pointer: [C99 6.7.5]
///         '*' type-qualifier-list[opt]
///         '*' type-qualifier-list[opt] pointer
///
///       ptr-operator:
///         '*' cv-qualifier-seq[opt]
///         '&'
/// [C++0x] '&&'
/// [GNU]   '&' restrict[opt] attributes[opt]
/// [GNU?]  '&&' restrict[opt] attributes[opt]
///         '::'[opt] nested-name-specifier '*' cv-qualifier-seq[opt]
void Parser::ParseDeclaratorInternal(Declarator &D,
                                     DirectDeclParseFunction DirectDeclParser) {
```

这里会做一些指针引用判断什么的, 不过这里我们是普通函数, 所以走普通的directDeclParser流程:

```c++
  // Not a pointer, C++ reference, or block.
  if (!isPtrOperatorToken(Kind, getLangOpts(), D.getContext())) {
    if (DirectDeclParser)
      (this->*DirectDeclParser)(D);
    return;
  }
```

函数指针的来源是上层调用点, 传入了ParseDirectDeclarator:

```c++
/// ParseDeclarator - Parse and verify a newly-initialized declarator.
void Parser::ParseDeclarator(Declarator &D) {
  /// This implements the 'declarator' production in the C grammar, then checks
  /// for well-formedness and issues diagnostics.
  Actions.runWithSufficientStackSpace(D.getBeginLoc(), [&] {
    ParseDeclaratorInternal(D, &Parser::ParseDirectDeclarator);
  });
}
```

看一下ParseDirectDeclarator的产生式:

```c++
/// ParseDirectDeclarator
///       direct-declarator: [C99 6.7.5]
/// [C99]   identifier
///         '(' declarator ')'
/// [GNU]   '(' attributes declarator ')'
/// [C90]   direct-declarator '[' constant-expression[opt] ']'
/// [C99]   direct-declarator '[' type-qual-list[opt] assignment-expr[opt] ']'
/// [C99]   direct-declarator '[' 'static' type-qual-list[opt] assign-expr ']'
/// [C99]   direct-declarator '[' type-qual-list 'static' assignment-expr ']'
/// [C99]   direct-declarator '[' type-qual-list[opt] '*' ']'
/// [C++11] direct-declarator '[' constant-expression[opt] ']'
///                    attribute-specifier-seq[opt]
///         direct-declarator '(' parameter-type-list ')'
///         direct-declarator '(' identifier-list[opt] ')'
/// [GNU]   direct-declarator '(' parameter-forward-declarations
///                    parameter-type-list[opt] ')'
/// [C++]   direct-declarator '(' parameter-declaration-clause ')'
///                    cv-qualifier-seq[opt] exception-specification[opt]
/// [C++11] direct-declarator '(' parameter-declaration-clause ')'
///                    attribute-specifier-seq[opt] cv-qualifier-seq[opt]
///                    ref-qualifier[opt] exception-specification[opt]
/// [C++]   declarator-id
/// [C++11] declarator-id attribute-specifier-seq[opt]
///
///       declarator-id: [C++ 8]
///         '...'[opt] id-expression
///         '::'[opt] nested-name-specifier[opt] type-name
///
///       id-expression: [C++ 5.1]
///         unqualified-id
///         qualified-id
///
///       unqualified-id: [C++ 5.1]
///         identifier
///         operator-function-id
///         conversion-function-id
///          '~' class-name
///         template-id
///
/// C++17 adds the following, which we also handle here:
///
///       simple-declaration:
///         <decl-spec> '[' identifier-list ']' brace-or-equal-initializer ';'
///
/// Note, any additional constructs added here may need corresponding changes
/// in isConstructorDeclarator.
void Parser::ParseDirectDeclarator(Declarator &D) {
```

当前Tok是identifier(`test`), 这里会进入这个分支:

```c++
    if (Tok.isOneOf(tok::identifier, tok::kw_operator, tok::annot_template_id,
                    tok::tilde)) {
      // We found something that indicates the start of an unqualified-id.
      // Parse that unqualified-id.
      bool AllowConstructorName;
      bool AllowDeductionGuide;
      if (D.getDeclSpec().hasTypeSpecifier()) {
```

通过ParseUnqualifiedId解析函数名:

```c++
      if (ParseUnqualifiedId(D.getCXXScopeSpec(),
                             /*ObjectType=*/nullptr,
                             /*ObjectHadErrors=*/false,
                             /*EnteringContext=*/true,
                             /*AllowDestructorName=*/true, AllowConstructorName,
                             AllowDeductionGuide, nullptr, D.getName()) ||
          // Once we're past the identifier, if the scope was bad, mark the
          // whole declarator bad.
          D.getCXXScopeSpec().isInvalid()) {
```

unqualified-id产生式如下:

```c++
/// Parse a C++ unqualified-id (or a C identifier), which describes the
/// name of an entity.
///
/// \code
///       unqualified-id: [C++ expr.prim.general]
///         identifier
///         operator-function-id
///         conversion-function-id
/// [C++0x] literal-operator-id [TODO]
///         ~ class-name
///         template-id
///
/// \endcode
///
/// \param SS The nested-name-specifier that preceded this unqualified-id. If
/// non-empty, then we are parsing the unqualified-id of a qualified-id.
///
/// \param ObjectType if this unqualified-id occurs within a member access
/// expression, the type of the base object whose member is being accessed.
///
/// \param ObjectHadErrors if this unqualified-id occurs within a member access
/// expression, indicates whether the original subexpressions had any errors.
/// When true, diagnostics for missing 'template' keyword will be supressed.
///
/// \param EnteringContext whether we are entering the scope of the
/// nested-name-specifier.
///
/// \param AllowDestructorName whether we allow parsing of a destructor name.
///
/// \param AllowConstructorName whether we allow parsing a constructor name.
///
/// \param AllowDeductionGuide whether we allow parsing a deduction guide name.
///
/// \param Result on a successful parse, contains the parsed unqualified-id.
///
/// \returns true if parsing fails, false otherwise.
bool Parser::ParseUnqualifiedId(CXXScopeSpec &SS, ParsedType ObjectType,
                                bool ObjectHadErrors, bool EnteringContext,
                                bool AllowDestructorName,
                                bool AllowConstructorName,
                                bool AllowDeductionGuide,
                                SourceLocation *TemplateKWLoc,
                                UnqualifiedId &Result) {
```

消费函数名, return走:

```c++
  // unqualified-id:
  //   identifier
  //   template-id (when it hasn't already been annotated)
  if (Tok.is(tok::identifier)) {
  ParseIdentifier:
    // Consume the identifier.
    IdentifierInfo *Id = Tok.getIdentifierInfo();
    SourceLocation IdLoc = ConsumeToken();
    ...
    return false;
  }
```

回到ParseDirectDeclarator, 吃掉左括号, 调用ParseFunctionDeclarator开始参数解析:

```c++
while (true) {
    if (Tok.is(tok::l_paren)) {
      bool IsFunctionDeclaration = D.isFunctionDeclaratorAFunctionDeclaration();
      // Enter function-declaration scope, limiting any declarators to the
      // function prototype scope, including parameter declarators.
      ...
      ParsedAttributes attrs(AttrFactory);
      BalancedDelimiterTracker T(*this, tok::l_paren);
      T.consumeOpen();
      if (IsFunctionDeclaration)
        Actions.ActOnStartFunctionDeclarationDeclarator(D,
                                                        TemplateParameterDepth);
      ParseFunctionDeclarator(D, attrs, T, IsAmbiguous);
      if (IsFunctionDeclaration)
        Actions.ActOnFinishFunctionDeclarationDeclarator(D);
      PrototypeScope.Exit();
      ...
```

先看下注释:

```c++
/// ParseFunctionDeclarator - We are after the identifier and have parsed the
/// declarator D up to a paren, which indicates that we are parsing function
/// arguments.
///
/// If FirstArgAttrs is non-null, then the caller parsed those attributes
/// immediately after the open paren - they will be applied to the DeclSpec
/// of the first parameter.
///
/// If RequiresArg is true, then the first argument of the function is required
/// to be present and required to not be an identifier list.
///
/// For C++, after the parameter-list, it also parses the cv-qualifier-seq[opt],
/// (C++11) ref-qualifier[opt], exception-specification[opt],
/// (C++11) attribute-specifier-seq[opt], (C++11) trailing-return-type[opt] and
/// (C++2a) the trailing requires-clause.
///
/// [C++11] exception-specification:
///           dynamic-exception-specification
///           noexcept-specification
///
void Parser::ParseFunctionDeclarator(Declarator &D,
                                     ParsedAttributes &FirstArgAttrs,
                                     BalancedDelimiterTracker &Tracker,
                                     bool IsAmbiguous,
                                     bool RequiresArg) {
```

参数解析在这里:

```c++
  } else {
    if (Tok.isNot(tok::r_paren))
      ParseParameterDeclarationClause(D, FirstArgAttrs, ParamInfo, EllipsisLoc);
    else if (RequiresArg)
      Diag(Tok, diag::err_argument_required_after_attribute);
```

看一下参数列表的产生式:

```c++
/// ParseParameterDeclarationClause - Parse a (possibly empty) parameter-list
/// after the opening parenthesis. This function will not parse a K&R-style
/// identifier list.
///
/// DeclContext is the context of the declarator being parsed.  If FirstArgAttrs
/// is non-null, then the caller parsed those attributes immediately after the
/// open paren - they will be applied to the DeclSpec of the first parameter.
///
/// After returning, ParamInfo will hold the parsed parameters. EllipsisLoc will
/// be the location of the ellipsis, if any was parsed.
///
///       parameter-type-list: [C99 6.7.5]
///         parameter-list
///         parameter-list ',' '...'
/// [C++]   parameter-list '...'
///
///       parameter-list: [C99 6.7.5]
///         parameter-declaration
///         parameter-list ',' parameter-declaration
///
///       parameter-declaration: [C99 6.7.5]
///         declaration-specifiers declarator
/// [C++]   declaration-specifiers declarator '=' assignment-expression
/// [C++11]                                       initializer-clause
/// [GNU]   declaration-specifiers declarator attributes
///         declaration-specifiers abstract-declarator[opt]
/// [C++]   declaration-specifiers abstract-declarator[opt]
///           '=' assignment-expression
/// [GNU]   declaration-specifiers abstract-declarator[opt] attributes
/// [C++11] attribute-specifier-seq parameter-declaration
///
void Parser::ParseParameterDeclarationClause(
    DeclaratorContext DeclaratorCtx, ParsedAttributes &FirstArgAttrs,
    SmallVectorImpl<DeclaratorChunk::ParamInfo> &ParamInfo,
    SourceLocation &EllipsisLoc, bool IsACXXFunctionDeclaration) {
```

主体是个do while循环, 不断解析每个参数:

```c++
  do {
  ...
  } while (TryConsumeToken(tok::comma));
}
```

调用ParseDeclarationSpecifiers解析`int`:

```c++
SourceLocation DSStart = Tok.getLocation();

ParseDeclarationSpecifiers(DS, /*TemplateInfo=*/ParsedTemplateInfo(),
                           AS_none, DeclSpecContext::DSC_normal,
                           /*LateAttrs=*/nullptr, AllowImplicitTypename);
DS.takeAttributesFrom(ArgDeclSpecAttrs);
```

递归调用ParseDeclarator解析`x`:

```c++
// Parse the declarator.  This is "PrototypeContext" or
    // "LambdaExprParameterContext", because we must accept either
    // 'declarator' or 'abstract-declarator' here.
    Declarator ParmDeclarator(DS, ArgDeclAttrs,
                              DeclaratorCtx == DeclaratorContext::RequiresExpr
                                  ? DeclaratorContext::RequiresExpr
                              : DeclaratorCtx == DeclaratorContext::LambdaExpr
                                  ? DeclaratorContext::LambdaExprParameter
                                  : DeclaratorContext::Prototype);
    ParseDeclarator(ParmDeclarator);
```

解析完后, 由于下一个token并不是逗号, 因此会退出当前while循环, 相应地也会退出当前函数, 回到ParseFunctionDeclarator.

消费`)`:

```c++
    // OpenCL disallows functions without a prototype, but it doesn't enforce
    // strict prototypes as in C2x because it allows a function definition to
    // have an identifier list. See OpenCL 3.0 6.11/g for more details.
    HasProto = ParamInfo.size() || getLangOpts().requiresStrictPrototypes() ||
               getLangOpts().OpenCL;

    // If we have the closing ')', eat it.
    Tracker.consumeClose();
```

后面还有一些特殊语法判断, 不过我们的例程没涵盖这些情况, 最终会退出ParseFunctionDeclarator, 回到ParseDirectDeclarator. 

来到第二次循环, 此时会走else分支执行break退出while循环, 相应地也会退出ParseDirectDeclarator, 回到ParseDeclaratorInternal.

ParseDeclaratorInternal来到return语句, 退出, 回到ParseDeclarator, 再回到ParseCXXMemberDeclaratorBeforeInitializer.

之后的代码也没什么可执行的, 退出ParseCXXMemberDeclaratorBeforeInitializer, 回到ParseCXXClassMemberDeclaration.

可以看到和之前介绍的一样, ParseCXXMemberDeclaratorBeforeInitializer只负责解析到`{`.

这里通过`{`判断出当前函数是一个函数定义:

```c++
    FunctionDefinitionKind DefinitionKind = FunctionDefinitionKind::Declaration;
    // function-definition:
    //
    // In C++11, a non-function declarator followed by an open brace is a
    // braced-init-list for an in-class member initialization, not an
    // erroneous function definition.
    if (Tok.is(tok::l_brace) && !getLangOpts().CPlusPlus11) {
      DefinitionKind = FunctionDefinitionKind::Definition;
    } else if (DeclaratorInfo.isFunctionDeclarator()) {
      if (Tok.isOneOf(tok::l_brace, tok::colon, tok::kw_try)) {
        DefinitionKind = FunctionDefinitionKind::Definition;
      } else if (Tok.is(tok::equal)) {
```

这里调用ParseCXXInlineMethodDef解析函数定义:

```c++
      Decl *FunDecl = ParseCXXInlineMethodDef(AS, AccessAttrs, DeclaratorInfo,
                                              TemplateInfo, VS, PureSpecLoc);
```

ParseCXXInlineMethodDef注释:

```c++
/// ParseCXXInlineMethodDef - We parsed and verified that the specified
/// Declarator is a well formed C++ inline method definition. Now lex its body
/// and store its tokens for parsing after the C++ class is complete.
NamedDecl *Parser::ParseCXXInlineMethodDef(
    AccessSpecifier AS, const ParsedAttributesView &AccessAttrs,
    ParsingDeclarator &D, const ParsedTemplateInfo &TemplateInfo,
    const VirtSpecifiers &VS, SourceLocation PureSpecLoc) {
```

延迟解析函数体:

```c++
  if (FnD)
    HandleMemberFunctionDeclDelays(D, FnD);
```

看HandleMemberFunctionDeclDelays的注释就能明白, 在遍历类的member的过程中不会解析body, 在遍历完整个类后会统一延迟解析body, 这也是为什么member函数可以索引到其下面的member函数:

```c++
/// If the given declarator has any parts for which parsing has to be
/// delayed, e.g., default arguments or an exception-specification, create a
/// late-parsed method declaration record to handle the parsing at the end of
/// the class definition.
void Parser::HandleMemberFunctionDeclDelays(Declarator &DeclaratorInfo,
                                            Decl *ThisDecl) {
```

不过这个函数里只有一些关于延后解析逻辑的设置, 并没有实际的解析内容, 这里只是为了展示其注释, 方便理解, 我们还是回到ParseCXXInlineMethodDef继续解析.

```c++
  // In delayed template parsing mode, if we are within a class template
  // or if we are about to parse function member template then consume
  // the tokens and store them for parsing at the end of the translation unit.
  if (getLangOpts().DelayedTemplateParsing &&
      D.getFunctionDefinitionKind() == FunctionDefinitionKind::Definition &&
      !D.getDeclSpec().hasConstexprSpecifier() &&
      !(FnD && FnD->getAsFunction() &&
        FnD->getAsFunction()->getReturnType()->getContainedAutoType()) &&
      ((Actions.CurContext->isDependentContext() ||
        (TemplateInfo.Kind != ParsedTemplateInfo::NonTemplate &&
         TemplateInfo.Kind != ParsedTemplateInfo::ExplicitSpecialization)) &&
       !Actions.IsInsideALocalClassWithinATemplateFunction())) {
```

这段逻辑虽然不会被执行到, 但其注释也值得关注. 大体意思是如果开启delayed template parsing mode模式(`-fdelayed-template-parsing`), 在解析模板类中的memnber, 或者解析普通类中的member模板时, 会跳过其函数体, 把相关token存储下来, 在解析完translation unit后再来解析. 

> C++ 中的 **delayed template parsing mode** 是 Clang 编译器在处理 C++ 模板时的一种解析策略。它允许模板的某些部分（例如函数体）在模板实例化时延迟解析，而不是在模板定义时立即解析。这种模式可以提高编译器处理模板代码的效率，尤其是在模板的实例化涉及到复杂的类型推导或特化时。
>
> **工作原理**：
>
> - 在普通模式下，当编译器遇到模板定义时，它会立即解析模板的所有部分，包括函数体。这样做的好处是可以在模板定义时捕获语法错误，但缺点是编译时间较长，尤其是当模板并未在程序中被使用时，编译这些模板可能是多余的。
> - 在 delayed template parsing mode 下，编译器只解析模板的声明部分（即函数的签名、类的声明等），并推迟对函数体的解析，直到该模板被实例化。这意味着只有在编译器确定模板实例会被使用时，才会解析它的函数体。这可以加快编译速度，尤其是在大型项目中，未使用的模板不会被完全解析。

<font color="red">注意：隐式实例化也是在解析完translation unit后再进行的</font>

这里将当前函数加入LateParsedDeclarations , 用于在解析完整个 类后延后解析成员函数:

```c++
  // Consume the tokens and store them for later parsing.

  LexedMethod* LM = new LexedMethod(this, FnD);
  getCurrentClass().LateParsedDeclarations.push_back(LM);
  CachedTokens &Toks = LM->Toks;

  tok::TokenKind kind = Tok.getKind();
```

LateParsedDeclarationsContainer注释:

```c++
/// LateParsedDeclarations - Method declarations, inline definitions and
    /// nested classes that contain pieces whose parsing will be delayed until
    /// the top-level class is fully defined.
    LateParsedDeclarationsContainer LateParsedDeclarations;
```

调用ConsumeAndStoreFunctionPrologue存储到`{`(包含`{`)为止的全部内容:

```c++
// Consume everything up to (and including) the left brace of the
  // function body.
  if (ConsumeAndStoreFunctionPrologue(Toks)) {
    // We didn't find the left-brace we expected after the
    // constructor initializer.
```

看下相关注释:

```c++
/// Consume tokens and store them in the passed token container until
/// we've passed the try keyword and constructor initializers and have consumed
/// the opening brace of the function body. The opening brace will be consumed
/// if and only if there was no error.
///
/// \return True on error.
bool Parser::ConsumeAndStoreFunctionPrologue(CachedTokens &Toks) {
```

调用ConsumeAndStoreUntil清理一些垃圾Token, 错误诊断场景下才会work, 这里我们可以简单跳过:

```c++
if (Tok.isNot(tok::colon)) {
  // Easy case, just a function body.

  // Grab any remaining garbage to be diagnosed later. We stop when we reach a
  // brace: an opening one is the function body, while a closing one probably
  // means we've reached the end of the class.
  ConsumeAndStoreUntil(tok::l_brace, tok::r_brace, Toks,
                       /*StopAtSemi=*/true,
                       /*ConsumeFinalToken=*/false);
  if (Tok.isNot(tok::l_brace))
    return Diag(Tok.getLocation(), diag::err_expected) << tok::l_brace;

  Toks.push_back(Tok);
  ConsumeBrace();
  return false;
}
```

消费`{`:

```c++
if (Tok.isNot(tok::l_brace))
  return Diag(Tok.getLocation(), diag::err_expected) << tok::l_brace;

Toks.push_back(Tok);
ConsumeBrace();
return false;
```

回到ParseCXXInlineMethodDef, 调用ConsumeAndStoreUntil存储`}`(包括`}`)前的全部内容.

进行函数重定义检查:

```c++
  if (FnD) {
    FunctionDecl *FD = FnD->getAsFunction();
    // Track that this function will eventually have a body; Sema needs
    // to know this.
    Actions.CheckForFunctionRedefinition(FD);
    FD->setWillHaveBody(true);
```

回到ParseCXXClassMemberDeclaration, 返回ParseCXXClassMemberDeclarationWithPragmas, 返回ParseCXXMemberSpecification, 此时已经到了class的`}`, 没有其他member可以继续解析, 所以消费`}`, 退出while循环:

```c++
    // While we still have something to read, read the member-declarations.
    while (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
           Tok.isNot(tok::eof)) {
      // Each iteration of this loop reads one member-declaration.
      ParseCXXClassMemberDeclarationWithPragmas(
          CurAS, AccessAttrs, static_cast<DeclSpec::TST>(TagType), TagDecl);
      MaybeDestroyTemplateIds();
    }
    T.consumeClose();
```

调用ParseLexedMethodDeclarations解析之前延迟的member:

```c++
  // C++11 [class.mem]p2:
  //   Within the class member-specification, the class is regarded as complete
  //   within function bodies, default arguments, exception-specifications, and
  //   brace-or-equal-initializers for non-static data members (including such
  //   things in nested classes).
  if (TagDecl && NonNestedClass) {
    // We are not inside a nested class. This class and its nested classes
    // are complete and we can parse the delayed portions of method
    // declarations and the lexed inline method definitions, along with any
    // delayed attributes.

    SourceLocation SavedPrevTokLocation = PrevTokLocation;
    ParseLexedPragmas(getCurrentClass());
    ParseLexedAttributes(getCurrentClass());
    ParseLexedMethodDeclarations(getCurrentClass());
```

相关注释:

```c++
/// ParseLexedMethodDeclarations - We finished parsing the member
/// specification of a top (non-nested) C++ class. Now go over the
/// stack of method declarations with some parts for which parsing was
/// delayed (such as default arguments) and parse them.
void Parser::ParseLexedMethodDeclarations(ParsingClass &Class) {
  ReenterClassScopeRAII InClassScope(*this, Class);

  for (LateParsedDeclaration *LateD : Class.LateParsedDeclarations)
    LateD->ParseLexedMethodDeclarations();
}
```

这里LateParsedDeclarations只有一个元素, 就是我们之前存储的`test`. gdb验证一下:

```shell
(gdb) print LateD
$1 = (clang::Parser::LexedMethod *) 0x555555624c30
(gdb) print ((LexedMethod*)LateD)->D
$2 = (clang::CXXMethodDecl *) 0x5555556ac908
(gdb) print ((CXXMethodDecl*)((LexedMethod*)LateD)->D)->getNameAsString()
$3 = "test"
```

调用ParseLexedMethodDeclarations进行延后解析:

```c++
void Parser::LateParsedDeclaration::ParseLexedMethodDeclarations() {}
```

纳尼? 居然是空的! 看来真正的解析在别的地方...

回到ParseDeclarationSpecifiers, 继续走到ParseLexedMethodDefs, 这次应该是延后解析函数定义的地方了吧:

```c++
// We've finished with all pending member declarations.
Actions.ActOnFinishCXXMemberDecls();

ParseLexedMemberInitializers(getCurrentClass());
ParseLexedMethodDefs(getCurrentClass());
```

相关注释:

```c++
/// ParseLexedMethodDefs - We finished parsing the member specification of a top
/// (non-nested) C++ class. Now go over the stack of lexed methods that were
/// collected during its parsing and parse them all.
void Parser::ParseLexedMethodDefs(ParsingClass &Class) {
  ReenterClassScopeRAII InClassScope(*this, Class);

  for (LateParsedDeclaration *D : Class.LateParsedDeclarations)
    D->ParseLexedMethodDefs();
}
```

嗯, ParseLexedMethodDefs这次不为空了:

```c++
void Parser::LexedMethod::ParseLexedMethodDefs() {
  Self->ParseLexedMethodDef(*this);
}
```

继续调用ParseLexedMethodDef:

```c++
void Parser::ParseLexedMethodDef(LexedMethod &LM) {
  // If this is a member template, introduce the template parameter scope.
  ReenterTemplateScopeRAII InFunctionTemplateScope(*this, LM.D);

  ParenBraceBracketBalancer BalancerRAIIObj(*this);

  assert(!LM.Toks.empty() && "Empty body!");
  Token LastBodyToken = LM.Toks.back();
```

此时LM.Toks中共有5个元素, 分别是`{ return 1; }`:

```shell
$1 = (clang::Token &) @0x5555556b6d50: {Loc = 67, UintData = 1, PtrData = 0x0, Kind = clang::tok::r_brace, Flags = 2}
(gdb) print LM.Toks[0]
$2 = (clang::Token &) @0x5555556b6cf0: {Loc = 55, UintData = 1, PtrData = 0x0, Kind = clang::tok::l_brace, Flags = 2}
(gdb) print LM.Toks[1]
$3 = (clang::Token &) @0x5555556b6d08: {Loc = 57, UintData = 6, PtrData = 0x555555636ee8, Kind = clang::tok::kw_return, Flags = 2}
(gdb) print LM.Toks[2]
$4 = (clang::Token &) @0x5555556b6d20: {Loc = 64, UintData = 1, PtrData = 0x5555556c5118, Kind = clang::tok::identifier, Flags = 2}
(gdb) print LM.Toks[3]
$5 = (clang::Token &) @0x5555556b6d38: {Loc = 65, UintData = 1, PtrData = 0x0, Kind = clang::tok::semi, Flags = 0}
(gdb) print LM.Toks[4]
$6 = (clang::Token &) @0x5555556b6d50: {Loc = 67, UintData = 1, PtrData = 0x0, Kind = clang::tok::r_brace, Flags = 2}
```

为`LM`的Token序列尾部添加eof (另外在eof后还添加了当前token(`;`), 用这样一个小trick避免当前token丢失):

```c++
  Token BodyEnd;
  BodyEnd.startToken();
  BodyEnd.setKind(tok::eof);
  BodyEnd.setLocation(LastBodyToken.getEndLoc());
  BodyEnd.setEofData(LM.D);
  LM.Toks.push_back(BodyEnd);
  // Append the current token at the end of the new token stream so that it
  // doesn't get lost.
  LM.Toks.push_back(Tok);
```

配置PP:

```c++
PP.EnterTokenStream(LM.Toks, true, /*IsReinject*/true);

// Consume the previously pushed token.
ConsumeAnyToken(/*ConsumeCodeCompletionTok=*/true);
```

由于我们已经将`;`安全地压入了LM.Toks,这里可以用ConsumeAnyToken将其安全消耗掉, 再由于我们对PP的配置, 下一个Token将会是位于LM.Toks头部的`{`.

解析函数体:

```c++
  assert((Actions.getDiagnostics().hasErrorOccurred() ||
          !isa<FunctionTemplateDecl>(LM.D) ||
          cast<FunctionTemplateDecl>(LM.D)->getTemplateParameters()->getDepth()
            < TemplateParameterDepth) &&
         "TemplateParameterDepth should be greater than the depth of "
         "current template being instantiated!");

  ParseFunctionStatementBody(LM.D, FnScope);
```

进入ParseFunctionStatementBody:

```c++
Decl *Parser::ParseFunctionStatementBody(Decl *Decl, ParseScope &BodyScope) {
  assert(Tok.is(tok::l_brace));
  SourceLocation LBraceLoc = Tok.getLocation();
```

调用ParseCompoundStatementBody解析函数体中的语句:

```c++
  // Do not enter a scope for the brace, as the arguments are in the same scope
  // (the function body) as the body itself.  Instead, just read the statement
  // list and put it into a CompoundStmt for safe keeping.
  StmtResult FnBody(ParseCompoundStatementBody());
```

相关注释:

```c++
/// ParseCompoundStatementBody - Parse a sequence of statements optionally
/// followed by a label and invoke the ActOnCompoundStmt action.  This expects
/// the '{' to be the current token, and consume the '}' at the end of the
/// block.  It does not manipulate the scope stack.
StmtResult Parser::ParseCompoundStatementBody(bool isStmtExpr) {
```

消费`{`:

```c++
  InMessageExpressionRAIIObject InMessage(*this, false);
  BalancedDelimiterTracker T(*this, tok::l_brace);
  if (T.consumeOpen())
    return StmtError();
```

这里我们的语句前并没有编译指令, 因此跳过:

```c++
  // Parse any pragmas at the beginning of the compound statement.
  ParseCompoundStatementLeadingPragmas();
```

进入while循环不断解析函数体内的语句:

```c++
while (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
         Tok.isNot(tok::eof)) {
```

ParseCompoundStatementBody -> ParseStatementOrDeclaration

```c++
StmtResult R;
if (Tok.isNot(tok::kw___extension__)) {
  R = ParseStatementOrDeclaration(Stmts, SubStmtCtx);
```

相关注释:

```c++
/// ParseStatementOrDeclaration - Read 'statement' or 'declaration'.
///       StatementOrDeclaration:
///         statement
///         declaration
///
///       statement:
///         labeled-statement
///         compound-statement
///         expression-statement
///         selection-statement
///         iteration-statement
///         jump-statement
/// [C++]   declaration-statement
/// [C++]   try-block
/// [MS]    seh-try-block
/// [OBC]   objc-throw-statement
/// [OBC]   objc-try-catch-statement
/// [OBC]   objc-synchronized-statement
/// [GNU]   asm-statement
/// [OMP]   openmp-construct             [TODO]
///
///       labeled-statement:
///         identifier ':' statement
///         'case' constant-expression ':' statement
///         'default' ':' statement
///
///       selection-statement:
///         if-statement
///         switch-statement
///
///       iteration-statement:
///         while-statement
///         do-statement
///         for-statement
///
///       expression-statement:
///         expression[opt] ';'
///
///       jump-statement:
///         'goto' identifier ';'
///         'continue' ';'
///         'break' ';'
///         'return' expression[opt] ';'
/// [GNU]   'goto' '*' expression ';'
///
/// [OBC] objc-throw-statement:
/// [OBC]   '@' 'throw' expression ';'
/// [OBC]   '@' 'throw' ';'
///
StmtResult
Parser::ParseStatementOrDeclaration(StmtVector &Stmts,
                                    ParsedStmtContext StmtCtx,
                                    SourceLocation *TrailingElseLoc) {
```

ParseStatementOrDeclaration -> ParseStatementOrDeclarationAfterAttributes

```c++
StmtResult Res = ParseStatementOrDeclarationAfterAttributes(
    Stmts, StmtCtx, TrailingElseLoc, CXX11Attrs, GNUAttrs);
MaybeDestroyTemplateIds();
```

来到switch case判断:

```c++
StmtResult Parser::ParseStatementOrDeclarationAfterAttributes(
    StmtVector &Stmts, ParsedStmtContext StmtCtx,
    SourceLocation *TrailingElseLoc, ParsedAttributes &CXX11Attrs,
    ParsedAttributes &GNUAttrs) {
  const char *SemiError = nullptr;
  StmtResult Res;
  SourceLocation GNUAttributeLoc;

  // Cases in this switch statement should fall through if the parser expects
  // the token to end in a semicolon (in which case SemiError should be set),
  // or they directly 'return;' if not.
Retry:
  tok::TokenKind Kind  = Tok.getKind();
  SourceLocation AtLoc;
  switch (Kind) {
```

走return分支:

```c++
  case tok::kw_return:              // C99 6.8.6.4: return-statement
    Res = ParseReturnStatement();
    SemiError = "return";
    break;
```

调用ParseReturnStatement, 相关注释:

```c++
/// ParseReturnStatement
///       jump-statement:
///         'return' expression[opt] ';'
///         'return' braced-init-list ';'
///         'co_return' expression[opt] ';'
///         'co_return' braced-init-list ';'
StmtResult Parser::ParseReturnStatement() {
```

消费`return`:

```c++
  bool IsCoreturn = Tok.is(tok::kw_co_return);
  SourceLocation ReturnLoc = ConsumeToken();  
```

调用ParseExpression解析表达式:

```c++
            << R.get()->getSourceRange();
    } else
      R = ParseExpression();
    if (R.isInvalid()) {
```

相关注释:
```c++
/// Simple precedence-based parser for binary/ternary operators.
///
/// Note: we diverge from the C99 grammar when parsing the assignment-expression
/// production.  C99 specifies that the LHS of an assignment operator should be
/// parsed as a unary-expression, but consistency dictates that it be a
/// conditional-expession.  In practice, the important thing here is that the
/// LHS of an assignment has to be an l-value, which productions between
/// unary-expression and conditional-expression don't produce.  Because we want
/// consistency, we parse the LHS as a conditional-expression, then check for
/// l-value-ness in semantic analysis stages.
///
/// \verbatim
///       pm-expression: [C++ 5.5]
///         cast-expression
///         pm-expression '.*' cast-expression
///         pm-expression '->*' cast-expression
///
///       multiplicative-expression: [C99 6.5.5]
///     Note: in C++, apply pm-expression instead of cast-expression
///         cast-expression
///         multiplicative-expression '*' cast-expression
///         multiplicative-expression '/' cast-expression
///         multiplicative-expression '%' cast-expression
///
///       additive-expression: [C99 6.5.6]
///         multiplicative-expression
///         additive-expression '+' multiplicative-expression
///         additive-expression '-' multiplicative-expression
///
///       shift-expression: [C99 6.5.7]
///         additive-expression
///         shift-expression '<<' additive-expression
///         shift-expression '>>' additive-expression
///
///       compare-expression: [C++20 expr.spaceship]
///         shift-expression
///         compare-expression '<=>' shift-expression
///
///       relational-expression: [C99 6.5.8]
///         compare-expression
///         relational-expression '<' compare-expression
///         relational-expression '>' compare-expression
///         relational-expression '<=' compare-expression
///         relational-expression '>=' compare-expression
///
///       equality-expression: [C99 6.5.9]
///         relational-expression
///         equality-expression '==' relational-expression
///         equality-expression '!=' relational-expression
///
///       AND-expression: [C99 6.5.10]
///         equality-expression
///         AND-expression '&' equality-expression
///
///       exclusive-OR-expression: [C99 6.5.11]
///         AND-expression
///         exclusive-OR-expression '^' AND-expression
///
///       inclusive-OR-expression: [C99 6.5.12]
///         exclusive-OR-expression
///         inclusive-OR-expression '|' exclusive-OR-expression
///
///       logical-AND-expression: [C99 6.5.13]
///         inclusive-OR-expression
///         logical-AND-expression '&&' inclusive-OR-expression
///
///       logical-OR-expression: [C99 6.5.14]
///         logical-AND-expression
///         logical-OR-expression '||' logical-AND-expression
///
///       conditional-expression: [C99 6.5.15]
///         logical-OR-expression
///         logical-OR-expression '?' expression ':' conditional-expression
/// [GNU]   logical-OR-expression '?' ':' conditional-expression
/// [C++] the third operand is an assignment-expression
///
///       assignment-expression: [C99 6.5.16]
///         conditional-expression
///         unary-expression assignment-operator assignment-expression
/// [C++]   throw-expression [C++ 15]
///
///       assignment-operator: one of
///         = *= /= %= += -= <<= >>= &= ^= |=
///
///       expression: [C99 6.5.17]
///         assignment-expression ...[opt]
///         expression ',' assignment-expression ...[opt]
/// \endverbatim
ExprResult Parser::ParseExpression(TypeCastState isTypeCast) {
  ExprResult LHS(ParseAssignmentExpression(isTypeCast));
  return ParseRHSOfBinaryExpression(LHS, prec::Comma);
}
```

调用ParseAssignmentExpression解析分配表达式, 相关注释:

```c++
/// Parse an expr that doesn't include (top-level) commas.
ExprResult Parser::ParseAssignmentExpression(TypeCastState isTypeCast) {
```

解析ParseCastExpression:

```c++
if (Tok.is(tok::kw_throw))
  return ParseThrowExpression();
if (Tok.is(tok::kw_co_yield))
  return ParseCoyieldExpression();

ExprResult LHS = ParseCastExpression(AnyCastExpr,
                                     /*isAddressOfOperand=*/false,
                                     isTypeCast);
```

相关注释:

```c++
/// Parse a cast-expression, unary-expression or primary-expression, based
/// on \p ExprType.
///
/// \p isAddressOfOperand exists because an id-expression that is the
/// operand of address-of gets special treatment due to member pointers.
///
ExprResult Parser::ParseCastExpression(CastParseKind ParseKind,
                                       bool isAddressOfOperand,
                                       TypeCastState isTypeCast,
                                       bool isVectorLiteral,
                                       bool *NotPrimaryExpression) {
```

可以看到这里不仅能解析cast-expression, 还能解析unary-expression或primary-expression

调用真正的ParseCastExpression:

```c++
ExprResult Res = ParseCastExpression(ParseKind,
                                     isAddressOfOperand,
                                     NotCastExpr,
                                     isTypeCast,
                                     isVectorLiteral,
                                     NotPrimaryExpression);
```

相关注释:

```c++
/// Parse a cast-expression, or, if \pisUnaryExpression is true, parse
/// a unary-expression.
///
/// \p isAddressOfOperand exists because an id-expression that is the operand
/// of address-of gets special treatment due to member pointers. NotCastExpr
/// is set to true if the token is not the start of a cast-expression, and no
/// diagnostic is emitted in this case and no tokens are consumed.
///
/// \verbatim
///       cast-expression: [C99 6.5.4]
///         unary-expression
///         '(' type-name ')' cast-expression
///
///       unary-expression:  [C99 6.5.3]
///         postfix-expression
///         '++' unary-expression
///         '--' unary-expression
/// [Coro]  'co_await' cast-expression
///         unary-operator cast-expression
///         'sizeof' unary-expression
///         'sizeof' '(' type-name ')'
/// [C++11] 'sizeof' '...' '(' identifier ')'
/// [GNU]   '__alignof' unary-expression
/// [GNU]   '__alignof' '(' type-name ')'
/// [C11]   '_Alignof' '(' type-name ')'
/// [C++11] 'alignof' '(' type-id ')'
/// [GNU]   '&&' identifier
/// [C++11] 'noexcept' '(' expression ')' [C++11 5.3.7]
/// [C++]   new-expression
/// [C++]   delete-expression
///
///       unary-operator: one of
///         '&'  '*'  '+'  '-'  '~'  '!'
/// [GNU]   '__extension__'  '__real'  '__imag'
///
///       primary-expression: [C99 6.5.1]
/// [C99]   identifier
/// [C++]   id-expression
///         constant
///         string-literal
/// [C++]   boolean-literal  [C++ 2.13.5]
/// [C++11] 'nullptr'        [C++11 2.14.7]
/// [C++11] user-defined-literal
///         '(' expression ')'
/// [C11]   generic-selection
/// [C++2a] requires-expression
///         '__func__'        [C99 6.4.2.2]
/// [GNU]   '__FUNCTION__'
/// [MS]    '__FUNCDNAME__'
/// [MS]    'L__FUNCTION__'
/// [MS]    '__FUNCSIG__'
/// [MS]    'L__FUNCSIG__'
/// [GNU]   '__PRETTY_FUNCTION__'
/// [GNU]   '(' compound-statement ')'
/// [GNU]   '__builtin_va_arg' '(' assignment-expression ',' type-name ')'
/// [GNU]   '__builtin_offsetof' '(' type-name ',' offsetof-member-designator')'
/// [GNU]   '__builtin_choose_expr' '(' assign-expr ',' assign-expr ','
///                                     assign-expr ')'
/// [GNU]   '__builtin_FILE' '(' ')'
/// [GNU]   '__builtin_FUNCTION' '(' ')'
/// [GNU]   '__builtin_LINE' '(' ')'
/// [CLANG] '__builtin_COLUMN' '(' ')'
/// [GNU]   '__builtin_source_location' '(' ')'
/// [GNU]   '__builtin_types_compatible_p' '(' type-name ',' type-name ')'
/// [GNU]   '__null'
/// [OBJC]  '[' objc-message-expr ']'
/// [OBJC]  '\@selector' '(' objc-selector-arg ')'
/// [OBJC]  '\@protocol' '(' identifier ')'
/// [OBJC]  '\@encode' '(' type-name ')'
/// [OBJC]  objc-string-literal
/// [C++]   simple-type-specifier '(' expression-list[opt] ')'      [C++ 5.2.3]
/// [C++11] simple-type-specifier braced-init-list                  [C++11 5.2.3]
/// [C++]   typename-specifier '(' expression-list[opt] ')'         [C++ 5.2.3]
/// [C++11] typename-specifier braced-init-list                     [C++11 5.2.3]
/// [C++]   'const_cast' '<' type-name '>' '(' expression ')'       [C++ 5.2p1]
/// [C++]   'dynamic_cast' '<' type-name '>' '(' expression ')'     [C++ 5.2p1]
/// [C++]   'reinterpret_cast' '<' type-name '>' '(' expression ')' [C++ 5.2p1]
/// [C++]   'static_cast' '<' type-name '>' '(' expression ')'      [C++ 5.2p1]
/// [C++]   'typeid' '(' expression ')'                             [C++ 5.2p1]
/// [C++]   'typeid' '(' type-id ')'                                [C++ 5.2p1]
/// [C++]   'this'          [C++ 9.3.2]
/// [G++]   unary-type-trait '(' type-id ')'
/// [G++]   binary-type-trait '(' type-id ',' type-id ')'           [TODO]
/// [EMBT]  array-type-trait '(' type-id ',' integer ')'
/// [clang] '^' block-literal
///
///       constant: [C99 6.4.4]
///         integer-constant
///         floating-constant
///         enumeration-constant -> identifier
///         character-constant
///
///       id-expression: [C++ 5.1]
///                   unqualified-id
///                   qualified-id
///
///       unqualified-id: [C++ 5.1]
///                   identifier
///                   operator-function-id
///                   conversion-function-id
///                   '~' class-name
///                   template-id
///
///       new-expression: [C++ 5.3.4]
///                   '::'[opt] 'new' new-placement[opt] new-type-id
///                                     new-initializer[opt]
///                   '::'[opt] 'new' new-placement[opt] '(' type-id ')'
///                                     new-initializer[opt]
///
///       delete-expression: [C++ 5.3.5]
///                   '::'[opt] 'delete' cast-expression
///                   '::'[opt] 'delete' '[' ']' cast-expression
///
/// [GNU/Embarcadero] unary-type-trait:
///                   '__is_arithmetic'
///                   '__is_floating_point'
///                   '__is_integral'
///                   '__is_lvalue_expr'
///                   '__is_rvalue_expr'
///                   '__is_complete_type'
///                   '__is_void'
///                   '__is_array'
///                   '__is_function'
///                   '__is_reference'
///                   '__is_lvalue_reference'
///                   '__is_rvalue_reference'
///                   '__is_fundamental'
///                   '__is_object'
///                   '__is_scalar'
///                   '__is_compound'
///                   '__is_pointer'
///                   '__is_member_object_pointer'
///                   '__is_member_function_pointer'
///                   '__is_member_pointer'
///                   '__is_const'
///                   '__is_volatile'
///                   '__is_trivial'
///                   '__is_standard_layout'
///                   '__is_signed'
///                   '__is_unsigned'
///
/// [GNU] unary-type-trait:
///                   '__has_nothrow_assign'
///                   '__has_nothrow_copy'
///                   '__has_nothrow_constructor'
///                   '__has_trivial_assign'                  [TODO]
///                   '__has_trivial_copy'                    [TODO]
///                   '__has_trivial_constructor'
///                   '__has_trivial_destructor'
///                   '__has_virtual_destructor'
///                   '__is_abstract'                         [TODO]
///                   '__is_class'
///                   '__is_empty'                            [TODO]
///                   '__is_enum'
///                   '__is_final'
///                   '__is_pod'
///                   '__is_polymorphic'
///                   '__is_sealed'                           [MS]
///                   '__is_trivial'
///                   '__is_union'
///                   '__has_unique_object_representations'
///
/// [Clang] unary-type-trait:
///                   '__is_aggregate'
///                   '__trivially_copyable'
///
///       binary-type-trait:
/// [GNU]             '__is_base_of'
/// [MS]              '__is_convertible_to'
///                   '__is_convertible'
///                   '__is_same'
///
/// [Embarcadero] array-type-trait:
///                   '__array_rank'
///                   '__array_extent'
///
/// [Embarcadero] expression-trait:
///                   '__is_lvalue_expr'
///                   '__is_rvalue_expr'
/// \endverbatim
///
ExprResult Parser::ParseCastExpression(CastParseKind ParseKind,
                                       bool isAddressOfOperand,
                                       bool &NotCastExpr,
                                       TypeCastState isTypeCast,
                                       bool isVectorLiteral,
                                       bool *NotPrimaryExpression) {
```

这里我们要解析的应该是primary-expression.

来到一个复杂的switch(900+行):

```c++
  // This handles all of cast-expression, unary-expression, postfix-expression,
  // and primary-expression.  We handle them together like this for efficiency
  // and to simplify handling of an expression starting with a '(' token: which
  // may be one of a parenthesized expression, cast-expression, compound literal
  // expression, or statement expression.
  //
  // If the parsed tokens consist of a primary-expression, the cases below
  // break out of the switch;  at the end we call ParsePostfixExpressionSuffix
  // to handle the postfix expression suffixes.  Cases that cannot be followed
  // by postfix exprs should set AllowSuffix to false.
  switch (SavedKind) {
```

走到identifier分支:

```c++
  case tok::identifier:
  ParseIdentifier: {    // primary-expression: identifier
                        // unqualified-id: identifier
                        // constant: enumeration-constant
    // Turn a potentially qualified name into a annot_typename or
    // annot_cxxscope if it would be valid.  This handles things like x::y, etc.
    if (getLangOpts().CPlusPlus) {
      // Avoid the unnecessary parse-time lookup in the common case
      // where the syntax forbids a type.
      const Token &Next = NextToken();
```

消费`x`:

```c++
    // Consume the identifier so that we can see if it is followed by a '(' or
    // '.'.
    IdentifierInfo &II = *Tok.getIdentifierInfo();
    SourceLocation ILoc = ConsumeToken();
```

后面有一大段复杂的逻辑, 不过由于我们已经到了`;`, 这些逻辑都不会执行, 最终break出switch:

```c++
    if (!Res.isInvalid() && Tok.is(tok::less))
      checkPotentialAngleBracket(Res);
    break;
  }
```

ParseCastExpression后面也没有什么东西要处理, 回到ParseAssignmentExpression, 执行ParseRHSOfBinaryExpression:

```c++
  ExprResult LHS = ParseCastExpression(AnyCastExpr,
                                       /*isAddressOfOperand=*/false,
                                       isTypeCast);
  return ParseRHSOfBinaryExpression(LHS, prec::Assignment);
```

不过ParseRHSOfBinaryExpression也没什么可做的, 回到ParseAssignmentExpression, 返回ParseExpression, 继续执行ParseRHSOfBinaryExpression:

```c++
ExprResult Parser::ParseExpression(TypeCastState isTypeCast) {
  ExprResult LHS(ParseAssignmentExpression(isTypeCast));
  return ParseRHSOfBinaryExpression(LHS, prec::Comma);
}
```

同理, 这里ParseRHSOfBinaryExpression也没什么可做的, 返回到ParseReturnStatement, 接着返回到ParseStatementOrDeclarationAfterAttributes, 退出swith, 在最后把`;`消费掉:

```c++
  // If we reached this code, the statement must end in a semicolon.
  if (!TryConsumeToken(tok::semi) && !Res.isInvalid()) {
    // If the result was valid, then we do want to diagnose this.  Use
    // ExpectAndConsume to emit the diagnostic, even though we know it won't
    // succeed.
    ExpectAndConsume(tok::semi, diag::err_expected_semi_after_stmt, SemiError);
    // Skip until we see a } or ;, but don't eat it.
    SkipUntil(tok::r_brace, StopAtSemi | StopBeforeMatch);
  }
```

返回ParseStatementOrDeclaration, 返回ParseCompoundStatementBody, 此时已解析完全部语句, 退出while循环, 消费掉`}`:

```c++
  // We broke out of the while loop because we found a '}' or EOF.
  if (!T.consumeClose()) {
    // If this is the '})' of a statement expression, check that it's written
    // in a sensible way.
    if (isStmtExpr && Tok.is(tok::r_paren))
      checkCompoundToken(CloseLoc, tok::r_brace, CompoundToken::StmtExprEnd);
  } else {
```

回到ParseFunctionStatementBody, 回到ParseLexedMethodDef, 消费eof:

```c++
  if (Tok.is(tok::eof) && Tok.getEofData() == LM.D)
    ConsumeAnyToken();
```

注意, 当前的Tok为我们之前存储的类结尾的`;`, 延迟member解析完毕, 我们可以继续解析源文件的剩余内容.

一路退回到ParseDeclarationSpecifiers, 第二次while switch会走到DoneWithDeclSpec:

```c++
    switch (Tok.getKind()) {
    default:
    DoneWithDeclSpec:
      if (!AttrsLastTime)
        ProhibitAttributes(attrs);
      else {
```

然后return:

```c++
      // If this is not a declaration specifier token, we're done reading decl
      // specifiers.  First verify that DeclSpec's are consistent.
      DS.Finish(Actions, Policy);
      return;
```

退回到ParseSingleDeclarationAfterTemplate, 消费`;`并return:

```c++
  if (Tok.is(tok::semi)) {
    ProhibitAttributes(prefixAttrs);
    DeclEnd = ConsumeToken();
    RecordDecl *AnonRecord = nullptr;
    Decl *Decl = Actions.ParsedFreeStandingDeclSpec(
        getCurScope(), AS, DS, ParsedAttributesView::none(),
        TemplateInfo.TemplateParams ? *TemplateInfo.TemplateParams
                                    : MultiTemplateParamsArg(),
        TemplateInfo.Kind == ParsedTemplateInfo::ExplicitInstantiation,
        AnonRecord);
    assert(!AnonRecord &&
           "Anonymous unions/structs should not be valid with template");
    DS.complete(Decl);
    return Decl;
  }
```

一路退回到ParseTopLevelDecl, 再退回到ParserAST.cpp, 进行下一轮ParseTopLevelDecl:

```c++
    for (bool AtEOF = P.ParseFirstTopLevelDecl(ADecl, ImportState); !AtEOF;
         AtEOF = P.ParseTopLevelDecl(ADecl, ImportState)) {
      // If we got a null return and something *was* parsed, ignore it.  This
      // is due to a top-level semicolon, an action override, or a parse error
      // skipping something.
      if (ADecl && !Consumer->HandleTopLevelDecl(ADecl.get()))
        return;
    }
```

再次ParseExternalDeclaration, switch走default分支, 调用ParseDeclarationOrFunctionDefinition:

```c++
default:
dont_know:
  if (Tok.isEditorPlaceholder()) {
    ConsumeToken();
    return nullptr;
  }
  if (PP.isIncrementalProcessingEnabled() &&
      !isDeclarationStatement(/*DisambiguatingWithExpression=*/true))
    return ParseTopLevelStmtDecl();

  // We can't tell whether this is a function-definition or declaration yet.
  if (!SingleDecl)
    return ParseDeclarationOrFunctionDefinition(Attrs, DeclSpecAttrs, DS);
}
```

调用ParseDeclOrFunctionDefInternal:

```c++
} else {
  ParsingDeclSpec PDS(*this);
  // Must temporarily exit the objective-c container scope for
  // parsing c constructs and re-enter objc container scope
  // afterwards.
  ObjCDeclContextSwitch ObjCDC(*this);

  return ParseDeclOrFunctionDefInternal(Attrs, DeclSpecAttrs, PDS, AS);
}
```

相关注释:

```c++
/// Parse either a function-definition or a declaration.  We can't tell which
/// we have until we read up to the compound-statement in function-definition.
/// TemplateParams, if non-NULL, provides the template parameters when we're
/// parsing a C++ template-declaration.
///
///       function-definition: [C99 6.9.1]
///         decl-specs      declarator declaration-list[opt] compound-statement
/// [C90] function-definition: [C99 6.7.1] - implicit int result
/// [C90]   decl-specs[opt] declarator declaration-list[opt] compound-statement
///
///       declaration: [C99 6.7]
///         declaration-specifiers init-declarator-list[opt] ';'
/// [!C99]  init-declarator-list ';'                   [TODO: warn in c99 mode]
/// [OMP]   threadprivate-directive
/// [OMP]   allocate-directive                         [TODO]
///
Parser::DeclGroupPtrTy Parser::ParseDeclOrFunctionDefInternal(
    ParsedAttributes &Attrs, ParsedAttributes &DeclSpecAttrs,
    ParsingDeclSpec &DS, AccessSpecifier AS) {
```

调用ParseDeclarationSpecifiers解析`int`:

```c++
MaybeParseMicrosoftAttributes(DS.getAttributes());
// Parse the common declaration-specifiers piece.
ParseDeclarationSpecifiers(DS, ParsedTemplateInfo(), AS,
                           DeclSpecContext::DSC_top_level);
```

调用ParseDeclGroup:

```c++
  }

  return ParseDeclGroup(DS, DeclaratorContext::File, Attrs);
}
```

相关注释:

```c++
/// ParseDeclGroup - Having concluded that this is either a function
/// definition or a group of object declarations, actually parse the
/// result.
Parser::DeclGroupPtrTy Parser::ParseDeclGroup(ParsingDeclSpec &DS,
                                              DeclaratorContext Context,
                                              ParsedAttributes &Attrs,
                                              SourceLocation *DeclEnd,
                                              ForRangeInit *FRI) {
```

调用ParseDeclarator解析`main()`

```c++
// Parse the first declarator.
// Consume all of the attributes from `Attrs` by moving them to our own local
// list. This ensures that we will not attempt to interpret them as statement
// attributes higher up the callchain.
ParsedAttributes LocalAttrs(AttrFactory);
LocalAttrs.takeAllFrom(Attrs);
ParsingDeclarator D(*this, DS, LocalAttrs, Context);
ParseDeclarator(D);
```

解析函数定义:

```c++
            // Recover by treating the 'typedef' as spurious.
            DS.ClearStorageClassSpecs();
          }

          Decl *TheDecl = ParseFunctionDefinition(D, ParsedTemplateInfo(),
                                                  &LateParsedAttrs);
          return Actions.ConvertDeclToDeclGroup(TheDecl);
        }
```

相关注释:

```c++
/// ParseFunctionDefinition - We parsed and verified that the specified
/// Declarator is well formed.  If this is a K&R-style function, read the
/// parameters declaration-list, then start the compound-statement.
///
///       function-definition: [C99 6.9.1]
///         decl-specs      declarator declaration-list[opt] compound-statement
/// [C90] function-definition: [C99 6.7.1] - implicit int result
/// [C90]   decl-specs[opt] declarator declaration-list[opt] compound-statement
/// [C++] function-definition: [C++ 8.4]
///         decl-specifier-seq[opt] declarator ctor-initializer[opt]
///         function-body
/// [C++] function-definition: [C++ 8.4]
///         decl-specifier-seq[opt] declarator function-try-block
///
Decl *Parser::ParseFunctionDefinition(ParsingDeclarator &D,
                                      const ParsedTemplateInfo &TemplateInfo,
                                      LateParsedAttrList *LateParsedAttrs) {
```

这里注意一下:

```c++
  // In delayed template parsing mode, for function template we consume the
  // tokens and store them for late parsing at the end of the translation unit.
  if (getLangOpts().DelayedTemplateParsing && Tok.isNot(tok::equal) &&
      TemplateInfo.Kind == ParsedTemplateInfo::Template &&
      Actions.canDelayFunctionBody(D)) {
```

可以看到ParseFunctionDefinition时, 也需要判断当前是否是函数模板, 以及是否开启了延后解析模式.

调用ParseFunctionStatementBody解析函数体(`return `):

```c++
  // Late attributes are parsed in the same scope as the function body.
  if (LateParsedAttrs)
    ParseLexedAttributeList(*LateParsedAttrs, Res, false, true);

  return ParseFunctionStatementBody(Res, BodyScope);
}
```

注意解析普通函数和member函数的区别, member函数是通过ParseCXXInlineMethodDef存储函数定义的token, 然后通过ParseLexedMethodDef->ParseFunctionStatementBody进行延后解析.

类似之前的流程, 调用ParseCompoundStatementBody, 消耗`{`. 来到这个while循环:

```
  while (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
         Tok.isNot(tok::eof)) {
    if (Tok.is(tok::annot_pragma_unused)) {
```

调用ParseStatementOrDeclaration->ParseStatementOrDeclarationAfterAttributes, 这次switch分支走identifier(`A`):

```c++
case tok::identifier:
  ParseIdentifier: {
    Token Next = NextToken();
    if (Next.is(tok::colon)) { // C99 6.8.1: labeled-statement
      // Both C++11 and GNU attributes preceding the label appertain to the
      // label, so put them in a single list to pass on to
      // ParseLabeledStatement().
      ParsedAttributes Attrs(AttrFactory);
```

这里有个很重要的步骤, 调用TryAnnotateName实现identifier与相关类型绑定:

```c++
    if (Next.isNot(tok::coloncolon)) {
      // Try to limit which sets of keywords should be included in typo
      // correction based on what the next token is.
      StatementFilterCCC CCC(Next);
      if (TryAnnotateName(&CCC) == ANK_Error) {
```

看一下注释:

```c++
/// Attempt to classify the name at the current token position. This may
/// form a type, scope or primary expression annotation, or replace the token
/// with a typo-corrected keyword. This is only appropriate when the current
/// name must refer to an entity which has already been declared.
///
/// \param CCC Indicates how to perform typo-correction for this name. If NULL,
///        no typo correction will be performed.
/// \param AllowImplicitTypename Whether we are in a context where a dependent
///        nested-name-specifier without typename is treated as a type (e.g.
///        T::type).
Parser::AnnotatedNameKind
Parser::TryAnnotateName(CorrectionCandidateCallback *CCC,
                        ImplicitTypenameContext AllowImplicitTypename) {
```

调用ParseOptionalCXXScopeSpecifier:

```c++
if (getLangOpts().CPlusPlus &&
    ParseOptionalCXXScopeSpecifier(SS, /*ObjectType=*/nullptr,
                                   /*ObjectHasErrors=*/false,
                                   EnteringContext))
```

相关注释:

```c++
/// Parse global scope or nested-name-specifier if present.
///
/// Parses a C++ global scope specifier ('::') or nested-name-specifier (which
/// may be preceded by '::'). Note that this routine will not parse ::new or
/// ::delete; it will just leave them in the token stream.
///
///       '::'[opt] nested-name-specifier
///       '::'
///
///       nested-name-specifier:
///         type-name '::'
///         namespace-name '::'
///         nested-name-specifier identifier '::'
///         nested-name-specifier 'template'[opt] simple-template-id '::'
///
///
/// \param SS the scope specifier that will be set to the parsed
/// nested-name-specifier (or empty)
///
/// \param ObjectType if this nested-name-specifier is being parsed following
/// the "." or "->" of a member access expression, this parameter provides the
/// type of the object whose members are being accessed.
///
/// \param ObjectHadErrors if this unqualified-id occurs within a member access
/// expression, indicates whether the original subexpressions had any errors.
/// When true, diagnostics for missing 'template' keyword will be supressed.
///
/// \param EnteringContext whether we will be entering into the context of
/// the nested-name-specifier after parsing it.
///
/// \param MayBePseudoDestructor When non-NULL, points to a flag that
/// indicates whether this nested-name-specifier may be part of a
/// pseudo-destructor name. In this case, the flag will be set false
/// if we don't actually end up parsing a destructor name. Moreover,
/// if we do end up determining that we are parsing a destructor name,
/// the last component of the nested-name-specifier is not parsed as
/// part of the scope specifier.
///
/// \param IsTypename If \c true, this nested-name-specifier is known to be
/// part of a type name. This is used to improve error recovery.
///
/// \param LastII When non-NULL, points to an IdentifierInfo* that will be
/// filled in with the leading identifier in the last component of the
/// nested-name-specifier, if any.
///
/// \param OnlyNamespace If true, only considers namespaces in lookup.
///
///
/// \returns true if there was an error parsing a scope specifier
bool Parser::ParseOptionalCXXScopeSpecifier(
```

这里开始解析identifier(当前是`A`, 下一个Token是`<`):

```c++
    // The rest of the nested-name-specifier possibilities start with
    // tok::identifier.
    if (Tok.isNot(tok::identifier))
      break;

    IdentifierInfo &II = *Tok.getIdentifierInfo();

    // nested-name-specifier:
    //   type-name '::'
    //   namespace-name '::'
    //   nested-name-specifier identifier '::'
    Token Next = NextToken();
    Sema::NestedNameSpecInfo IdInfo(&II, Tok.getLocation(), Next.getLocation(),
                                    ObjectType);
```

这里判断出是一个模板实例化类型:

```c++
// nested-name-specifier:
    //   type-name '<'
    if (Next.is(tok::less)) {

      TemplateTy Template;
      UnqualifiedId TemplateName;
      TemplateName.setIdentifier(&II, Tok.getLocation());
      bool MemberOfUnknownSpecialization;
```

判断是否为一个已知的模板:

```c++
      if (TemplateNameKind TNK = Actions.isTemplateName(getCurScope(), SS,
                                              /*hasTemplateKeyword=*/false,
                                                        TemplateName,
                                                        ObjectType,
                                                        EnteringContext,
                                                        Template,
                                              MemberOfUnknownSpecialization)) {
```

获取TName, 这里为`A`:

```c++
switch (Name.getKind()) {
case UnqualifiedIdKind::IK_Identifier:
  TName = DeclarationName(Name.Identifier);
  break;
```

接着我们用LookupTemplateName进行查找:

```c++
  LookupResult R(*this, TName, Name.getBeginLoc(), LookupOrdinaryName);
  if (LookupTemplateName(R, S, SS, ObjectType, EnteringContext,
                         MemberOfUnknownSpecialization, SourceLocation(),
                         &AssumedTemplate,
                         /*AllowTypoCorrection=*/!Disambiguation))
    return TNK_Non_template;
```

来到这个分支:

```c++
  if (SS.isEmpty() && (ObjectType.isNull() || Found.empty())) {
    // C++ [basic.lookup.classref]p1:
    //   In a class member access expression (5.2.5), if the . or -> token is
    //   immediately followed by an identifier followed by a <, the
    //   identifier must be looked up to determine whether the < is the
    //   beginning of a template argument list (14.2) or a less-than operator.
    //   The identifier is first looked up in the class of the object
    //   expression. If the identifier is not found, it is then looked up in
    //   the context of the entire postfix-expression and shall name a class
    //   template.
    if (S)
      LookupName(Found, S);
```

调用LookupName进行搜索, 相关注释:

```c++
/// Perform unqualified name lookup starting from a given
/// scope.
///
/// Unqualified name lookup (C++ [basic.lookup.unqual], C99 6.2.1) is
/// used to find names within the current scope. For example, 'x' in
/// @code
/// int x;
/// int f() {
///   return x; // unqualified name look finds 'x' in the global scope
/// }
/// @endcode
///
/// Different lookup criteria can find different names. For example, a
/// particular scope can have both a struct and a function of the same
/// name, and each can be found by certain lookup criteria. For more
/// information about lookup criteria, see the documentation for the
/// class LookupCriteria.
///
/// @param S        The scope from which unqualified name lookup will
/// begin. If the lookup criteria permits, name lookup may also search
/// in the parent scopes.
///
/// @param [in,out] R Specifies the lookup to perform (e.g., the name to
/// look up and the lookup kind), and is updated with the results of lookup
/// including zero or more declarations and possibly additional information
/// used to diagnose ambiguities.
///
/// @returns \c true if lookup succeeded and false otherwise.
bool Sema::LookupName(LookupResult &R, Scope *S, bool AllowBuiltinCreation,
                      bool ForceNoCPlusPlus) {
```

我们先来分析其两个重要的参数, `LoopkupResult`和`Scope`.

LookupResult相关注释:

```c++
/// Represents the results of name lookup.
///
/// An instance of the LookupResult class captures the results of a
/// single name lookup, which can return no result (nothing found),
/// a single declaration, a set of overloaded functions, or an
/// ambiguity. Use the getKind() method to determine which of these
/// results occurred for a given lookup.
class LookupResult {
```

可以看到Lookup主要是为了存储单个name查找的结果, 结果可以是未找到, 单一Decl, 一组重载的函数以及二义错误. 可以通过getKind()获取结果类型:

```c++
  enum LookupResultKind {
    /// No entity found met the criteria.
    NotFound = 0,

    /// No entity found met the criteria within the current
    /// instantiation,, but there were dependent base classes of the
    /// current instantiation that could not be searched.
    NotFoundInCurrentInstantiation,

    /// Name lookup found a single declaration that met the
    /// criteria.  getFoundDecl() will return this declaration.
    Found,

    /// Name lookup found a set of overloaded functions that
    /// met the criteria.
    FoundOverloaded,

    /// Name lookup found an unresolvable value declaration
    /// and cannot yet complete.  This only happens in C++ dependent
    /// contexts with dependent using declarations.
    FoundUnresolvedValue,

    /// Name lookup results in an ambiguity; use
    /// getAmbiguityKind to figure out what kind of ambiguity
    /// we have.
    Ambiguous
  };
```

再来看Scope, 在前面的分析中这个概念也是经常出现:

```c++
/// Scope - A scope is a transient data structure that is used while parsing the
/// program.  It assists with resolving identifiers to the appropriate
/// declaration.
class Scope {
```

注释说Scope是个临时生成的数据结构, 用于辅助将identifier解析到对应Decl.

注意Scope和DeclContext的关系:

```c++
  /// The DeclContext with which this scope is associated. For
  /// example, the entity of a class scope is the class itself, the
  /// entity of a function scope is a function, etc.
  DeclContext *Entity;
```

scope是比DeclContext更高层的概念, 符号的查找也主要是由其辅助实现的.

继续来看LookupName, 接下来调用CppLookupName进行处理:

```c++
    // Perform C++ unqualified name lookup.
    if (CppLookupName(R, S))
      return true;
  }
```

在CppLookupName中, 首先在local scope中搜索:

```c++
// First we lookup local scope.
  // We don't consider using-directives, as per 7.3.4.p1 [namespace.udir]
  // ...During unqualified name lookup (3.4.1), the names appear as if
  // they were declared in the nearest enclosing namespace which contains
  // both the using-directive and the nominated namespace.
  // [Note: in this context, "contains" means "contains directly or
  // indirectly".
  //
  // For example:
  // namespace A { int i; }
  // void foo() {
  //   int i;
  //   {
  //     using namespace A;
  //     ++i; // finds local 'i', A::i appears at global scope
  //   }
  // }
  //
  UnqualUsingDirectiveSet UDirs(*this);
  bool VisitedUsingDirectives = false;
  bool LeftStartingScope = false;
```

对于using的相关解释可以参考cpp标准手册相关章节:https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/n4594.pdf

不过当前所需的`A`的声明并不在local scope中, 所以接下来会到global scope中搜索:

```c++
// Lookup namespace scope, and global scope.
// Unqualified name lookup in C++ requires looking into scopes
// that aren't strictly lexical, and therefore we walk through the
// context as well as walking through the scopes.
for (; S; S = S->getParent()) {
```

注意这里`S`已经是最顶层, 所以相当于是在`S`内部, 也就是整个程序的顶层空间中搜索.

检测所有toplevel decl, 这里会遍历到类模板`A` (目前也只有这一个toplevel decl), 标记Found为true, 并将相关Decl加入LookupResult R中:

```c++
// Check whether the IdResolver has anything in this scope.
bool Found = false;
for (; I != IEnd && S->isDeclScope(*I); ++I) {
  if (NamedDecl *ND = R.getAcceptableDecl(*I)) {
    // We found something.  Look for anything else in our scope
    // with this same name and in an acceptable identifier
    // namespace, so that we can construct an overload set if we
    // need to.
    Found = true;
    R.addDecl(ND);
  }
}
```

```shell
$2 = (clang::Scope *) 0x55555561f730
(gdb) print ND
$3 = (clang::ClassTemplateDecl *) 0x5555556ac4d0
(gdb) print ND->getNameAsString()
$4 = "A"
```

这里我们来关注`I`, 这是一个`IdentifierResolver`, 相关注释如下:

```c++
/// IdentifierResolver - Keeps track of shadowed decls on enclosing
/// scopes.  It manages the shadowing chains of declaration names and
/// implements efficient decl lookup based on a declaration name.
class IdentifierResolver {
  /// IdDeclInfo - Keeps track of information about decls associated
  /// to a particular declaration name. IdDeclInfos are lazily
  /// constructed and assigned to a declaration name the first time a
  /// decl with that declaration name is shadowed in some scope.
  class IdDeclInfo {
```

可以看到IdentifierResolver会借助IdDeclInfo持续追踪每个DeclarationName相关的Decl

<font color="red">这个和Scope配合起来用就相当于clang的符号表, 体现在代码上就是S->isDeclScope(*I), 用Scope来限制当前IdentifierResolver对Decl迭代的作用域. </font>

接下来在相关Context中搜索 ,并考虑using (CppNamespaceLookup):

```c++
    DeclContext *Ctx = S->getLookupEntity();
    if (Ctx) {
      DeclContext *OuterCtx = findOuterContext(S);
      for (; Ctx && !Ctx->Equals(OuterCtx); Ctx = Ctx->getLookupParent()) {
        // We do not directly look into transparent contexts, since
        // those entities will be found in the nearest enclosing
        // non-transparent context.
        if (Ctx->isTransparentContext())
          continue;

        // If we have a context, and it's not a context stashed in the
        // template parameter scope for an out-of-line definition, also
        // look into that context.
        if (!(Found && S->isTemplateParamScope())) {
          assert(Ctx->isFileContext() &&
              "We should have been looking only at file context here already.");

          // Look into context considering using-directives.
          if (CppNamespaceLookup(*this, R, Context, Ctx, UDirs))
            Found = true;
        }
```

CppNamespaceLookup -> LookupDirect

```c++
// Performs C++ unqualified lookup into the given file context.
static bool
CppNamespaceLookup(Sema &S, LookupResult &R, ASTContext &Context,
                   DeclContext *NS, UnqualUsingDirectiveSet &UDirs) {

  assert(NS && NS->isFileContext() && "CppNamespaceLookup() requires namespace!");

  // Perform direct name lookup into the LookupCtx.
  bool Found = LookupDirect(S, R, NS);
```

这里还是能找到类模板`A`, 注意R中相关数据结构是set, 会自动去重:

```c++
// Adds all qualifying matches for a name within a decl context to the
// given lookup result.  Returns true if any matches were found.
static bool LookupDirect(Sema &S, LookupResult &R, const DeclContext *DC) {
  bool Found = false;

  // Lazily declare C++ special member functions.
  if (S.getLangOpts().CPlusPlus)
    DeclareImplicitMemberFunctionsWithName(S, R.getLookupName(), R.getNameLoc(),
                                           DC);

  // Perform lookup into this declaration context.
  DeclContext::lookup_result DR = DC->lookup(R.getLookupName());
  for (NamedDecl *D : DR) {
    if ((D = R.getAcceptableDecl(D))) {
      R.addDecl(D);
      Found = true;
    }
  }
```

这里return Found(true):

```c++
  if (R.getLookupName().getNameKind()
        != DeclarationName::CXXConversionFunctionName ||
      R.getLookupName().getCXXNameType()->isDependentType() ||
      !isa<CXXRecordDecl>(DC))
    return Found;
```

回到CppNamespaceLookup, 调用resolveKind解析结果类型, 这里为LookupResult::Found:
```c++
  R.resolveKind();

  return Found;
}
```

回到CppLookupName, return true:

```c++
          // Look into context considering using-directives.
          if (CppNamespaceLookup(*this, R, Context, Ctx, UDirs))
            Found = true;
        }

        if (Found) {
          R.resolveKind();
          return true;
        }
```

回到LookupName, return true:

```c++
    // Perform C++ unqualified name lookup.
    if (CppLookupName(R, S))
      return true;
  }
```

回到LookupTemplateName, 最终会return false(表示正常)

```c++
  }

  return false;
}
```

回到isTemplateName, 这里拿到解析出来的TemplateDecl:

```
    TemplateDecl *TD = cast<TemplateDecl>(D);
    Template =
        FoundUsingShadow ? TemplateName(FoundUsingShadow) : TemplateName(TD);
    assert(!FoundUsingShadow || FoundUsingShadow->getTargetDecl() == TD);
```

return TemplateKind, 这里为TNK_Type_template.

回到ParseOptionalCXXScopeSpecifier, 消费当前Token(`A`), 并调用AnnotateTemplateIdToken向token buffer中插入新的annotate token:

```c++
        // We have found a template name, so annotate this token
        // with a template-id annotation. We do not permit the
        // template-id to be translated into a type annotation,
        // because some clients (e.g., the parsing of class template
        // specializations) still want to see the original template-id
        // token, and it might not be a type at all (e.g. a concept name in a
        // type-constraint).
        ConsumeToken();
        if (AnnotateTemplateIdToken(Template, TNK, SS, SourceLocation(),
                                    TemplateName, false))
          return true;
```

AnnotateTemplateIdToken相关注释:

```c++
/// Replace the tokens that form a simple-template-id with an
/// annotation token containing the complete template-id.
///
/// The first token in the stream must be the name of a template that
/// is followed by a '<'. This routine will parse the complete
/// simple-template-id and replace the tokens with a single annotation
/// token with one of two different kinds: if the template-id names a
/// type (and \p AllowTypeAnnotation is true), the annotation token is
/// a type annotation that includes the optional nested-name-specifier
/// (\p SS). Otherwise, the annotation token is a template-id
/// annotation that does not include the optional
/// nested-name-specifier.
///
/// \param Template  the declaration of the template named by the first
/// token (an identifier), as returned from \c Action::isTemplateName().
///
/// \param TNK the kind of template that \p Template
/// refers to, as returned from \c Action::isTemplateName().
///
/// \param SS if non-NULL, the nested-name-specifier that precedes
/// this template name.
///
/// \param TemplateKWLoc if valid, specifies that this template-id
/// annotation was preceded by the 'template' keyword and gives the
/// location of that keyword. If invalid (the default), then this
/// template-id was not preceded by a 'template' keyword.
///
/// \param AllowTypeAnnotation if true (the default), then a
/// simple-template-id that refers to a class template, template
/// template parameter, or other template that produces a type will be
/// replaced with a type annotation token. Otherwise, the
/// simple-template-id is always replaced with a template-id
/// annotation token.
///
/// \param TypeConstraint if true, then this is actually a type-constraint,
/// meaning that the template argument list can be omitted (and the template in
/// question must be a concept).
///
/// If an unrecoverable parse error occurs and no annotation token can be
/// formed, this function returns true.
///
bool Parser::AnnotateTemplateIdToken(TemplateTy Template, TemplateNameKind TNK,
                                     CXXScopeSpec &SS,
                                     SourceLocation TemplateKWLoc,
                                     UnqualifiedId &TemplateName,
                                     bool AllowTypeAnnotation,
                                     bool TypeConstraint) {
```

先来调用ParseTemplateIdAfterTemplateName解析尖括号内容:

```c++
// Parse the enclosed template argument list.
SourceLocation LAngleLoc, RAngleLoc;
TemplateArgList TemplateArgs;
bool ArgsInvalid = false;
if (!TypeConstraint || Tok.is(tok::less)) {
  ArgsInvalid = ParseTemplateIdAfterTemplateName(
      false, LAngleLoc, TemplateArgs, RAngleLoc, Template);
  // If we couldn't recover from invalid arguments, don't form an annotation
  // token -- we don't know how much to annotate.
  // FIXME: This can lead to duplicate diagnostics if we retry parsing this
  // template-id in another context. Try to annotate anyway?
  if (RAngleLoc.isInvalid())
    return true;
}
```

相关注释:

```c++
/// Parses a template-id that after the template name has
/// already been parsed.
///
/// This routine takes care of parsing the enclosed template argument
/// list ('<' template-parameter-list [opt] '>') and placing the
/// results into a form that can be transferred to semantic analysis.
///
/// \param ConsumeLastToken if true, then we will consume the last
/// token that forms the template-id. Otherwise, we will leave the
/// last token in the stream (e.g., so that it can be replaced with an
/// annotation token).
bool Parser::ParseTemplateIdAfterTemplateName(bool ConsumeLastToken,
                                              SourceLocation &LAngleLoc,
                                              TemplateArgList &TemplateArgs,
                                              SourceLocation &RAngleLoc,
                                              TemplateTy Template) {
```

消费`<`:

```c++
// Consume the '<'.
LAngleLoc = ConsumeToken();
```

解析模板参数列表:

```c++
  // Parse the optional template-argument-list.
  bool Invalid = false;
  {
    GreaterThanIsOperatorScope G(GreaterThanIsOperator, false);
    if (!Tok.isOneOf(tok::greater, tok::greatergreater,
                     tok::greatergreatergreater, tok::greaterequal,
                     tok::greatergreaterequal))
      Invalid = ParseTemplateArgumentList(TemplateArgs, Template, LAngleLoc);
```

相关注释:

```c++
/// ParseTemplateArgumentList - Parse a C++ template-argument-list
/// (C++ [temp.names]). Returns true if there was an error.
///
///       template-argument-list: [C++ 14.2]
///         template-argument
///         template-argument-list ',' template-argument
///
/// \param Template is only used for code completion, and may be null.
bool Parser::ParseTemplateArgumentList(TemplateArgList &TemplateArgs,
                                       TemplateTy Template,
                                       SourceLocation OpenLoc) {
```

主体是个do while循环:

```c++
do {
...
} while (TryConsumeToken(tok::comma));
```

调用ParseTemplateArgument解析单个参数:

```c++
  do {
    PreferredType.enterFunctionArgument(Tok.getLocation(), RunSignatureHelp);
    ParsedTemplateArgument Arg = ParseTemplateArgument();
```

相关注释:

```c++
/// ParseTemplateArgument - Parse a C++ template argument (C++ [temp.names]).
///
///       template-argument: [C++ 14.2]
///         constant-expression
///         type-id
///         id-expression
ParsedTemplateArgument Parser::ParseTemplateArgument() {
```

调用ParseTypeName进行解析:

```c++
EnterExpressionEvaluationContext EnterConstantEvaluated(
  Actions, Sema::ExpressionEvaluationContext::ConstantEvaluated,
  /*LambdaContextDecl=*/nullptr,
  /*ExprContext=*/Sema::ExpressionEvaluationContextRecord::EK_TemplateArgument);
if (isCXXTypeId(TypeIdAsTemplateArgument)) {
  TypeResult TypeArg = ParseTypeName(
      /*Range=*/nullptr, DeclaratorContext::TemplateArg);
  return Actions.ActOnTemplateTypeArgument(TypeArg);
}
```

相关注释:

```c++
/// ParseTypeName
///       type-name: [C99 6.7.6]
///         specifier-qualifier-list abstract-declarator[opt]
///
/// Called type-id in C++.
TypeResult Parser::ParseTypeName(SourceRange *Range, DeclaratorContext Context,
                                 AccessSpecifier AS, Decl **OwnedType,
                                 ParsedAttributes *Attrs) {
```

调用ParseSpecifierQualifierList进行解析:

```c++
// Parse the common declaration-specifiers piece.
DeclSpec DS(AttrFactory);
if (Attrs)
  DS.addAttributes(*Attrs);
ParseSpecifierQualifierList(DS, AS, DSC);
```

相关注释:

```c++
/// ParseSpecifierQualifierList
///        specifier-qualifier-list:
///          type-specifier specifier-qualifier-list[opt]
///          type-qualifier specifier-qualifier-list[opt]
/// [GNU]    attributes     specifier-qualifier-list[opt]
///
void Parser::ParseSpecifierQualifierList(
    DeclSpec &DS, ImplicitTypenameContext AllowImplicitTypename,
    AccessSpecifier AS, DeclSpecContext DSC) {
```

调用ParseDeclarationSpecifiers进行解析:

```c++
ParseDeclarationSpecifiers(DS, ParsedTemplateInfo(), AS, DSC, nullptr,
                           AllowImplicitTypename);
```

这个方法我们在之前已经遇到过好多次了, 这次来到swith的int分支:

```c++
    case tok::kw_int:
      isInvalid = DS.SetTypeSpecType(DeclSpec::TST_int, Loc, PrevSpec,
                                     DiagID, Policy);
      break;
```

break后来到函数末尾, 消耗掉`int`:
```c++
    if (DiagID != diag::err_bool_redeclaration && ConsumedEnd.isInvalid())
      // After an error the next token can be an annotation token.
      ConsumeAnyToken();

    AttrsLastTime = false;
  }
}
```

第二次循环没什么可执行的, 返回到ParseSpecifierQualifierList, 也没什么可执行的, 返回ParseTypeName, 这里当前token为`>`, ParseDeclarator没什么可执行的:

```c++
// Parse the abstract-declarator, if present.
  Declarator DeclaratorInfo(DS, ParsedAttributesView::none(), Context);
  ParseDeclarator(DeclaratorInfo);
  if (Range)
    *Range = DeclaratorInfo.getSourceRange();
```

返回ParseTemplateArgument, 没什么可执行的, 返回ParseTemplateArgumentList, 保存模板参数:

```c++
    // Save this template argument.
    TemplateArgs.push_back(Arg);
```

由于当前token不是`,`, 退出while循环, 返回到ParseTemplateIdAfterTemplateName, 调用ParseGreaterThanInTemplateList:

```c++
return ParseGreaterThanInTemplateList(LAngleLoc, RAngleLoc, ConsumeLastToken,
                                      /*ObjCGenericList=*/false) ||
       Invalid;
```

相关注释:

```c++
/// Parses a '>' at the end of a template list.
///
/// If this function encounters '>>', '>>>', '>=', or '>>=', it tries
/// to determine if these tokens were supposed to be a '>' followed by
/// '>', '>>', '>=', or '>='. It emits an appropriate diagnostic if necessary.
///
/// \param RAngleLoc the location of the consumed '>'.
///
/// \param ConsumeLastToken if true, the '>' is consumed.
///
/// \param ObjCGenericList if true, this is the '>' closing an Objective-C
/// type parameter or type argument list, rather than a C++ template parameter
/// or argument list.
///
/// \returns true, if current token does not start with '>', false otherwise.
bool Parser::ParseGreaterThanInTemplateList(SourceLocation LAngleLoc,
                                            SourceLocation &RAngleLoc,
                                            bool ConsumeLastToken,
                                            bool ObjCGenericList) {
```

这里进入swith的`>`分支,不注意这里并不会消耗掉`>`(为了方便接下来替换):

```c++
  switch (Tok.getKind()) {
  default:
    Diag(getEndOfPreviousToken(), diag::err_expected) << tok::greater;
    Diag(LAngleLoc, diag::note_matching) << tok::less;
    return true;

  case tok::greater:
    // Determine the location of the '>' token. Only consume this token
    // if the caller asked us to.
    RAngleLoc = Tok.getLocation();
    if (ConsumeLastToken)
      ConsumeToken();
    return false;
```

返回ParseTemplateIdAfterTemplateName, 返回AnnotateTemplateIdToken, 这里我们将当前的`>`替换为`tok::annot_template_id`:

```c++
// Build a template-id annotation token that can be processed
// later.
Tok.setKind(tok::annot_template_id);
```

返回到ParseOptionalCXXScopeSpecifier, 进行第二次循环, break:

```c++
    // The rest of the nested-name-specifier possibilities start with
    // tok::identifier.
    if (Tok.isNot(tok::identifier))
      break;
```

返回到TryAnnotateName, 这里调用TryAnnotateTypeOrScopeTokenAfterScopeSpec尝试继续细化annot_template_id:

```c++
  if (Tok.isNot(tok::identifier) || SS.isInvalid()) {
    if (TryAnnotateTypeOrScopeTokenAfterScopeSpec(SS, !WasScopeAnnotation,
                                                  AllowImplicitTypename))
      return ANK_Error;
    return ANK_Unresolved;
  }
```

相关注释:

```c++
/// Try to annotate a type or scope token, having already parsed an
/// optional scope specifier. \p IsNewScope should be \c true unless the scope
/// specifier was extracted from an existing tok::annot_cxxscope annotation.
bool Parser::TryAnnotateTypeOrScopeTokenAfterScopeSpec(
```

可以看到这里尝试把一个template_id细化为typename

来到这个分支, 通过AnnotateTemplateIdTokenAsType细化token:

```c++
if (Tok.is(tok::annot_template_id)) {
  TemplateIdAnnotation *TemplateId = takeTemplateIdAnnotation(Tok);
  if (TemplateId->Kind == TNK_Type_template) {
    // A template-id that refers to a type was parsed into a
    // template-id annotation in a context where we weren't allowed
    // to produce a type annotation token. Update the template-id
    // annotation token to a type annotation token now.
    AnnotateTemplateIdTokenAsType(SS, AllowImplicitTypename);
    return false;
  }
}
```

相关注释:

```c++
/// Replaces a template-id annotation token with a type
/// annotation token.
///
/// If there was a failure when forming the type from the template-id,
/// a type annotation token will still be created, but will have a
/// NULL type pointer to signify an error.
///
/// \param SS The scope specifier appearing before the template-id, if any.
///
/// \param AllowImplicitTypename whether this is a context where T::type
/// denotes a dependent type.
/// \param IsClassName Is this template-id appearing in a context where we
/// know it names a class, such as in an elaborated-type-specifier or
/// base-specifier? ('typename' and 'template' are unneeded and disallowed
/// in those contexts.)
void Parser::AnnotateTemplateIdTokenAsType(
```

在这里把Token替换为`clang::tok::annot_typename`:

```c++
// Create the new "type" annotation token.
Tok.setKind(tok::annot_typename);
setTypeAnnotation(Tok, Type);
```

回到TryAnnotateTypeOrScopeTokenAfterScopeSpec, return false, 回到TryAnnotateName, return ANK_Unresolved, 回到ParseStatementOrDeclarationAfterAttributes, 跳转到Retry:

```c++
      // If the identifier was typo-corrected, try again.
      if (Tok.isNot(tok::identifier))
        goto Retry;
```

第二次while swith时, 走default分支, 调用ParseDeclaration:

```
      } else {
        Decl = ParseDeclaration(DeclaratorContext::Block, DeclEnd, CXX11Attrs,
                                GNUAttrs);
      }
```

这个函数我们之前也解析过, 这里快速过一下.

switch走default分支, 调用ParseSimpleDeclaration:

```c++
default:
  return ParseSimpleDeclaration(Context, DeclEnd, DeclAttrs, DeclSpecAttrs,
                                true, nullptr, DeclSpecStart);
}
```

相关注释:

```c++
///       simple-declaration: [C99 6.7: declaration] [C++ 7p1: dcl.dcl]
///         declaration-specifiers init-declarator-list[opt] ';'
/// [C++11] attribute-specifier-seq decl-specifier-seq[opt]
///             init-declarator-list ';'
///[C90/C++]init-declarator-list ';'                             [TODO]
/// [OMP]   threadprivate-directive
/// [OMP]   allocate-directive                                   [TODO]
///
///       for-range-declaration: [C++11 6.5p1: stmt.ranged]
///         attribute-specifier-seq[opt] type-specifier-seq declarator
///
/// If RequireSemi is false, this does not check for a ';' at the end of the
/// declaration.  If it is true, it checks for and eats it.
///
/// If FRI is non-null, we might be parsing a for-range-declaration instead
/// of a simple-declaration. If we find that we are, we also parse the
/// for-range-initializer, and place it here.
///
/// DeclSpecStart is used when decl-specifiers are parsed before parsing
/// the Declaration. The SourceLocation for this Decl is set to
/// DeclSpecStart if DeclSpecStart is non-null.
Parser::DeclGroupPtrTy Parser::ParseSimpleDeclaration(
    DeclaratorContext Context, SourceLocation &DeclEnd,
    ParsedAttributes &DeclAttrs, ParsedAttributes &DeclSpecAttrs,
    bool RequireSemi, ForRangeInit *FRI, SourceLocation *DeclSpecStart) {
```

调用ParseDeclarationSpecifiers:

```c++
DeclSpecContext DSContext = getDeclSpecContextFromDeclaratorContext(Context);
ParseDeclarationSpecifiers(DS, ParsedTemplateInfo(), AS_none, DSContext);
```

switch分支走annot_typename, 将当前typename token消耗掉, 露出identifier token:

```c++
   case tok::annot_typename: {
      // If we've previously seen a tag definition, we were almost surely
      // missing a semicolon after it.
      if (DS.hasTypeSpecifier() && DS.hasTagDefinition())
        goto DoneWithDeclSpec;

      TypeResult T = getTypeAnnotation(Tok);
      isInvalid = DS.SetTypeSpecType(DeclSpec::TST_typename, Loc, PrevSpec,
                                     DiagID, T, Policy);
      if (isInvalid)
        break;

      DS.SetRangeEnd(Tok.getAnnotationEndLoc());
      ConsumeAnnotationToken(); // The typename

      continue;
    }
```

第二次while switch, 走identifier分支, 调到DoneWithDeclSpec, 然后return回ParseSimpleDeclaration, 调用ParseDeclGroup:

```c++
  return ParseDeclGroup(DS, Context, DeclAttrs, &DeclEnd, FRI);
}
```

调用ParseDeclarator:

```c++
ParsingDeclarator D(*this, DS, LocalAttrs, Context);
ParseDeclarator(D);
```

这个函数之前也分析过, 这里会消费`a`, 具体过程略过. 

返回ParseDeclGroup, 这里把`;`消费掉:

```c++
  if (ExpectSemi && ExpectAndConsumeSemi(
                        Context == DeclaratorContext::File
                            ? diag::err_invalid_token_after_toplevel_declarator
                            : diag::err_expected_semi_declaration)) {
```

返回ParseSimpleDeclaration, 返回ParseDeclaration, 返回ParseStatementOrDeclarationAfterAttributes, 返回ParseStatementOrDeclaration, 返回ParseCompoundStatementBody, 将当前语句加入`Stmts`:

```c++
if (R.isUsable())
      Stmts.push_back(R.get());
```

接下来进行第二次while循环, 解析main函数的第二条语句, 调用ParseStatementOrDeclaration, 调用ParseStatementOrDeclarationAfterAttributes, switch分支走return, 调用ParseReturnStatement, 消费`return`, 调用ParseExpression, 调用ParseAssignmentExpression, 调用ParseCastExpression, 来到switch, 走identifier分支,

<font color="red">这里我们主要关心`a.test(1)`是如何解析到对应的局部对象以及成员函数上的, 以及相应的成员函数是如何实例化的.</font>

消费当前identifier `a`:

```c++
    // Consume the identifier so that we can see if it is followed by a '(' or
    // '.'.
    IdentifierInfo &II = *Tok.getIdentifierInfo();
    SourceLocation ILoc = ConsumeToken();
```

调用ParsePostfixExpressionSuffix解析a后面的`.test(1)`:

```c++
  // These can be followed by postfix-expr pieces.
  PreferredType = SavedType;
  Res = ParsePostfixExpressionSuffix(Res);
  if (getLangOpts().OpenCL &&
      !getActions().getOpenCLOptions().isAvailableOption(
```

相关注释:

```c++
/// Once the leading part of a postfix-expression is parsed, this
/// method parses any suffixes that apply.
///
/// \verbatim
///       postfix-expression: [C99 6.5.2]
///         primary-expression
///         postfix-expression '[' expression ']'
///         postfix-expression '[' braced-init-list ']'
///         postfix-expression '[' expression-list [opt] ']'  [C++2b 12.4.5]
///         postfix-expression '(' argument-expression-list[opt] ')'
///         postfix-expression '.' identifier
///         postfix-expression '->' identifier
///         postfix-expression '++'
///         postfix-expression '--'
///         '(' type-name ')' '{' initializer-list '}'
///         '(' type-name ')' '{' initializer-list ',' '}'
///
///       argument-expression-list: [C99 6.5.2]
///         argument-expression ...[opt]
///         argument-expression-list ',' assignment-expression ...[opt]
/// \endverbatim
ExprResult
Parser::ParsePostfixExpressionSuffix(ExprResult LHS) {
```

来到解析`.`的分支, 消费`.`:

```c++
case tok::period: {
  // postfix-expression: p-e '->' template[opt] id-expression
  // postfix-expression: p-e '.' template[opt] id-expression
  tok::TokenKind OpKind = Tok.getKind();
  SourceLocation OpLoc = ConsumeToken();  // Eat the "." or "->" token.

  CXXScopeSpec SS;
```

调用ParseUnqualifiedId解析`test`:

```c++
  Name.setIdentifier(Id, Loc);
} else if (ParseUnqualifiedId(
               SS, ObjectType, LHS.get() && LHS.get()->containsErrors(),
               /*EnteringContext=*/false,
               /*AllowDestructorName=*/true,
               /*AllowConstructorName=*/
               getLangOpts().MicrosoftExt && SS.isNotEmpty(),
               /*AllowDeductionGuide=*/false, &TemplateKWLoc, Name)) {
  (void)Actions.CorrectDelayedTyposInExpr(LHS);
  LHS = ExprError();
}
```

消费`test`:

```c++
  // unqualified-id:
  //   identifier
  //   template-id (when it hasn't already been annotated)
  if (Tok.is(tok::identifier)) {
  ParseIdentifier:
    // Consume the identifier.
    IdentifierInfo *Id = Tok.getIdentifierInfo();
    SourceLocation IdLoc = ConsumeToken();

```

返回ParsePostfixExpressionSuffix, 进入第二次while循环, 这次走`(`分支:

```c++
    case tok::l_paren:         // p-e: p-e '(' argument-expression-list[opt] ')'
    case tok::lesslessless: {  // p-e: p-e '<<<' argument-expression-list '>>>'
                               //   '(' argument-expression-list[opt] ')'
      tok::TokenKind OpKind = Tok.getKind();
      InMessageExpressionRAIIObject InMessage(*this, false);

      Expr *ExecConfig = nullptr;
```

消费`(`:

```c++
      } else {
        PT.consumeOpen();
        Loc = PT.getOpenLocation();
      }
```

调用ParseExpressionList解析括号内的内容(实参列表):

```c++
      if (OpKind == tok::l_paren || !LHS.isInvalid()) {
        if (Tok.isNot(tok::r_paren)) {
          if (ParseExpressionList(ArgExprs, [&] {
                PreferredType.enterFunctionArgument(Tok.getLocation(),
                                                    RunSignatureHelp);
              })) {
```

相关注释:

```c++
/// ParseExpressionList - Used for C/C++ (argument-)expression-list.
///
/// \verbatim
///       argument-expression-list:
///         assignment-expression
///         argument-expression-list , assignment-expression
///
/// [C++] expression-list:
/// [C++]   assignment-expression
/// [C++]   expression-list , assignment-expression
///
/// [C++0x] expression-list:
/// [C++0x]   initializer-list
///
/// [C++0x] initializer-list
/// [C++0x]   initializer-clause ...[opt]
/// [C++0x]   initializer-list , initializer-clause ...[opt]
///
/// [C++0x] initializer-clause:
/// [C++0x]   assignment-expression
/// [C++0x]   braced-init-list
/// \endverbatim
bool Parser::ParseExpressionList(SmallVectorImpl<Expr *> &Exprs,
                                 llvm::function_ref<void()> ExpressionStarts,
                                 bool FailImmediatelyOnInvalidExpr,
                                 bool EarlyTypoCorrection) {
```

调用ParseAssignmentExpression解析参数:

```c++
} else
  Expr = ParseAssignmentExpression();
```

调用ParseCastExpression:

```c++
ExprResult LHS = ParseCastExpression(AnyCastExpr,
                                     /*isAddressOfOperand=*/false,
                                     isTypeCast);
```

走常数分支,消耗`1`:

```c++
    // primary-expression
  case tok::numeric_constant:
    // constant: integer-constant
    // constant: floating-constant

    Res = Actions.ActOnNumericConstant(Tok, /*UDLScope*/getCurScope());
    ConsumeToken();
    break;
```

返回ParseAssignmentExpression, 返回ParseExpressionList, 返回ParsePostfixExpressionSuffix, 消费`)`:

```c++
} else {
  Expr *Fn = LHS.get();
  SourceLocation RParLoc = Tok.getLocation();
  LHS = Actions.ActOnCallExpr(getCurScope(), Fn, Loc, ArgExprs, RParLoc,
                              ExecConfig);
  if (LHS.isInvalid()) {
    ArgExprs.insert(ArgExprs.begin(), Fn);
    LHS =
        Actions.CreateRecoveryExpr(Fn->getBeginLoc(), RParLoc, ArgExprs);
  }
  PT.consumeClose();
}
```

返回ParseCastExpression, 返回ParseAssignmentExpression, 返回ParseExpression, 返回ParseReturnStatement, 返回ParseStatementOrDeclarationAfterAttributes, 消费`;`:

```c++
  // If we reached this code, the statement must end in a semicolon.
  if (!TryConsumeToken(tok::semi) && !Res.isInvalid()) {
    // If the result was valid, then we do want to diagnose this.  Use
    // ExpectAndConsume to emit the diagnostic, even though we know it won't
    // succeed.
    ExpectAndConsume(tok::semi, diag::err_expected_semi_after_stmt, SemiError);
    // Skip until we see a } or ;, but don't eat it.
    SkipUntil(tok::r_brace, StopAtSemi | StopBeforeMatch);
  }
```

返回ParseStatementOrDeclaration, 返回ParseCompoundStatementBody, 此时compound body中已经没有其他语句, 退出while循环, 然后消费掉`}`:

```c++
// We broke out of the while loop because we found a '}' or EOF.
if (!T.consumeClose()) {
  // If this is the '})' of a statement expression, check that it's written
  // in a sensible way.
  if (isStmtExpr && Tok.is(tok::r_paren))
    checkCompoundToken(CloseLoc, tok::r_brace, CompoundToken::StmtExprEnd);
```

此时我们终于达到了`eof`.

返回ParseFunctionStatementBody, 返回ParseFunctionDefinition, 返回ParseDeclGroup, 返回ParseDeclOrFunctionDefInternal, 返回ParseDeclarationOrFunctionDefinition, 返回ParseExternalDeclaration, 返回ParseTopLevelDecl.

接着来到最后一次ParseTopLevelDecl, 处理eof:

```c++
  case tok::eof:
    // Check whether -fmax-tokens= was reached.
    if (PP.getMaxTokens() != 0 && PP.getTokenCount() > PP.getMaxTokens()) {
      PP.Diag(Tok.getLocation(), diag::warn_max_tokens_total)
          << PP.getTokenCount() << PP.getMaxTokens();
      SourceLocation OverrideLoc = PP.getMaxTokensOverrideLoc();
      if (OverrideLoc.isValid()) {
        PP.Diag(OverrideLoc, diag::note_max_tokens_total_override);
      }
    }

    // Late template parsing can begin.
    Actions.SetLateTemplateParser(LateTemplateParserCallback, nullptr, this);
    Actions.ActOnEndOfTranslationUnit();
    //else don't tell Sema that we ended parsing: more input might come.
    return true;
```

隐式实例化是在ActOnEndOfTranslationUnit中进行的, 我们稍后调试.

## 总结

```c++
ParseTopLevelDecl
|ParseExternalDeclaration
||ParseDeclaration
|||ParseDeclarationStartingWithTemplate
||||ParseTemplateDeclarationOrSpecialization
|||||consume 'template'
|||||ParseTemplateParameters
||||||consume '<'
||||||ParseTemplateParameterList
|||||||ParseTemplateParameter
||||||||ParseTypeParameter
|||||||||consume 'typename'
|||||||||consume 'T'
||||||consume '>'
|||||ParseSingleDeclarationAfterTemplate
||||||ParseDeclarationSpecifiers
|||||||consume 'class'
|||||||ParseClassSpecifier
||||||||consume 'A'
||||||||ParseCXXMemberSpecification
|||||||||consume '{'
|||||||||ParseCXXClassMemberDeclarationWithPragmas
||||||||||consume 'public'
|||||||||ParseCXXClassMemberDeclarationWithPragmas
||||||||||ParseCXXClassMemberDeclaration
|||||||||||ParseDeclarationSpecifiers
||||||||||||consume 'T'
|||||||||||ParseCXXMemberDeclaratorBeforeInitializer
||||||||||||ParseDeclarator
|||||||||||||ParseDeclaratorInternal
||||||||||||||ParseDirectDeclarator
|||||||||||||||ParseUnqualifiedId
||||||||||||||||consume 'test'
|||||||||||||||consume '('
|||||||||||||||ParseFunctionDeclarator
||||||||||||||||ParseParameterDeclarationClause
|||||||||||||||||ParseDeclarationSpecifiers
||||||||||||||||||... consume 'int'
|||||||||||||||||ParseDeclarator
||||||||||||||||||... consmue 'x'
||||||||||||||||consume ')'
|||||||||||ParseCXXInlineMethodDef
||||||||||||ConsumeAndStoreFunctionPrologue
|||||||||||||consume '{'
||||||||||||ConsumeAndStoreUntil
|||||||||||||consume until '}'
|||||||consume '}'
|||||||ParseLexedMethodDefs
||||||||ParseLexedMethodDef
|||||||||ParseFunctionStatementBody
||||||||||ParseCompoundStatementBody
|||||||||||consume '{'
||||||||||||ParseStatementOrDeclaration
|||||||||||||ParseStatementOrDeclarationAfterAttributes
||||||||||||||ParseReturnStatement
|||||||||||||||consume 'return'
|||||||||||||||ParseExpression
||||||||||||||||ParseAssignmentExpression
|||||||||||||||||ParseCastExpression
||||||||||||||||||ParseCastExpression
||||||||||||||||||consume 'x'
||||||||||||||consume ';'
|||||||||||consume '}'
||||||consume ';'
ParseTopLevelDecl
|ParseExternalDeclaration
||ParseDeclarationOrFunctionDefinition
|||ParseDeclOrFunctionDefInternal
||||ParseDeclarationSpecifiers
|||||consume 'int'
||||ParseDeclGroup
|||||ParseDeclarator
||||||consume 'main', '(', ')'
|||||ParseFunctionDefinition
||||||ParseFunctionStatementBody
|||||||ParseCompoundStatementBody
||||||||consume '{'
||||||||ParseStatementOrDeclaration
|||||||||ParseStatementOrDeclarationAfterAttributes
||||||||||TryAnnotateName
|||||||||||ParseOptionalCXXScopeSpecifier
||||||||||||isTemplateName
|||||||||||||LookupTemplateName
||||||||||||||LookupName
|||||||||||||||CppLookupName
||||||||||||||||CppNamespaceLookup
||||ActOnEndOfTranslationUnit|||||||||||||LookupDirect
||||||||||||consume 'A'
||||||||||||AnnotateTemplateIdToken
|||||||||||||ParseTemplateIdAfterTemplateName
||||||||||||||consume '<'
||||||||||||||ParseTemplateArgumentList
|||||||||||||||ParseTemplateArgument
||||||||||||||||ParseTypeName
|||||||||||||||||ParseSpecifierQualifierList
||||||||||||||||||ParseDeclarationSpecifiers
|||||||||||||||||||consume 'int'
||||||||||||||ParseGreaterThanInTemplateList
|||||||||||||replace '>' to 'annot_template_id'
|||||||||||TryAnnotateTypeOrScopeTokenAfterScopeSpec
||||||||||||AnnotateTemplateIdTokenAsType
|||||||||||||replace 'annot_template_id' to 'annot_typename'
||||||||||ParseDeclaration
|||||||||||ParseSimpleDeclaration
||||||||||||ParseDeclarationSpecifiers
|||||||||||||consume 'annot_typename'
||||||||||||ParseDeclGroup
|||||||||||||ParseDeclarator
||||||||||||||...consume 'a'
|||||||||||||consume ';'
||||||||ParseStatementOrDeclaration
|||||||||ParseStatementOrDeclarationAfterAttributes
||||||||||ParseReturnStatement
|||||||||||consume 'return'
|||||||||||ParseExpression
||||||||||||ParseAssignmentExpression
|||||||||||||ParseCastExpression
||||||||||||||consume 'a'
||||||||||||||ParsePostfixExpressionSuffix
|||||||||||||||consume '.'
|||||||||||||||ParseUnqualifiedId
||||||||||||||||consume 'test'
|||||||||||||||consume '('
|||||||||||||||ParseExpressionList
||||||||||||||||ParseAssignmentExpression
|||||||||||||||||ParseCastExpression
||||||||||||||||||consume '1'
|||||||||||||||consume '}'
||||||||||consume ';'
||||||||consume '}'
ParseTopLevelDecl
|handle eof
```

## 技巧：AST实时打印

为了及时了解符号是否被实例化,我们需要实时打印AST

```c++
print Actions.getASTContext().getTranslationUnitDecl()->dump()
```

另外也可以实时打印一个Decl的SourceLocation:

```shell
print Pattern->getSourceRange().dump(SourceMgr)
```

总之善用dump!

# 尾部实例化

## 调试记录

就算不开模板延迟解析, 函数定义的实例化也是在eof后进行的, 当然声明的实例化还是在解析中进行的.

接下来调试test的尾部实例化.

首先进入ActOnEndOfTranslationUnit:

```c++
  case tok::eof:
    // Check whether -fmax-tokens= was reached.
    if (PP.getMaxTokens() != 0 && PP.getTokenCount() > PP.getMaxTokens()) {
      PP.Diag(Tok.getLocation(), diag::warn_max_tokens_total)
          << PP.getTokenCount() << PP.getMaxTokens();
      SourceLocation OverrideLoc = PP.getMaxTokensOverrideLoc();
      if (OverrideLoc.isValid()) {
        PP.Diag(OverrideLoc, diag::note_max_tokens_total_override);
      }
    }

    // Late template parsing can begin.
    Actions.SetLateTemplateParser(LateTemplateParserCallback, nullptr, this);
    Actions.ActOnEndOfTranslationUnit();
    //else don't tell Sema that we ended parsing: more input might come.
    return true;
```

调用ActOnEndOfTranslationUnitFragment处理隐式实例化:

```c++
  // Complete translation units and modules define vtables and perform implicit
  // instantiations. PCH files do not.
  if (TUKind != TU_Prefix) {
    DiagnoseUseOfUnimplementedSelectors();

    ActOnEndOfTranslationUnitFragment(
        !ModuleScopes.empty() && ModuleScopes.back().Module->Kind ==
                                     Module::PrivateModuleFragment
            ? TUFragmentKind::Private
            : TUFragmentKind::Normal);

    if (LateTemplateParserCleanup)
      LateTemplateParserCleanup(OpaqueParser);

    CheckDelayedMemberExceptionSpecs();
  } 
```

调用PerformPendingInstantiations:

```c++
{
  llvm::TimeTraceScope TimeScope("PerformPendingInstantiations");
  PerformPendingInstantiations();
}
```

相关注释:

```c++
/// Performs template instantiation for all implicit template
/// instantiations we have seen until this point.
void Sema::PerformPendingInstantiations(bool LocalOnly) {
```

每次从队列取出一个待实例化元素:

```c++
  while (!PendingLocalImplicitInstantiations.empty() ||
         (!LocalOnly && !PendingInstantiations.empty())) {
    PendingImplicitInstantiation Inst;

    if (PendingLocalImplicitInstantiations.empty()) {
      Inst = PendingInstantiations.front();
      PendingInstantiations.pop_front();
    } else {
```

PendingImplicitInstantiation的相关注释:

```c++
/// An entity for which implicit template instantiation is required.
///
/// The source location associated with the declaration is the first place in
/// the source code where the declaration was "used". It is not necessarily
/// the point of instantiation (which will be either before or after the
/// namespace-scope declaration that triggered this implicit instantiation),
/// However, it is the location that diagnostics should generally refer to,
/// because users will need to know what code triggered the instantiation.
typedef std::pair<ValueDecl *, SourceLocation> PendingImplicitInstantiation;
```

判断是否是函数实例化(我们也只关注函数实例化):

```c++
    // Instantiate function definitions
    if (FunctionDecl *Function = dyn_cast<FunctionDecl>(Inst.first)) {
      bool DefinitionRequired = Function->getTemplateSpecializationKind() ==
                                TSK_ExplicitInstantiationDefinition;
```

这里有个isMultiVersion的判断, IsMultiVersion是指被attribute 'target'修饰的multiversioned函数, 这里我们并不关心此类函数, 所以走else分支, 调用InstantiateFunctionDefinition.

```c++
if (Function->isMultiVersion()) {
  getASTContext().forEachMultiversionedFunctionVersion(
      Function, [this, Inst, DefinitionRequired](FunctionDecl *CurFD) {
        InstantiateFunctionDefinition(/*FIXME:*/ Inst.second, CurFD, true,
                                      DefinitionRequired, true);
        if (CurFD->isDefined())
          CurFD->setInstantiationIsPending(false);
      });
} else {
  InstantiateFunctionDefinition(/*FIXME:*/ Inst.second, Function, true,
                                DefinitionRequired, true);
  if (Function->isDefined())
    Function->setInstantiationIsPending(false);
}
```

相关注释:

(这个函数大概200行)

```c++
/// Instantiate the definition of the given function from its
/// template.
///
/// \param PointOfInstantiation the point at which the instantiation was
/// required. Note that this is not precisely a "point of instantiation"
/// for the function, but it's close.
///
/// \param Function the already-instantiated declaration of a
/// function template specialization or member function of a class template
/// specialization.
///
/// \param Recursive if true, recursively instantiates any functions that
/// are required by this instantiation.
///
/// \param DefinitionRequired if true, then we are performing an explicit
/// instantiation where the body of the function is required. Complain if
/// there is no such body.
void Sema::InstantiateFunctionDefinition(SourceLocation PointOfInstantiation,
                                         FunctionDecl *Function,
                                         bool Recursive,
                                         bool DefinitionRequired,
                                         bool AtEndOfTU) {
```

不要实例化显示实例化, builtin和已经有定义的函数:
```c++
  // Never instantiate an explicit specialization except if it is a class scope
  // explicit specialization.
  TemplateSpecializationKind TSK =
      Function->getTemplateSpecializationKindForInstantiation();
  if (TSK == TSK_ExplicitSpecialization)
    return;

  // Never implicitly instantiate a builtin; we don't actually need a function
  // body.
  if (Function->getBuiltinID() && TSK == TSK_ImplicitInstantiation &&
      !DefinitionRequired)
    return;

  // Don't instantiate a definition if we already have one.
  const FunctionDecl *ExistingDefn = nullptr;
  if (Function->isDefined(ExistingDefn,
                          /*CheckForPendingFriendDefinition=*/true)) {
    if (ExistingDefn->isThisDeclarationADefinition())
      return;

    // If we're asked to instantiate a function whose body comes from an
    // instantiated friend declaration, attach the instantiated body to the
    // corresponding declaration of the function.
    assert(ExistingDefn->isThisDeclarationInstantiatedFromAFriendDefinition());
    Function = const_cast<FunctionDecl*>(ExistingDefn);
  }
```

获取相关函数模板的Decl:

```c++
  // Find the function body that we'll be substituting.
  const FunctionDecl *PatternDecl = Function->getTemplateInstantiationPattern();
  assert(PatternDecl && "instantiating a non-template");
```

获取模板函数的定义:

```c++
  const FunctionDecl *PatternDef = PatternDecl->getDefinition();
  Stmt *Pattern = nullptr;
  if (PatternDef) {
    Pattern = PatternDef->getBody(PatternDef);
    PatternDecl = PatternDef;
    if (PatternDef->willHaveBody())
      PatternDef = nullptr;
  }
```

处理延后模板解析(我们的例子不涉及延后模板解析):

```c++
// Postpone late parsed template instantiations.
if (PatternDecl->isLateTemplateParsed() &&
    !LateTemplateParser) {
  Function->setInstantiationIsPending(true);
  LateParsedInstantiations.push_back(
      std::make_pair(Function, PointOfInstantiation));
  return;
}
```

另一段延后模板解析逻辑:

```c++
  // Call the LateTemplateParser callback if there is a need to late parse
  // a templated function definition.
  if (!Pattern && PatternDecl->isLateTemplateParsed() &&
      LateTemplateParser) {
    // FIXME: Optimize to allow individual templates to be deserialized.
    if (PatternDecl->isFromASTFile())
      ExternalSource->ReadLateParsedTemplates(LateParsedTemplateMap);

    auto LPTIter = LateParsedTemplateMap.find(PatternDecl);
    assert(LPTIter != LateParsedTemplateMap.end() &&
           "missing LateParsedTemplate");
    LateTemplateParser(OpaqueParser, *LPTIter->second);
    Pattern = PatternDecl->getBody(PatternDecl);
  }
```

实例化函数体前的内容:

```c++
// Substitute into the qualifier; we can get a substitution failure here
// through evil use of alias templates.
// FIXME: Is CurContext correct for this? Should we go to the (instantiation
// of the) lexical context of the pattern?
SubstQualifier(*this, PatternDecl, Function, TemplateArgs);
```

后三个参数分别是函数模板Decl, 当前要实例化的函数Decl, 以及实参列表.

不过这里函数头已经实例化好了, 所以SubstQualifier直接return false.

这里有一段Scope的保存逻辑, 可能和上下文搜索逻辑有关, 先记录一下:

```c++
// Enter the scope of this instantiation. We don't use
// PushDeclContext because we don't have a scope.
Sema::ContextRAII savedContext(*this, Function);

if (addInstantiatedParametersToScope(Function, PatternDecl, Scope,
                                     TemplateArgs))
  return;
```

调用SubstStmt实例化函数体:

```c++
      // Instantiate the function body.
      Body = SubstStmt(Pattern, TemplateArgs);

      if (Body.isInvalid())
        Function->setInvalidDecl();
    }
```

调用TransformStmt:

```c++
StmtResult
Sema::SubstStmt(Stmt *S, const MultiLevelTemplateArgumentList &TemplateArgs) {
  if (!S)
    return S;

  TemplateInstantiator Instantiator(*this, TemplateArgs,
                                    SourceLocation(),
                                    DeclarationName());
  return Instantiator.TransformStmt(S);
}
```

这个地方有宏, 不太好debug, 我们手动debug定位到TransformCompoundStmt.

```c++
template <typename Derived>
StmtResult TreeTransform<Derived>::TransformStmt(Stmt *S, StmtDiscardKind SDK) {
  if (!S)
    return S;

  switch (S->getStmtClass()) {
  case Stmt::NoStmtClass: break;

  // Transform individual statement nodes
  // Pass SDK into statements that can produce a value
#define STMT(Node, Parent)                                              \
  case Stmt::Node##Class: return getDerived().Transform##Node(cast<Node>(S));
#define VALUESTMT(Node, Parent)                                         \
  case Stmt::Node##Class:                                               \
    return getDerived().Transform##Node(cast<Node>(S), SDK);
#define ABSTRACT_STMT(Node)
#define EXPR(Node, Parent)
#include "clang/AST/StmtNodes.inc"

  // Transform expressions by calling TransformExpr.
#define STMT(Node, Parent)
#define ABSTRACT_STMT(Stmt)
#define EXPR(Node, Parent) case Stmt::Node##Class:
#include "clang/AST/StmtNodes.inc"
    {
      ExprResult E = getDerived().TransformExpr(cast<Expr>(S));

      if (SDK == SDK_StmtExprResult)
        E = getSema().ActOnStmtExprResult(E);
      return getSema().ActOnExprStmt(E, SDK == SDK_Discarded);
    }
  }

  return S;
}
```

来到TransformCompoundStmt, 对每条Stmt(这里只有一条` return x`)调用TransformStmt

```c++
for (auto *B : S->body()) {
  StmtResult Result = getDerived().TransformStmt(
      B, IsStmtExpr && B == ExprResult ? SDK_StmtExprResult : SDK_Discarded);
```

来到TransformStmt->TransformReturnStmt, 调用TransformInitializer, 将模板RetrunStmt(`S`)转换为int实例化Stmt(`Result`)

```c++
template<typename Derived>
StmtResult
TreeTransform<Derived>::TransformReturnStmt(ReturnStmt *S) {
  ExprResult Result = getDerived().TransformInitializer(S->getRetValue(),
                                                        /*NotCopyInit*/false);
  if (Result.isInvalid())
    return StmtError();

  // FIXME: We always rebuild the return statement because there is no way
  // to tell whether the return type of the function has changed.
  return getDerived().RebuildReturnStmt(S->getReturnLoc(), Result.get());
}
```

走这个分支, 调用TransformExpr:

```c++
// If this is copy-initialization, we only need to reconstruct
// InitListExprs. Other forms of copy-initialization will be a no-op if
// the initializer is already the right type.
CXXConstructExpr *Construct = dyn_cast<CXXConstructExpr>(Init);
if (!NotCopyInit && !(Construct && Construct->isListInitialization()))
  return getDerived().TransformExpr(Init);
```

调用TransformDeclRefExpr, 获取指向的decl, 这里是x:

```c++
ExprResult
TemplateInstantiator::TransformDeclRefExpr(DeclRefExpr *E) {
  NamedDecl *D = E->getDecl();
```

调用inherited::TransformDeclRefExpr(`typedef TreeTransform<TemplateInstantiator> inherited;`):

```c++
  // Handle references to function parameter packs.
  if (VarDecl *PD = dyn_cast<VarDecl>(D))
    if (PD->isParameterPack())
      return TransformFunctionParmPackRefExpr(E, PD);

  return inherited::TransformDeclRefExpr(E);
}
```

这里的inherited应该是指目标Decl在源码中找不到, 所以要去已有实例化里找.

这里我们调用TransformDecl把T x(`E->getDecl()`)转换为int x(`ND`):

```c++
  ValueDecl *ND
    = cast_or_null<ValueDecl>(getDerived().TransformDecl(E->getLocation(),
                                                         E->getDecl()));
```

调用FindInstantiatedDecl:

```c++
  return SemaRef.FindInstantiatedDecl(Loc, cast<NamedDecl>(D), TemplateArgs);
}
```

相关注释:

```c++
/// Find the instantiation of the given declaration within the
/// current instantiation.
///
/// This routine is intended to be used when \p D is a declaration
/// referenced from within a template, that needs to mapped into the
/// corresponding declaration within an instantiation. For example,
/// given:
///
/// \code
/// template<typename T>
/// struct X {
///   enum Kind {
///     KnownValue = sizeof(T)
///   };
///
///   bool getKind() const { return KnownValue; }
/// };
///
/// template struct X<int>;
/// \endcode
///
/// In the instantiation of X<int>::getKind(), we need to map the \p
/// EnumConstantDecl for \p KnownValue (which refers to
/// X<T>::<Kind>::KnownValue) to its instantiation (X<int>::<Kind>::KnownValue).
/// \p FindInstantiatedDecl performs this mapping from within the instantiation
/// of X<int>.
NamedDecl *Sema::FindInstantiatedDecl(SourceLocation Loc, NamedDecl *D,
                          const MultiLevelTemplateArgumentList &TemplateArgs,
                          bool FindingInstantiatedContext) {
```

大概意思是在当前模板已经实例化好了一些部分, 并且已经建立了原始Decl到实例化Decl间的映射, 后续实例化模板其他部分时, 如果遇到了对于原始Decl的引用, 需要将其绑定到已实例化Decl上, 因此就有了这个函数.

调用findInstantiationOf:

```c++
// D is a local of some kind. Look into the map of local
// declarations to their instantiations.
if (CurrentInstantiationScope) {
  if (auto Found = CurrentInstantiationScope->findInstantiationOf(D)) {
    if (Decl *FD = Found->dyn_cast<Decl *>())
      return cast<NamedDecl>(FD);
```

在local scope中查找:

```c++
D = getCanonicalParmVarDecl(D);
for (LocalInstantiationScope *Current = this; Current;
     Current = Current->Outer) {
```

接着是一个do while循环:

```c++
// Check if we found something within this scope.
const Decl *CheckD = D;
do {
  LocalDeclsMap::iterator Found = Current->LocalDecls.find(CheckD);
  if (Found != Current->LocalDecls.end())
    return &Found->second;

  // If this is a tag declaration, it's possible that we need to look for
  // a previous declaration.
  if (const TagDecl *Tag = dyn_cast<TagDecl>(CheckD))
    CheckD = Tag->getPreviousDecl();
  else
    CheckD = nullptr;
} while (CheckD);
```

解释一下为什么要循环, 如果待查找Decl是一个定义在函数内部的Class, 这个Class可能有多个声明, 因此要遍历其Previous Decl, 为每个Decl都调用`Current->LocalDecls.find`进行查找.

这里我们第一次就可以直接找到对应的`int x` Decl, 直接返回FindInstantiatedDecl, 返回TransformDecl, 返回TransformDeclRefExpr, 验证一下我们查找到的Decl:

```shell
print ND->dump()
ParmVarDecl 0x5555556acf28 <test.cpp:3:10, col:12> col:12 x 'int':'int'
```

接着通过RebuildDeclRefExpr重建DeclRefExpr:

```
  return getDerived().RebuildDeclRefExpr(QualifierLoc, ND, NameInfo,
                                         Found, TemplateArgs);
}
```

返回TransformDeclRefExpr, 返回TransformExpr, 返回TransformInitializer, 返回TransformReturnStmt, 此时`Result`中存放的是转换完的的return语句, 接下来我们调用RebuildReturnStmt重建return语句即可:

```c++
  // FIXME: We always rebuild the return statement because there is no way
  // to tell whether the return type of the function has changed.
  return getDerived().RebuildReturnStmt(S->getReturnLoc(), Result.get());
```

返回TransformStmt, 返回TransformCompoundStmt, 将刚刚实例化的语句添加到`Statements`:

```c++
    SubStmtChanged = SubStmtChanged || Result.get() != B;
    Statements.push_back(Result.getAs<Stmt>());
  }
```

退出body语句解析的循环, 调用RebuildCompoundStmt重建compoundstmt:

```c++
return getDerived().RebuildCompoundStmt(S->getLBracLoc(),
                                        Statements,
                                        S->getRBracLoc(),
                                        IsStmtExpr);
```

返回TransformStmt, 返回InstantiateFunctionDefinition, 调用ActOnFinishFunctionBody完成函数处理逻辑:

```c++
      // Instantiate the function body.
      Body = SubstStmt(Pattern, TemplateArgs);

      if (Body.isInvalid())
        Function->setInvalidDecl();
    }
    // FIXME: finishing the function body while in an expression evaluation
    // context seems wrong. Investigate more.
    ActOnFinishFunctionBody(Function, Body.get(), /*IsInstantiation=*/true);
```

解析完的函数给Consumer消费一下:

```c++
DeclGroupRef DG(Function);
Consumer.HandleTopLevelDecl(DG);
```

最后这段逻辑可能是处理实例化过程中实例化了其他模板的逻辑, 后面验证一下:

```c++
// This class may have local implicit instantiations that need to be
// instantiation within this scope.
LocalInstantiations.perform();
Scope.Exit();
GlobalInstantiations.perform();
```

> perform会调用PerformPendingInstantiations, 大概率就是处理嵌套实例化逻辑的

返回PerformPendingInstantiations, 返回ActOnEndOfTranslationUnitFragment, 返回ActOnEndOfTranslationUnit, 完成实例化逻辑.

## 总结

```
ActOnEndOfTranslationUnit
|ActOnEndOfTranslationUnitFragment
||PerformPendingInstantiations
|||InstantiateFunctionDefinition
||||SubstStmt
|||||TransformStmt
||||||TransformCompoundStmt
|||||||TransformStmt
||||||||TransformReturnStmt
|||||||||TransformInitializer
||||||||||TransformExpr
||||||||||TransformDeclRefExpr
|||||||||||TransformDeclRefExpr
||||||||||||TransformDecl
|||||||||||||FindInstantiatedDecl
||||||||||||||findInstantiationOf
|||||||||||||||match T x -> int x
|||||||||||***RebuildDeclRefExpr***
|||||||||***RebuildReturnStmt***
|||||||***RebuildCompoundStmt***
```

## 技巧：IR实时打印

print ((clang::BackendConsumer &)Consumer).getCodeGenerator()->GetModule()->dump()

没有符号的话可以切到有符号的debug上下文打印

# 尾部实例化的IR生成

简化一下测试用例:

```c++
template<typename T> T test(T x) {
  return x;
}

int main() {
  return test(1);
}
```

## 调试记录

实例化的IR是在最后的HnadleTranslationUnit生成的.

具体而言是clang/lib/CodeGen/CodeGenModule.cpp(CodeGenModule::EmitDeferred)生成的

需要实例化的Decl存放在`DeferredDeclsToEmit`中.

接下来会有一个for循环+递归:

```c++
  for (GlobalDecl &D : CurDeclsToEmit) {
	...

    // Otherwise, emit the definition and move on to the next one.
    EmitGlobalDefinition(D, GV);

    // If we found out that we need to emit more decls, do that recursively.
    // This has the advantage that the decls are emitted in a DFS and related
    // ones are close together, which is convenient for testing.
    if (!DeferredVTables.empty() || !DeferredDeclsToEmit.empty()) {
      EmitDeferred();
      assert(DeferredVTables.empty() && DeferredDeclsToEmit.empty());
    }
  }
```

之所以采用递归, 注释中已经解释了.

这里继续调用EmitGlobalDefinition, EmitGlobalFunctionDefinition.

生成函数的IR Type(ret type + args type):

```c++
  // Compute the function info and LLVM type.
  const CGFunctionInfo &FI = getTypes().arrangeGlobalDeclaration(GD);
  llvm::FunctionType *Ty = getTypes().GetFunctionType(FI);
```

生成函数体IR:

```c++
  CodeGenFunction(*this).GenerateCode(GD, Fn, FI);
```

## 总结

```
HandleTranslationUnit
|Release
||EmitDeferred
|||EmitGlobalDefinition
||||EmitGlobalFunctionDefinition
|||||GetFunctionType
|||||GenerateCode
```

