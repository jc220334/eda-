param([string]$Title = "EDA Editor*", [string]$Out = "E:\202691\eda-work\shot.png")
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices; using System.Text;
public class CapP {
  public delegate bool CB(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr ctx);
  [DllImport("user32.dll")] public static extern bool EnumWindows(CB cb, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  public struct RECT { public int L,T,R,B; }
}
'@
[void][CapP]::SetThreadDpiAwarenessContext([IntPtr](-4))
$script:target=[IntPtr]::Zero
$cb=[CapP+CB]{ param($h,$l)
  $sb=New-Object System.Text.StringBuilder 512
  [void][CapP]::GetWindowText($h,$sb,512)
  if($sb.ToString() -like $Title -and [CapP]::IsWindowVisible($h)){ $script:target=$h; return $false }
  return $true }
[void][CapP]::EnumWindows($cb,[IntPtr]::Zero)
if($script:target -eq [IntPtr]::Zero){ Write-Output "NOT FOUND"; exit 1 }
$h=$script:target
$r=New-Object CapP+RECT
[void][CapP]::GetWindowRect($h,[ref]$r)
$w=$r.R-$r.L; $ht=$r.B-$r.T
$bmp=New-Object System.Drawing.Bitmap $w,$ht
$g=[System.Drawing.Graphics]::FromImage($bmp)
$hdc=$g.GetHdc()
$ok=[CapP]::PrintWindow($h,$hdc,2)
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save($Out,[System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "SAVED $Out ${w}x${ht} printwindow=$ok"
