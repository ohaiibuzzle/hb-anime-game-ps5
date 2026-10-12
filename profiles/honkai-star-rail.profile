; Honkai: Star Rail - Download / Update: sophon-dl.exe fetches the game through
; HoYoPlay's Sophon chunk protocol into C:\Games\StarRail of this prefix.
;
; Run it again at any time: it resumes an interrupted download, updates to
; the current version when HoYoverse releases one, and repairs damaged
; files. Add --verify to the arguments to hash every file (slow, about as
; long as reading 116 GB). Voice packs: replace en-us with any of zh-cn,
; ja-jp, ko-kr, or list several; each is 12-16 GB.
;
; Cross (Enter) closes the window once it has finished or failed; the log is
; C:\Games\StarRail\sophon-dl.log (prefixes/honkai-star-rail/drive_c/...).
[application]
id = honkai-star-rail
name = Honkai: Star Rail - Download / Update
executable = C:\Games\sophon-dl\sophon-dl.exe
working_directory = C:\Games\sophon-dl
arguments = hkrpg_global C:\Games\StarRail game en-us
prefix = honkai-star-rail
runtime = wine-wow64
architecture = pe64
graphics = gdi

[display]
desktop = 1920x1080
scaling = fit

[input]
mode = keyboard
cross = enter
