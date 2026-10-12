#include <assert.h>
#include <string.h>
#include "../../src/black_menu_prompt_language.h"

int main(void)
{
    assert(strcmp(black_menu_prompt_localize("press ", "en-US"), "press ") == 0);
    assert(strcmp(black_menu_prompt_localize("press ", "pt-BR"), "PRESSIONE ") == 0);
    assert(strcmp(black_menu_prompt_localize("press ", "es"), "PULSA ") == 0);
    assert(strcmp(black_menu_prompt_localize("press ", "fr"), "APPUYEZ SUR ") == 0);
    assert(strcmp(black_menu_prompt_localize("press ", "de"), "DRÜCKE ") == 0);
    assert(strcmp(black_menu_prompt_localize("press ", "it"), "PREMI ") == 0);
    assert(strcmp(black_menu_prompt_localize(" to start", "pt-BR"), " para iniciar") == 0);
    assert(strcmp(black_menu_prompt_localize(" to save", "pt-BR"), " para salvar") == 0);
    assert(strcmp(black_menu_prompt_localize(" for info", "pt-BR"), " para ver informações") == 0);
    assert(strcmp(black_menu_prompt_localize("press ", "unknown"), "press ") == 0);
    return 0;
}
