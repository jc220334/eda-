param(
    [string]$Title = "EDA Editor - PCB Design",
    [string]$Steps = ""
)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices; using System.Text;
public class Ui {
  public delegate bool CB(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr ctx);
  [DllImport("user32.dll")] public static extern bool EnumWindows(CB cb, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, CB cb, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  public struct RECT { public int L,T,R,B; }
}
'@
[void][Ui]::SetThreadDpiAwarenessContext([IntPtr](-4))
$script:target = [IntPtr]::Zero
$cb = [Ui+CB]{ param($h,$l)
  $sb = New-Object System.Text.StringBuilder 512
  [void][Ui]::GetWindowText($h,$sb,512)
  if ($sb.ToString() -like $Title -and [Ui]::IsWindowVisible($h)) { $script:target = $h; return $false }
  return $true
}
[void][Ui]::EnumWindows($cb, [IntPtr]::Zero)
if ($script:target -eq [IntPtr]::Zero) { Write-Output "NOT FOUND: $Title"; exit 1 }
$h = $script:target
[void][Ui]::ShowWindow($h, 9)
[void][Ui]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
[void][Ui]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 350
$r = New-Object Ui+RECT
[void][Ui]::GetWindowRect($h, [ref]$r)
Write-Output "WINDOW $($r.L),$($r.T) $($r.R-$r.L)x$($r.B-$r.T)"

function Save-Shot([string]$path) {
  $rr = New-Object Ui+RECT
  [void][Ui]::GetWindowRect($h, [ref]$rr)
  $w = $rr.R - $rr.L; $ht = $rr.B - $rr.T
  $bmp = New-Object System.Drawing.Bitmap $w, $ht
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($rr.L, $rr.T, 0, 0, (New-Object System.Drawing.Size $w, $ht))
  $g.Dispose(); $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Output "SHOT $path ${w}x${ht}"
}

function Move-To($x, $y) {
  [void][Ui]::SetCursorPos($r.L + [int]$x, $r.T + [int]$y)
}

function Do-Click($x, $y) {
  Move-To $x $y
  Start-Sleep -Milliseconds 120
  [Ui]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  [Ui]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 150
}

function Do-Drag($x1, $y1, $x2, $y2) {
  Move-To $x1 $y1
  Start-Sleep -Milliseconds 120
  [Ui]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  for ($i = 1; $i -le 12; $i++) {
    $x = [int]($x1 + ($x2 - $x1) * $i / 12)
    $y = [int]($y1 + ($y2 - $y1) * $i / 12)
    Move-To $x $y
    Start-Sleep -Milliseconds 20
  }
  [Ui]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 250
}

function Get-CanvasHandle {
  $script:canvas = [IntPtr]::Zero
  $script:canvasRect = New-Object Ui+RECT
  $cb2 = [Ui+CB]{ param($w,$l)
    $cn = New-Object System.Text.StringBuilder 128
    [void][Ui]::GetClassName($w,$cn,128)
    if ($cn.ToString() -eq "wxWindowNR") {
      $rr = New-Object Ui+RECT
      [void][Ui]::GetWindowRect($w, [ref]$rr)
      $ww = $rr.R - $rr.L; $hh = $rr.B - $rr.T
      if ($ww -gt 600 -and $hh -gt 400) {
        $script:canvas = $w
        $script:canvasRect = $rr
      }
    }
    return $true
  }
  [void][Ui]::EnumChildWindows($h, $cb2, [IntPtr]::Zero)
  if ($script:canvas -eq [IntPtr]::Zero) { Write-Output "CANVAS NOT FOUND"; return $false }
  $cr = New-Object Ui+RECT
  [void][Ui]::GetClientRect($script:canvas, [ref]$cr)
  $script:cw = $cr.R - $cr.L
  $script:ch = $cr.B - $cr.T
  if ($script:cw -le 0) { $script:cw = 1 }
  $script:ratioX = ($script:canvasRect.R - $script:canvasRect.L) / $script:cw
  $script:ratioY = ($script:canvasRect.B - $script:canvasRect.T) / $script:ch
  Write-Output "CANVAS client ${script:cw}x${script:ch} phys $(($script:canvasRect.R-$script:canvasRect.L))x$(($script:canvasRect.B-$script:canvasRect.T)) ratio ${script:ratioX}"
  return $true
}

function Canvas-Point([int]$x, [int]$y, [ref]$lx, [ref]$ly) {
  $lx.Value = [int][math]::Round($x / $script:ratioX)
  $ly.Value = [int][math]::Round($y / $script:ratioY)
}

function Do-MsgClick($x, $y) {
  if (-not (Get-CanvasHandle)) { return }
  $lx = 0; $ly = 0
  Canvas-Point ([int]$x) ([int]$y) ([ref]$lx) ([ref]$ly)
  $lp = [long](($ly -band 0xFFFF) * 65536 + ($lx -band 0xFFFF))
  Write-Output "MSG down phys($x,$y) client($lx,$ly) lp=$lp canvas=$($script:canvas)"
  [void][Ui]::PostMessage($script:canvas, 0x0201, [IntPtr]1, [IntPtr]$lp)
  Start-Sleep -Milliseconds 100
  [void][Ui]::PostMessage($script:canvas, 0x0202, [IntPtr]0, [IntPtr]$lp)
  Start-Sleep -Milliseconds 200
}

function Do-MsgDrag($x1, $y1, $x2, $y2) {
  if (-not (Get-CanvasHandle)) { return }
  $lx = 0; $ly = 0
  Canvas-Point ([int]$x1) ([int]$y1) ([ref]$lx) ([ref]$ly)
  $lp = [long](($ly -band 0xFFFF) * 65536 + ($lx -band 0xFFFF))
  Write-Output "MSG down phys($x,$y) client($lx,$ly) lp=$lp canvas=$($script:canvas)"
  [void][Ui]::PostMessage($script:canvas, 0x0201, [IntPtr]1, [IntPtr]$lp)
  Start-Sleep -Milliseconds 100
  for ($i = 1; $i -le 8; $i++) {
    $xx = [int]($x1 + ($x2 - $x1) * $i / 8)
    $yy = [int]($y1 + ($y2 - $y1) * $i / 8)
    Canvas-Point $xx $yy ([ref]$lx) ([ref]$ly)
    $lp = [long](($ly -band 0xFFFF) * 65536 + ($lx -band 0xFFFF))
    [void][Ui]::PostMessage($script:canvas, 0x0200, [IntPtr]1, [IntPtr]$lp)
    Start-Sleep -Milliseconds 30
  }
  [void][Ui]::PostMessage($script:canvas, 0x0202, [IntPtr]0, [IntPtr]$lp)
  Start-Sleep -Milliseconds 250
}
if ($Steps -ne "") {
  foreach ($step in $Steps.Split(";")) {
    $step = $step.Trim()
    if ($step -eq "") { continue }
    $parts = $step.Split(":", 2)
    $cmd = $parts[0]
    if ($cmd -eq "click") { $p = $parts[1].Split(","); Do-Click $p[0] $p[1] }
    elseif ($cmd -eq "drag") { $p = $parts[1].Split(","); Do-Drag $p[0] $p[1] $p[2] $p[3] }
    elseif ($cmd -eq "move") { $p = $parts[1].Split(","); Move-To $p[0] $p[1] }
    elseif ($cmd -eq "msgclick") { $p = $parts[1].Split(","); Do-MsgClick $p[0] $p[1] }
    elseif ($cmd -eq "msgdrag") { $p = $parts[1].Split(","); Do-MsgDrag $p[0] $p[1] $p[2] $p[3] }
    elseif ($cmd -eq "shot") { Save-Shot $parts[1] }
    elseif ($cmd -eq "sleep") { Start-Sleep -Milliseconds ([int]$parts[1]) }
    elseif ($cmd -eq "text") { [System.Windows.Forms.SendKeys]::SendWait($parts[1]) }
    elseif ($cmd -eq "key") { [System.Windows.Forms.SendKeys]::SendWait("{$($parts[1])}") }
    elseif ($cmd -eq "send") { [System.Windows.Forms.SendKeys]::SendWait($parts[1]) }
    else { Write-Output "UNKNOWN STEP $step" }
  }
}
[void][Ui]::SetWindowPos($h, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)






