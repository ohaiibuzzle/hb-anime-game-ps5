; Genshin Impact, as downloaded by the Download / Update entry. UNTESTED on
; the console.
;
; Direct3D 11 through DXVK (the recipe installs it into the prefix). The game
; is 64-bit, so it runs natively.
;
; GenshinImpact.exe is started directly. Spritz-Wine starts it through a
; steam.exe stub (a second process), which Wine on the console cannot do; if
; the game refuses to start, that stub is the first suspect.
;
; PW_ENABLE_TIMEOUT_FIX turns on Spritz-Wine's curl timeout fix for the
; game's login requests (wine-patches/0920 in this repo, renamed from
; WINE_ENABLE_TIMEOUT_FIX so a profile may set it). It is a no-op on a
; prospero-win build without that patch.
;
; PW_ENABLE_DISCONNECT forces Spritz-Wine's "block the first connection"
; hack (wine-patches/0922), which Spritz only applies to Star Rail, onto this
; game too. If the game cannot log in, delete that line first.
[application]
id = genshin-impact-game
name = Genshin Impact
executable = C:\Games\GenshinImpact\GenshinImpact.exe
working_directory = C:\Games\GenshinImpact
dll_overrides = d3d9,d3d10core,d3d11,dxgi=n
prefix = genshin-impact
runtime = wine-wow64
architecture = pe64
graphics = dxvk

[display]
desktop = 1920x1080

[input]
mode = xinput

[debug]
env = PW_ENABLE_TIMEOUT_FIX=1
env = PW_ENABLE_DISCONNECT=1
