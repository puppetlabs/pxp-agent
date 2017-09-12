#!/usr/bin/env powershell
#requires -version 2.0

try {
  $here = (Split-Path -Parent $MyInvocation.MyCommand.Path)
  . $here\PowershellShim-Helper.ps1

  $scriptArgsHash = Get-ContentAsJson ($input -join "")

  Write-Debug "`nTask-shim deserialized arguments to Hashtable:`n"

  if ($scriptArgsHash) {
    $scriptArgsHash.GetEnumerator() | % {
      Write-Debug "* $($_.Key) ($($_.Value.GetType())):`n$($_.Value | ConvertTo-String)"
    }
  }

  & $args[0] @scriptArgsHash

} catch {
  Write-Error $_
  if ($LASTEXITCODE -eq $null) { $LASTEXITCODE = 1 }
}
exit $LASTEXITCODE
