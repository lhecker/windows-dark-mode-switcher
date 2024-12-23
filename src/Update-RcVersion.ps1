<#
  .SYNOPSIS
  Updates the version numbers in *.rc files.

  .DESCRIPTION
  Updates major version, minor version, patch number and revision number of
  both the file version and the product version specified in resource files.

  .PARAMETER major
  Specifies the new major version.

  .PARAMETER minor
  Specifies the new minor version.

  .PARAMETER patch
  Specifies the new patch number.

  .PARAMETER revision
  [optional] Specifies the new revision number.

  .PARAMETER rcfiles
  [optional] Specifies a list of paths of the resource files to update.

  .INPUTS
  [optional] The list of paths of the resource files to update.

  .OUTPUTS
  The new content of the resource file.

  .NOTES
  The script can be executed without any arguments, where the user is prompted
  to enter the values for mandatory parameters. If needed, change the default
  value of the 'rcfiles' parameter accordingly.
  The old content of the resource file is overwritten. In addition, the updated
  content can be instantly checked in the terminal window. Changed lines are
  highlighted.
#>
[CmdletBinding()]
param (
  [Parameter(Mandatory=$true)][UInt16]$major,
  [Parameter(Mandatory=$true)][UInt16]$minor,
  [Parameter(Mandatory=$true)][UInt16]$patch,
  [UInt16]$revision = 0,
  [Parameter(ValueFromPipeline=$true)][string[]]$rcfiles = 'resource.rc'
)
begin {
  $regex = '(^[^/]*\b(?:file|product)version[", ]+)\d+([,.])\d+([,.])\d+([,.])\d+\b(.*$)'
  $subst = "`${1}$major`${2}$minor`${3}$patch`${4}$revision`${5}"
  $e = [Char]27
}
process {
  ForEach ($rcfile in $rcfiles) {
    if ((-not (Test-Path $rcfile -PathType Leaf)) -or ($rcfile -inotlike '*.rc')) {continue}
    (Get-Content $rcfile) -ireplace $regex, $subst | Tee-Object $rcfile |
     ForEach-Object {"$e[90m | $e[0m$($_ -ireplace $regex, "$e[97;41m`${0}$e[0m")"} -Begin {"`n$e[90m Updated `"$rcfile`":$e[0m"} -End {''}
  }
}
end {
  $instances = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue -Property @('Name', 'ParentProcessId', 'ProcessId')
  $parentpid = $instances | Where-Object ProcessId -eq $PID | Select-Object -ExpandProperty ParentProcessId
  $parentname = $instances | Where-Object ProcessId -eq $parentpid | Select-Object -ExpandProperty Name
  if ($parentname -eq 'explorer.exe') {pause}
}
