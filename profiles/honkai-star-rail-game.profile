; Honkai: Star Rail, as downloaded by the Download / Update entry. UNTESTED
; on the console.
;
; Direct3D 11 through DXVK (the recipe installs it into the prefix). The game
; is 64-bit, so it runs natively. Spritz-Wine starts Star Rail directly (it is
; not on its steam.exe stub list), as this entry does.
;
; Spritz-Wine's "block the first connection" hack (wine-patches/0922 in this
; repo) switches itself on for StarRail.exe; Spritz says it fixes launch
; issues and high CPU use. To turn it off, add:  env = PW_DISABLE_DISCONNECT=1
[application]
id = honkai-star-rail-game
name = Honkai: Star Rail
executable = C:\Games\StarRail\StarRail.exe
working_directory = C:\Games\StarRail
dll_overrides = d3d9,d3d10core,d3d11,dxgi=n
prefix = honkai-star-rail
runtime = wine-wow64
architecture = pe64
graphics = dxvk

[display]
desktop = 1920x1080

[input]
mode = xinput
