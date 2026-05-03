$path = 'C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\plugins\melody_maker\plugin_vst3.cpp'
$text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$startSig = "    tresult PLUGIN_API process(ProcessData& data) override"
$start = $text.IndexOf($startSig)
if ($start -lt 0) { throw 'process signature not found' }
$lineEnd = $text.IndexOf("`n", $start)
$signatureText = $text.Substring($start, $lineEnd + 1 - $start)
$braceStart = $text.IndexOf('{', $lineEnd)
$depth = 0; $inLine = $false; $inBlock = $false; $inStr = [char]0; $esc = $false; $end = -1
for ($i = $braceStart; $i -lt $text.Length; $i++) {
    $ch = $text[$i]; $nxt = if ($i + 1 -lt $text.Length) { $text[$i + 1] } else { [char]0 }
    if ($inLine) { if ($ch -eq "`n") { $inLine = $false }; continue }
    if ($inBlock) { if ($ch -eq '*' -and $nxt -eq '/') { $inBlock = $false; $i++ }; continue }
    if ($inStr -ne [char]0) { if ($esc) { $esc = $false } elseif ($ch -eq '\') { $esc = $true } elseif ($ch -eq $inStr) { $inStr = [char]0 }; continue }
    if ($ch -eq '/' -and $nxt -eq '/') { $inLine = $true; $i++; continue }
    if ($ch -eq '/' -and $nxt -eq '*') { $inBlock = $true; $i++; continue }
    if ($ch -eq '"' -or $ch -eq "'") { $inStr = $ch; continue }
    if ($ch -eq '{') { $depth++ }
    elseif ($ch -eq '}') { $depth--; if ($depth -eq 0) { $end = $i + 1; if ($end -lt $text.Length -and $text[$end] -eq "`r") { $end++ }; if ($end -lt $text.Length -and $text[$end] -eq "`n") { $end++ }; break } }
}
if ($end -lt 0) { throw 'process end not found' }
$block = $text.Substring($start, $end - $start)
$body = $block.Replace($signatureText, "tresult PLUGIN_API MelodyMakerVST3::process(ProcessData& data)`r`n")
$bodyLines = $body -split "(?<=`n)"
$body = (($bodyLines | ForEach-Object { if ($_.StartsWith('    ')) { $_.Substring(4) } else { $_ } }) -join '')
$text2 = $text.Substring(0, $start) + "    tresult PLUGIN_API process(ProcessData& data) override;`r`n" + $text.Substring($end)
$insertMarker = 'IPlugView* PLUGIN_API MelodyMakerVST3::createView(FIDString name)'
$insert = $text2.IndexOf($insertMarker)
if ($insert -lt 0) { throw 'insert marker not found' }
$text2 = $text2.Substring(0, $insert) + $body + "`r`n" + $text2.Substring($insert)
for ($try = 0; $try -lt 10; $try++) {
    try { [System.IO.File]::WriteAllText($path, $text2, [System.Text.Encoding]::UTF8); Write-Host 'Moved process body out of class'; exit 0 }
    catch { Start-Sleep -Milliseconds 300 }
}
throw 'Could not write plugin file after retries'
