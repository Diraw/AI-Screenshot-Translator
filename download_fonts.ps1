$baseUrl = "https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/fonts"
$fonts = @(
    "KaTeX_AMS-Regular",
    "KaTeX_Caligraphic-Bold",
    "KaTeX_Caligraphic-Regular",
    "KaTeX_Fraktur-Bold",
    "KaTeX_Fraktur-Regular",
    "KaTeX_Main-Bold",
    "KaTeX_Main-BoldItalic",
    "KaTeX_Main-Italic",
    "KaTeX_Main-Regular",
    "KaTeX_Math-BoldItalic",
    "KaTeX_Math-Italic",
    "KaTeX_SansSerif-Bold",
    "KaTeX_SansSerif-Italic",
    "KaTeX_SansSerif-Regular",
    "KaTeX_Script-Regular",
    "KaTeX_Size1-Regular",
    "KaTeX_Size2-Regular",
    "KaTeX_Size3-Regular",
    "KaTeX_Size4-Regular",
    "KaTeX_Typewriter-Regular"
)
$extensions = @("woff2", "woff", "ttf")
$dest = "e:\Script\antigravity\AI-Screenshot-Translator-Cpp-webview2\assets\libs\fonts"

if (!(Test-Path $dest)) {
    New-Item -ItemType Directory -Force -Path $dest
}

foreach ($font in $fonts) {
    foreach ($ext in $extensions) {
        $file = "$font.$ext"
        $url = "$baseUrl/$file"
        $out = Join-Path $dest $file
        if (!(Test-Path $out)) {
            Write-Host "Downloading $file..."
            try {
                Invoke-WebRequest -Uri $url -OutFile $out
            } catch {
                Write-Host "Failed to download $file"
            }
        }
    }
}
Write-Host "Fonts download complete."
