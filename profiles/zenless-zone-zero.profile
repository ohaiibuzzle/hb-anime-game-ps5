; Zenless Zone Zero - Download / Update: sophon-dl.exe fetches the game through
; HoYoPlay's Sophon chunk protocol into C:\Games\ZenlessZoneZero of this prefix.
;
; Run it again at any time: it resumes an interrupted download, updates to
; the current version when HoYoverse releases one, and repairs damaged
; files. Add --verify to the arguments to hash every file (slow, about as
; long as reading 67 GB). Voice packs: replace en-us with any of zh-cn,
; ja-jp, ko-kr, or list several; each is 4-5 GB.
; ZZZ also offers 109 numbered optional content packs (19.5 GB in 3.2.0),
; which Collapse Launcher downloads only if the player agrees. They are left
; out; whether the game fetches the ones it needs in-game is unverified.
;
; Cross (Enter) closes the window once it has finished or failed; the log is
; C:\Games\ZenlessZoneZero\sophon-dl.log (prefixes/zenless-zone-zero/drive_c/...).
[application]
id = zenless-zone-zero
name = Zenless Zone Zero - Download / Update
executable = C:\Games\sophon-dl\sophon-dl.exe
working_directory = C:\Games\sophon-dl
arguments = nap_global C:\Games\ZenlessZoneZero game en-us
prefix = zenless-zone-zero
runtime = wine-wow64
architecture = pe64
graphics = gdi

[display]
desktop = 1920x1080
scaling = fit

[input]
mode = keyboard
cross = enter
