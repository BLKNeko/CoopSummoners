# gate.ps1 — run scenarios through rhost.exe --standalone on THIS Windows machine (no ssh runner) and
# compare frame / tick / rng / fb with each scenario's expected.json (what `rhost.py corpus --check
# --standalone` checks).  Fresh-start scenarios only (no "from" chains).
#   gate.ps1 -Repo \wsl.localhost\Ubuntu\home\me\OpenSummoners -Game C:\fs-base -Out C:\fs-gate [-Coop] [-Only a,b]
param([string]$Repo, [string]$Game, [string]$Out, [switch]$Coop, [string[]]$Only, [string]$Extra = "",
      [string]$TraceDir = "", [string]$UserDir = "")
$ErrorActionPreference = "Stop"
$scen = Join-Path $Repo "tests\scenarios"
if ($TraceDir) { $scen = $TraceDir }
$exe = Join-Path $Out "rhost.exe"
New-Item -ItemType Directory -Force $Out | Out-Null
Copy-Item (Join-Path $Repo "build\rhost\rhost.exe") $exe -Force
$runs = @()
foreach ($d in Get-ChildItem $scen -Directory) {
    if ($Only -and $Only -notcontains $d.Name) { continue }
    $tr = Join-Path $d.FullName "trace.jsonl"
    if (-not (Test-Path $tr)) { continue }
    $head = Get-Content $tr -TotalCount 3 | Out-String
    if ((-not $UserDir) -and $head -match '"from"' -or $head -match 'EN-SE' -or $head -match '"ense') { continue }
    $rd = Join-Path $Out $d.Name
    if (Test-Path $rd) { Remove-Item -Recurse -Force $rd -Confirm:$false }
    $gd = Join-Path $rd "game"
    New-Item -ItemType Directory -Force $gd | Out-Null
    foreach ($f in "sotes.exe","sotesd.dll","sotesw.dll","sotesp.dll","lizsoft.spl") {
        New-Item -ItemType HardLink -Path (Join-Path $gd $f) -Target (Join-Path $Game $f) | Out-Null }
    Copy-Item $tr (Join-Path $rd "trace.jsonl")
    if ($UserDir) { Copy-Item $UserDir (Join-Path $rd "user") -Recurse }
    Copy-Item (Join-Path $Repo "tools\rhost\volatile.enold.txt") (Join-Path $rd "volatile.txt")
    $a = @("--exe", "$gd\sotes.exe", "--game", $gd, "--run", $rd, "--trace", "$rd\trace.jsonl", "--fb-hash", "--standalone")
    if ($Coop) { $a += "--coop" }
    if ($Extra) { $a += $Extra.Split(" ") }
    $p = Start-Process -FilePath $exe -ArgumentList $a -WorkingDirectory $rd -PassThru -WindowStyle Hidden `
         -RedirectStandardOutput "$rd\stdout.txt" -RedirectStandardError "$rd\stderr.txt"
    $runs += [pscustomobject]@{ Name = $d.Name; Dir = $rd; Proc = $p; Exp = (Join-Path $d.FullName "expected.json") }
}
$runs | ForEach-Object { $_.Proc.WaitForExit() }
$bad = 0
foreach ($r in $runs) {
    $line = Get-Content (Join-Path $r.Dir "rhost.log") | Where-Object { $_ -like "RESULT*" } | Select-Object -Last 1
    $got = @{}; foreach ($kv in ($line -split " ")) { if ($kv -match "^(\w+)=(.*)$") { $got[$Matches[1]] = $Matches[2] } }
    $msg = ""
    if (Test-Path $r.Exp) {
        $exp = Get-Content $r.Exp -Raw | ConvertFrom-Json
        foreach ($k in "frame","tick","rng","fb") {
            if ("$($exp.$k)" -ne "$($got[$k])") { $msg += " $k=$($got[$k])(want $($exp.$k))" } }
    } else { $msg = " (no expected.json)" }
    $ok = ($msg -eq "") -and ($got["status"] -eq "ok" -or $got["status"] -eq "exit")
    if (-not $ok) { $bad++ }
    "{0,-22} {1} status={2} frame={3}{4}" -f $r.Name, $(if ($ok) {"PASS"} else {"FAIL"}), $got["status"], $got["frame"], $msg
}
"GATE: $(if ($bad) {"FAIL ($bad)"} else {"PASS"}) ($($runs.Count) scenarios)"
