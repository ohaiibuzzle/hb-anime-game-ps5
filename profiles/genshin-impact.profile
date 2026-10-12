; Genshin Impact - Download / Update: sophon-dl.exe fetches the game through
; HoYoPlay's Sophon chunk protocol into C:\Games\GenshinImpact of this prefix.
;
; Run it again at any time: it resumes an interrupted download, updates to
; the current version when HoYoverse releases one, and repairs damaged
; files. Add --verify to the arguments to hash every file (slow, about as
; long as reading 150 GB). Voice packs: replace en-us with any of zh-cn,
; ja-jp, ko-kr, or list several; each is 15-18 GB.
;
; Cross (Enter) closes the window once it has finished or failed; the log is
; C:\Games\GenshinImpact\sophon-dl.log (prefixes/genshin-impact/drive_c/...).
[application]
id = genshin-impact
name = Genshin Impact - Download / Update
executable = C:\Games\sophon-dl\sophon-dl.exe
working_directory = C:\Games\sophon-dl
arguments = hk4e_global C:\Games\GenshinImpact game en-us
prefix = genshin-impact
runtime = wine-wow64
architecture = pe64
graphics = gdi

[display]
desktop = 1920x1080
scaling = fit

[input]
mode = keyboard
cross = enter
