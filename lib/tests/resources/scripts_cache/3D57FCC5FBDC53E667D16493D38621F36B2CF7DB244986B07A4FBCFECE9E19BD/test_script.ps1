if ($Args.count -eq 0) {
  Write-Output "TESTING"
  exit
} elseif ($Args[0] -eq "FAIL") {
  Write-Error "FAIL TESTING"
  exit 5
} else {
  $str=""
  foreach ($arg in $Args) {
    $str += "| $arg "
  }
  Write-Output $str
}
