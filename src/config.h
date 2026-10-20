#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

typedef struct {
    wchar_t lastFolder[MAX_PATH];
    int volume;
    int shuffle;
    int repeat;
} AppConfig;

void Config_Load(AppConfig *cfg, const wchar_t *configPath);
void Config_Save(const AppConfig *cfg, const wchar_t *configPath);

#endif // CONFIG_H
