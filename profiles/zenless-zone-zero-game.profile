; Zenless Zone Zero, as downloaded by the Download / Update entry. UNTESTED on
; the console.
;
; Direct3D 11 through DXVK (the recipe installs it into the prefix). The game
; is 64-bit, so it runs natively.
;
; ZenlessZoneZero.exe is started directly. Spritz-Wine starts it through a
; steam.exe stub (a second process), which Wine on the console cannot do; if
; the game refuses to start, that stub is the first suspect.
;
; PW_ENABLE_TIMEOUT_FIX turns on Spritz-Wine's curl timeout fix for the
; game's login requests (wine-patches/0920 in this repo, renamed from
; WINE_ENABLE_TIMEOUT_FIX so a profile may set it). It is a no-op on a
; prospero-win build without that patch.
;
; Do not add PW_ENABLE_DISCONNECT=1 (wine-patches/0922) here: on the PS5 it
; makes the game stop with "cannot run in a virtual machine".
[application]
id = zenless-zone-zero-game
name = Zenless Zone Zero
executable = C:\Games\ZenlessZoneZero\ZenlessZoneZero.exe
working_directory = C:\Games\ZenlessZoneZero
dll_overrides = d3d9,d3d10core,d3d11,dxgi=n
prefix = zenless-zone-zero
runtime = wine-wow64
architecture = pe64
graphics = dxvk

[display]
desktop = 1920x1080

[input]
mode = xinput

[debug]
env = PW_ENABLE_TIMEOUT_FIX=1
