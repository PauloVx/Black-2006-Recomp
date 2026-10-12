#ifndef BLACK_MENU_PROMPT_LANGUAGE_H
#define BLACK_MENU_PROMPT_LANGUAGE_H

#include <string.h>

static inline const char *black_menu_prompt_localize(const char *text, const char *language)
{
    static const struct {
        const char *source;
        const char *pt_br;
        const char *es;
        const char *fr;
        const char *de;
        const char *it;
    } translations[] = {
        { "press ", "PRESSIONE ", "PULSA ", "APPUYEZ SUR ", "DRÜCKE ", "PREMI " },
        { " confirm", " confirmar", " confirmar", " confirmer", " bestätigen", " confermare" },
        { " back", " voltar", " volver", " retour", " zurück", " indietro" },
        { " for info", " para ver informações", " para ver información", " pour voir les infos", " für Infos", " per informazioni" },
        { " to start selected challenge", " para iniciar o desafio selecionado", " para iniciar el desafío seleccionado", " pour lancer le défi sélectionné", " um die ausgewählte Herausforderung zu starten", " per avviare la sfida selezionata" },
        { " to save", " para salvar", " para guardar", " pour sauvegarder", " zum Speichern", " per salvare" },
        { " to start", " para iniciar", " para comenzar", " pour démarrer", " zum Starten", " per iniziare" },
    };
    const char *locale = language ? language : "en-US";
    size_t i;
    int column = !strcmp(locale, "pt-BR") ? 1 : !strcmp(locale, "es") ? 2 :
        !strcmp(locale, "fr") ? 3 : !strcmp(locale, "de") ? 4 : !strcmp(locale, "it") ? 5 : 0;
    if (!text || !column) return text;
    for (i = 0; i < sizeof(translations) / sizeof(translations[0]); ++i)
        if (!strcmp(text, translations[i].source)) {
            const char *translated[] = { translations[i].source, translations[i].pt_br,
                translations[i].es, translations[i].fr, translations[i].de, translations[i].it };
            return translated[column];
        }
    return text;
}

#endif
