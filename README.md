# OSU!BAND

OSU!BAND is a standalone external Windows x64 tool for osu!lazer 2026.804.2.

## What changed

- Product/project/executable renamed to `OSUBAND`.
- osu!stable client code, offsets and stable attach path removed.
- Gameplay modules included in this build are Aim Assist, Relax, Tap Assist, Replay Bot and Autobot.
- New animated Dear ImGui interface with a custom OSU!BAND identity.
- Lazer runtime status, bindings and cloud profile selection kept; Beta does not download profile files.
- Loader project added in the loader edition. It launches the external `OSUBAND.exe`; it does not inject a DLL.

## Controls

- Start osu!lazer.
- Start `OSUBAND.exe`, or use `OSUBAND.Loader.exe` in the loader edition.
- `F4` toggles the menu by default.

## Account and cloud profiles

The account card uses the Discord display name and avatar returned by the OSU!BAND site. Beta profiles are selected on the site or in the client, reviewed by the administrator, and applied between maps. No profile `.cfg` is downloaded by the loader.

## Build

Install Visual Studio 2022 with **Desktop development with C++**, the MSVC v143 toolset, and a Windows 10/11 SDK. Open `OSUBAND.sln`, select `Release | x64`, then choose **Build → Build Solution**.

The loader build places `OSUBAND.exe` and `OSUBAND.Loader.exe` in the same `builds/x64/Release/` directory.

## First run

1. Start `OSUBAND.Loader.exe`.
2. Open the verification URL shown by the loader and sign in to the OSU!BAND site.
3. Enter the short device code from the loader and approve the device.
4. In the loader launch the published Beta release. Select cloud profiles from the client/site.
5. Press `F4` in osu!lazer to open the OSU!BAND menu.

The loader and runtime are separate external applications. No DLL injection is used.
