 # ccc-print-bindings source code

llvm-project-llvmorg-16.0.4/clang/lib/Driver/Driver.cpp

`Driver::BuildJobsForAction`

```cpp
  if (CCCPrintBindings && !CCGenDiagnostics) {
    llvm::errs() << "# \"" << T->getToolChain().getTripleString() << '"'
                 << " - \"" << T->getName() << "\", inputs: [";
    for (unsigned i = 0, e = InputInfos.size(); i != e; ++i) {
      llvm::errs() << InputInfos[i].getAsString();
      if (i + 1 != e)
        llvm::errs() << ", ";
    }
    if (UnbundlingResults.empty())
      llvm::errs() << "], output: " << Result.getAsString() << "\n";
    else {
      llvm::errs() << "], outputs: [";
      for (unsigned i = 0, e = UnbundlingResults.size(); i != e; ++i) {
        llvm::errs() << UnbundlingResults[i].getAsString();
        if (i + 1 != e)
          llvm::errs() << ", ";
      }
      llvm::errs() << "] \n";
    }
  } else {
    if (UnbundlingResults.empty())
      T->ConstructJob(
          C, *JA, Result, InputInfos,
          C.getArgsForToolChain(TC, BoundArch, JA->getOffloadingDeviceKind()),
          LinkingOutput);
    else
      T->ConstructJobMultipleOutputs(
          C, *JA, UnbundlingResults, InputInfos,
          C.getArgsForToolChain(TC, BoundArch, JA->getOffloadingDeviceKind()),
          LinkingOutput);
  }
  return {Result};
```

format:

```
Tool::getToolChain() Tool::getName() inputs["...",...] (output: "..." | outputs["...", ...])
```

