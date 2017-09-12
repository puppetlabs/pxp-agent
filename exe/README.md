# PowerShell Shim Testing

A simple smoke test can be run on any platform with PowerShell via
```
type ../acceptance/files/complex-args.json | ./PowershellShim.ps1 ../acceptance/files/complex-task.ps1
```

More complete tests can be run via [Pester](https://github.com/pester/Pester), which will exercise PowerShell 2 code paths.
