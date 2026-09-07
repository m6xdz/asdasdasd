#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace band_ui {
struct preferences { bool russian=true; int theme=0; bool animations=true; float motion=1.f; float menu_scale=1.f; };
inline preferences prefs;
inline std::filesystem::path preferences_path(){
#ifdef _WIN32
 wchar_t root[MAX_PATH]{};
 if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,root)))return std::filesystem::path(root)/L"OSUBAND"/L"Beta"/L"ui.ini";
#endif
 return {};
}
inline void load_preferences(){auto path=preferences_path();if(path.empty())return;std::ifstream f(path);int ru=1,theme=0,motion=1;float speed=1,scale=1;
 if(f>>ru>>theme>>motion>>speed){if(!(f>>scale))scale=1; prefs={ru!=0,std::clamp(theme,0,2),motion!=0,std::isfinite(speed)?std::clamp(speed,0.25f,2.f):1.f,std::isfinite(scale)?std::clamp(scale,0.75f,1.35f):1.f};}}
inline void save_preferences(){auto path=preferences_path();if(path.empty())return;std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec)return;
 std::ofstream f(path);f<<prefs.russian<<' '<<prefs.theme<<' '<<prefs.animations<<' '<<prefs.motion<<' '<<prefs.menu_scale;}
inline const char* tr(const char* s){
 if(!prefs.russian)return s;
 static const std::unordered_map<std::string,const char*> words={
 {"Changes apply between maps","Изменения применятся между картами"},
 {"Previous settings remain available with Undo","Предыдущие настройки можно вернуть"},
 {"Checks every 15 seconds","Проверка каждые 15 секунд"},
 {"Cloud sync","Связь с облаком"},
 {"Modules ready","Функции готовы"},
 {"Undo last config","Вернуть прошлый конфиг"},
 {"Applied: ","Применено: "},
 {"Config queued until the map ends.","Конфиг применится после завершения карты."},
 {"Submitted for review.","Конфиг отправлен на проверку."},
 {"Saved to cloud.","Сохранено в облако."},
 {"Disconnect this loader","Выйти из аккаунта"},
 {"Open osu!lazer","Открыть osu!lazer"},
 {"MODULES PAUSED","ФУНКЦИИ НА ПАУЗЕ"},
 {"Settings applied. F8 pauses every module.","Настройки применены. F8 — пауза функций."},
 {"WAITING FOR OSU!LAZER","ОЖИДАНИЕ OSU!LAZER"},
 {"LAZER / CONNECTED","OSU!LAZER ПОДКЛЮЧЁН"},
 {"LAZER / BEATMAP READY","КАРТА ЗАГРУЖЕНА"},
 {"REPLAY VIEW / MODULES SUSPENDED","ПРОСМОТР ПОВТОРА · ФУНКЦИИ НА ПАУЗЕ"},
 {"Ready when you are.","Готов к подключению."},
 {"Account checked. Ready to launch.","Аккаунт подключён. Можно запускать."},
 {"This loader has disconnected.","Аккаунт отключён."},
 {"OSU!BAND started.","OSU BAND запущен."},
 {"Aim spread","Разброс курсора"},
 {"Assist window","Окно коррекции"},
 {"Browse","Выбрать файл"},
 {"Cursor only","Только курсор"},
 {"Curve strength","Кривизна траектории"},
 {"Drift amount","Дрейф курсора"},
 {"Enabling one automatically turns the other off.","Включение одной отключает другую."},
 {"Full playback","Курсор и клавиши"},
 {"INPUT","КЛАВИШИ"},
 {"Keys only","Только клавиши"},
 {"Load replay","Загрузить повтор"},
 {"Momentum","Инерция"},
 {"No replay loaded","Повтор не выбран"},
 {"No beatmap selected","Карта не выбрана"},
 {"Playback mode","Режим повтора"},
 {"REPLAY FILE","ФАЙЛ ПОВТОРА"},
 {"Relax and Tap Assist use separate input paths.","Relax и Tap Assist работают по отдельности."},
 {"Replay must match the selected beatmap.","Повтор должен соответствовать карте."},
 {"Slider follow","Следование слайдеру"},
 {"Spinner speed","Скорость спиннера"},
 {"Timing variation","Разброс тайминга"},
 {"Works with your physical taps.","Корректирует твои нажатия."},
 {"Connect your OSU!BAND account","Подключи аккаунт OSU BAND"},
 {"Your next session","Твой аккаунт"},
 {"Not connected","Не подключён"},
 {"Waiting for osu!lazer","Ожидание osu!lazer"},
 {"Play","Игра"},{"Profiles","Конфиги"},{"Config","Конфиг"},{"System","Настройки"},{"Session","Запуск"},{"Settings","Настройки"},{"Modules","Модули"},{"Module settings","НАСТРОЙКИ МОДУЛЯ"},
 {"My account","Мой аккаунт"},{"Open account","Открыть аккаунт"},{"Pause modules  F8","Пауза функций · F8"},{"Resume modules","Продолжить"},
 {"Aim Assist","Aim Assist"},{"Relax","Relax"},{"Tap Assist","Tap Assist"},{"Replay","Replay"},{"Autobot","Autobot"},
 {"Cursor correction","Мягкая коррекция курсора"},{"Automatic key timing","Автоматический тайминг нажатий"},{"Physical tap adjustment","Коррекция физических нажатий"},
 {"Your .osr playback","Воспроизведение .osr"},{"Full path automation","Автоматизация курсора и нажатий"},{"Enable module","Включить модуль"},
 {"Horizontal strength","Коррекция по X"},{"Vertical strength","Коррекция по Y"},{"Smoothing","Плавность движения"},{"Approach window","Время захвата"},
 {"Distance falloff","Ослабление по дистанции"},{"Freeze smoothing","Плавность остановки"},{"Tablet mode","Режим планшета"},{"Ignore sliders","Не трогать слайдеры"},
 {"Limit correction","Ограничение коррекции"},{"Timing variation / UR","Разброс тайминга / UR"},{"Manual timing offset","Смещение нажатий"},
 {"Prefer single tap","Предпочитать одну клавишу"},{"Single tap BPM ceiling","Порог чередования BPM"},{"K1 hold","Удержание K1"},{"K2 hold","Удержание K2"},
 {"K1 spread","Разброс K1"},{"K2 spread","Разброс K2"},{"Refresh","Обновить"},{"Apply","Применить"},{"Undo","Отменить"},
 {"Cloud configs","Облачные конфиги"},{"From modeof19","От modeof19"},{"From players","От игроков"},{"Mine","Мои"},{"On review","На проверке"},
 {"Published","Опубликован"},{"Rejected","Отклонён"},{"Private","Личный"},{"Save to cloud","Сохранить в облако"},{"Submit for review","Отправить на проверку"},
 {"Name","Название"},{"Description","Описание"},{"Your settings","Твои настройки"},{"Publish your config","Опубликовать конфиг"},{"Loading...","Загрузка…"},
 {"No configs yet","Здесь пока нет конфигов"},{"Select a config","Выбери конфиг"},{"Installed","Установлен"},{"Install","Установить"},{"Remove","Удалить у себя"},{"Save private","Сохранить личным"},{"Publish your config","Опубликовать конфиг"},{"Save private keeps it only in your account.","Личный конфиг виден только тебе."},{"Submit for review asks an admin to publish it","Проверка нужна, чтобы администратор опубликовал конфиг"},{"for everyone using the Beta build.","для всех пользователей Beta."},{"Cloud only · changes apply between maps","Только облако · применяется между картами"},{"UNINJECT","ВЫГРУЗИТЬ"},{"Language","Язык"},{"Theme","Тема"},{"Animations","Анимации"},{"Animation speed","Скорость анимаций"},
 {"Rose","Розовая"},{"Ocean","Синяя"},{"Light","Светлая"},{"Gameplay keys","Клавиши игры"},{"Exclude menu from capture","Скрывать меню при захвате"},

 {"Watermark","Водяной знак"},{"Menu key","Клавиша меню"},{"Press a key...","Нажми клавишу…"},{"Menu scale","Масштаб меню"},
 {"Beta channel","Канал Beta"},{"Beta access active","Доступ к Beta активен"},{"Beta access unavailable","Доступ к Beta недоступен"},{"Watermark shows version, local time and account name.","Водяной знак показывает версию, локальное время и имя аккаунта."},
 {"OSU!BAND configs","Конфиги OSU!BAND"},{"Player","Игрок"},{"Administrator","Администратор"},{"Stable","Stable"},{"Beta","Beta"},
  {"Session HUD","Статус поверх игры"},{"Enable Beta","Инструменты Beta"},{"All set.","Всё готово."},{"Find your rhythm.","Можно играть."},
 {"Ready when","Начнём"},{"you are.","с аккаунта."},{"Connect your account","Подключи аккаунт"},{"Confirm this loader in your browser.","Подтверди вход в браузере."},
 {"Connect account","Подключить аккаунт"},{"Connecting...","Подключение…"},{"Open website","Открыть сайт"},{"Launch OSU!BAND","Запустить OSU BAND"},
 {"Preparing your session...","Подготовка…"},{"Open account to activate a key","Активировать ключ на сайте"},{"Close loader after launch","Закрыть лоадер после запуска"},
 {"Your password stays in the browser.","Пароль остаётся в браузере."},{"One account. One place to start.","Вход через Discord."},
 {"Subscription and configs follow your account.","Подписка привязана к аккаунту."},{"YOUR CONNECTION CODE","КОД ПОДКЛЮЧЕНИЯ"},
 {"Waiting for confirmation on the website...","Ожидание подтверждения на сайте…"},{"SUBSCRIPTION ACTIVE","ПОДПИСКА АКТИВНА"},{"SUBSCRIPTION REQUIRED","НУЖНА ПОДПИСКА"},
 {"osu!lazer is running","osu!lazer запущен"},{"Open osu!lazer before starting","Сначала открой osu!lazer"},{"Disconnect","Выйти из аккаунта"},
 {"F4  /  Show or hide menu","F4 · Показать или скрыть меню"},{"F8  /  Pause all modules","F8 · Пауза всех функций"}
 };
 auto it=words.find(s);return it==words.end()?s:it->second;
}
}
