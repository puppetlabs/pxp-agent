[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [String]
  $Message
)

Write-Output $Message
