# OSU!BAND — сборка и первый запуск

## Нужно

- Windows 10/11 x64.
- Visual Studio 2022: Desktop development with C++.
- MSVC v143 и Windows 10/11 SDK.
- osu!lazer соответствующей поддерживаемой версии.

## Сборка

1. Открой `OSUBAND.sln`.
2. Выбери `Release` + `x64`.
3. Build → Build Solution.
4. Результат: `builds/x64/Release/OSUBAND.Loader.exe` и `OSUBAND.exe`.

## Первый запуск

1. Запусти `OSUBAND.Loader.exe`.
2. Нажми Connect account и подтверди код в браузере через Discord-вход.
3. Вернись в лоадер и обнови состояние аккаунта.
4. Выбери Stable или Beta, если Beta доступна подписке/аккаунту.
5. `Open osu!lazer` и `Launch OSU BAND` находятся рядом на экране Session.
6. По умолчанию меню открывается F4. Клавиша меняется в Settings → Menu key.

Runtime не позволяет запустить второй экземпляр OSU!BAND одновременно.
