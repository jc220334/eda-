param([string]$Title = "EDA Editor - PCB Design", [string]$Out = "E:\202691\eda-work\shot.png")
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices; using System.Text;
public class Cap4 {
  public delegate bool CB(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(CB cb, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr ctx);
  public struct RECT { public int L,T,R,B; }
}
'@
[void][Cap4]::SetThreadDpiAwarenessContext([IntPtr](-4))
$script:target = [IntPtr]::Zero
$cb = [Cap4+CB]{ param($h,$l)
  $sb = New-Object System.Text.StringBuilder 512
  [void][Cap4]::GetWindowText($h,$sb,512)
  if ($sb.ToString() -like $Title -and [Cap4]::IsWindowVisible($h)) { $script:target = $h; return $false }
  return $true
}
[void][Cap4]::EnumWindows($cb, [IntPtr]::Zero)
if ($script:target -eq [IntPtr]::Zero) { Write-Output "NOT FOUND"; exit 1 }
$h = $script:target
[void][Cap4]::ShowWindow($h, 9)
[void][Cap4]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
[void][Cap4]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 700
$r = New-Object Cap4+RECT
[void][Cap4]::GetWindowRect($h, [ref]$r)
$w = $r.R - $r.L; $ht = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $ht))
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
[void][Cap4]::SetWindowPos($h, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)
Write-Output "SAVED $Out ${w}x${ht}"

